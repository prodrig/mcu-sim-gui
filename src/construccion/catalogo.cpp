#include "catalogo.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace mcusim::build {

namespace {

QString lee(const QString& ruta)
{
    QFile f(ruta);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

// El tamaño de un SVG en mm, de su width/height
QSizeF tam_svg(const QString& svg)
{
    static const QRegularExpression w(QStringLiteral("<svg[^>]*\\swidth=\"([\\d.]+)mm\""));
    static const QRegularExpression h(QStringLiteral("<svg[^>]*\\sheight=\"([\\d.]+)mm\""));
    return QSizeF(w.match(svg).captured(1).toDouble(), h.match(svg).captured(1).toDouble());
}

// Las patillas de un MCU en su dibujo: el centro de cada `$.N`, con el lado
// por el que sale. Se leen de los rect y circle del SVG de B3
// (El simple no lleva ids, pero sus patillas están donde las del detallado
// dentro de <g id="$">: se leen del detallado y se mueven con el simple)
QVector<Ancla> anclas_mcu(const QString& con_ids, const QString& svg)
{
    QVector<Ancla> out;
    static const QRegularExpression tr(QStringLiteral("<g id=\"\\$\" transform=\"translate\\(([-\\d.]+),([-\\d.]+)\\)"));
    const auto mt = tr.match(svg);
    const double dx = mt.hasMatch() ? mt.captured(1).toDouble() : 0;
    const double dy = mt.hasMatch() ? mt.captured(2).toDouble() : 0;
    static const QRegularExpression vb(QStringLiteral("viewBox=\"0 0 ([\\d.]+) ([\\d.]+)\""));
    const auto mv = vb.match(svg);
    const double W = mv.captured(1).toDouble(), H = mv.captured(2).toDouble();
    static const QRegularExpression r(QStringLiteral(
        "<rect id=\"\\$\\.([A-Za-z0-9]+)\" x=\"([-\\d.]+)\" y=\"([-\\d.]+)\" width=\"([\\d.]+)\" height=\"([\\d.]+)\""));
    auto it = r.globalMatch(con_ids);
    while (it.hasNext()) {
        const auto m = it.next();
        const double x = m.captured(2).toDouble() + dx, y = m.captured(3).toDouble() + dy;
        const double w = m.captured(4).toDouble(), h = m.captured(5).toDouble();
        const QPointF c(x + w / 2, y + h / 2);
        // Sale por el lado del dibujo que tiene más cerca
        const double dl = c.x(), dr = W - c.x(), dt = c.y(), db = H - c.y();
        const double mn = std::min({dl, dr, dt, db});
        const QPointF dir = mn == dl ? QPointF(-1, 0) : mn == dr ? QPointF(1, 0)
                          : mn == dt ? QPointF(0, -1) : QPointF(0, 1);
        out.push_back({m.captured(1), c, dir});
    }
    static const QRegularExpression ci(QStringLiteral(
        "<circle id=\"\\$\\.([A-Za-z0-9]+)\" cx=\"([-\\d.]+)\" cy=\"([-\\d.]+)\""));
    auto ic = ci.globalMatch(con_ids);
    while (ic.hasNext()) {
        const auto m = ic.next();
        out.push_back({m.captured(1), QPointF(m.captured(2).toDouble() + dx, m.captured(3).toDouble() + dy),
                       QPointF(0, 1)});
    }
    return out;
}

} // namespace

const Ancla* Dibujo::ancla(const QString& t) const
{
    for (const Ancla& a : anclas)
        if (a.terminal == t) return &a;
    return nullptr;
}

const FichaPieza* Catalogo::pieza(const QString& tipo) const
{
    auto it = piezas_.constFind(tipo);
    if (it != piezas_.constEnd()) return it->valida() ? &*it : nullptr;
    const Entrada* e = bib_ ? bib_->busca(Entrada::Pieza, tipo) : nullptr;
    FichaPieza f = e ? lee_pieza(e->ruta) : FichaPieza();
    it = piezas_.insert(tipo, f);
    return it->valida() ? &*it : nullptr;
}

const FichaMcu* Catalogo::mcu(const QString& tipo) const
{
    auto it = mcus_.constFind(tipo);
    if (it != mcus_.constEnd()) return it->valida() ? &*it : nullptr;
    const Entrada* e = bib_ ? bib_->busca(Entrada::Mcu, tipo) : nullptr;
    FichaMcu f = e ? lee_mcu(e->ruta) : FichaMcu();
    it = mcus_.insert(tipo, f);
    return it->valida() ? &*it : nullptr;
}

const Perfil* Catalogo::perfil(const QString& nombre) const
{
    if (nombre.isEmpty()) return nullptr;
    auto it = perfiles_.constFind(nombre);
    if (it != perfiles_.constEnd()) return it->valido() ? &*it : nullptr;
    const Entrada* e = bib_ ? bib_->busca(Entrada::Perfil, nombre) : nullptr;
    Perfil p = e ? lee_perfil(e->ruta) : Perfil();
    it = perfiles_.insert(nombre, p);
    return it->valido() ? &*it : nullptr;
}

Dibujo Catalogo::dibujo(const Instancia& i) const
{
    Dibujo d;
    if (i.clase == Instancia::Mcu) {
        const FichaMcu* m = mcu(i.tipo);
        if (!m) return d;
        const QString ruta = i.detalle ? m->svg_detalle : m->svg_simple;
        auto it = svgs_.find(ruta);
        if (it == svgs_.end()) it = svgs_.insert(ruta, lee(ruta));
        d.svg = *it;
        d.tam = tam_svg(d.svg);
        auto ij = svgs_.find(m->svg_detalle);
        if (ij == svgs_.end()) ij = svgs_.insert(m->svg_detalle, lee(m->svg_detalle));
        d.anclas = anclas_mcu(*ij, d.svg);
        return d;
    }
    const FichaPieza* f = pieza(i.tipo);
    const Entrada* e = bib_ ? bib_->busca(Entrada::Pieza, i.tipo) : nullptr;
    if (!f || !e) return d;
    auto it = formas_.find(i.tipo);
    if (it == formas_.end()) {
        const QString ruta = QFileInfo(e->ruta).dir().filePath(i.tipo + QStringLiteral(".forma.xml"));
        it = formas_.insert(i.tipo, lee_forma(ruta));
    }
    Forma fo = *it;
    if (!fo.generador.isEmpty()) fo = genera_forma(fo.generador, *f, i.atributos, i.n_serie);
    d.svg = fo.svg;
    d.tam = fo.tam;
    d.anclas = fo.anclas;
    return d;
}

QStringList Catalogo::terminales(const Instancia& i) const
{
    if (i.clase == Instancia::Mcu) {
        QStringList out;
        if (const FichaMcu* m = mcu(i.tipo))
            for (const Patilla& p : m->patillas) out << p.n;
        return out;
    }
    const FichaPieza* f = pieza(i.tipo);
    return f ? terminales_de(*f, i.atributos, i.n_serie) : QStringList();
}

QString Catalogo::nodo_mcu(const Instancia& i, const QString& n) const
{
    const FichaMcu* m = mcu(i.tipo);
    const Patilla* p = m ? m->patilla(n) : nullptr;
    return p ? p->nodo : QString();
}

QString Catalogo::posicion_mcu(const Instancia& i, const QString& nodo) const
{
    if (const FichaMcu* m = mcu(i.tipo))
        for (const Patilla& p : m->patillas)
            if (p.nodo == nodo) return p.n;
    return {};
}

QString Catalogo::rotulo(const Instancia& i, const QString& t) const
{
    if (i.clase == Instancia::Mcu) {
        const FichaMcu* m = mcu(i.tipo);
        const Patilla* p = m ? m->patilla(t) : nullptr;
        return p ? QStringLiteral("%1 %2").arg(t, p->nombre) : t;
    }
    if (const Perfil* p = perfil(i.perfil)) {
        bool ok = false;
        const int k = t.toInt(&ok);
        if (ok && k >= 1 && k <= p->pines.size())
            return QStringLiteral("%1 %2").arg(t, p->pines[k - 1].uso);
    }
    return t;
}

Instancia Catalogo::nueva(const Entrada& e) const
{
    Instancia i;
    i.fuente = e.fuente;
    switch (e.clase) {
    case Entrada::Mcu:
        i.clase = Instancia::Mcu;
        i.tipo = e.nombre;
        break;
    case Entrada::Perfil: {
        i.tipo = QStringLiteral("Conector");
        i.perfil = e.nombre;
        if (const Perfil* p = perfil(e.nombre)) {
            i.atributos.insert(QStringLiteral("filas"), QString::number(p->filas));
            i.atributos.insert(QStringLiteral("columnas"), QString::number(p->columnas));
        }
        break;
    }
    default:
        i.tipo = e.nombre;
        if (i.tipo == QLatin1String("Conector") || i.tipo == QLatin1String("Jumper")) {
            i.atributos.insert(QStringLiteral("filas"), QStringLiteral("1"));
            i.atributos.insert(QStringLiteral("columnas"), QStringLiteral("2"));
        }
        if (i.tipo == QLatin1String("I2cWire")) i.n_serie = 2;
        break;
    }
    return i;
}

void Catalogo::olvida()
{
    piezas_.clear();
    formas_.clear();
    mcus_.clear();
    perfiles_.clear();
    svgs_.clear();
}

} // namespace mcusim::build
