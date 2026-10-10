#include "dibujo.h"

#include <QHash>
#include <QObject>
#include <QSvgRenderer>
#include <QTransform>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <functional>

namespace mcusim {

namespace {

// El id de un elemento, o vacío
QString id_de(const QXmlStreamReader& r)
{
    return r.attributes().value(QLatin1String("id")).toString();
}

// El destino de un enlace: `href` o `xlink:href`, el que traiga
QString href_de(const QXmlStreamReader& r)
{
    for (const QXmlStreamAttribute& a : r.attributes())
        if (a.name() == QLatin1String("href")) return a.value().toString().trimmed();
    return QString();
}

// Copia el documento token a token, y SALTA el subárbol de cada elemento para
// el que `quita` diga que sí. Es como se limpia un dibujo y como se le quitan
// los elementos vivos para hacer el fondo: Qt SVG no deja tocar su árbol, así
// que se reescribe el texto. false, con `error`, si el XML está roto.
bool filtra(const QByteArray& entrada, QByteArray& salida, QString& error,
            const std::function<bool(const QXmlStreamReader&)>& quita)
{
    QXmlStreamReader r(entrada);
    salida.clear();
    QXmlStreamWriter w(&salida);
    int saltando = 0;                // profundidad dentro de lo que se quita
    while (!r.atEnd()) {
        r.readNext();
        if (r.hasError()) break;
        if (saltando > 0) {
            if (r.isStartElement()) ++saltando;
            else if (r.isEndElement()) --saltando;
            continue;
        }
        if (r.isStartElement() && quita(r)) {
            saltando = 1;
            continue;
        }
        // Los elementos, a mano, con los nombres tal como venían:
        // `writeCurrentToken` se inventa prefijos para el espacio de nombres
        // por omisión (`n1:svg`), y a la segunda pasada los declara dos
        // veces, y eso ya no es XML
        if (r.isStartElement()) {
            w.writeStartElement(r.qualifiedName().toString());
            for (const QXmlStreamNamespaceDeclaration& n : r.namespaceDeclarations())
                w.writeAttribute(n.prefix().isEmpty()
                                     ? QStringLiteral("xmlns")
                                     : QStringLiteral("xmlns:") + n.prefix().toString(),
                                 n.namespaceUri().toString());
            for (const QXmlStreamAttribute& a : r.attributes())
                w.writeAttribute(a.qualifiedName().toString(), a.value().toString());
        } else if (r.isEndElement()) {
            w.writeEndElement();
        } else {
            w.writeCurrentToken(r);
        }
    }
    if (r.hasError()) {
        error = QObject::tr("no es XML: %1 (linea %2)").arg(r.errorString()).arg(r.lineNumber());
        return false;
    }
    return true;
}

} // namespace

// -----------------------------------------------------------------------------
QString InformeDibujo::resumen() const
{
    const int total = int(enlaces.size() + sin_elemento.size());
    QString t = QObject::tr("%1 de %2 piezas en el dibujo").arg(enlaces.size()).arg(total);
    if (!sin_elemento.isEmpty()) {
        QStringList a = sin_elemento.mid(0, 6);
        if (sin_elemento.size() > 6) a << QStringLiteral("…");
        t += QObject::tr("; sin dibujar: %1").arg(a.join(QStringLiteral(", ")));
    }
    if (!ocultas.isEmpty()) {
        QStringList a = ocultas.mid(0, 6);
        if (ocultas.size() > 6) a << QStringLiteral("…");
        t += QObject::tr("; ocultas: %1").arg(a.join(QStringLiteral(", ")));
    }
    const int problemas = int(tabla_rota.size() + repetidos.size() + avisos.size());
    if (problemas > 0) t += QObject::tr("; %n aviso(s)", nullptr, problemas);
    return t;
}

QStringList InformeDibujo::detalle() const
{
    QStringList d;
    if (!sin_elemento.isEmpty())
        d << QObject::tr("piezas sin elemento en el dibujo: %1")
                 .arg(sin_elemento.join(QStringLiteral(", ")));
    if (!ocultas.isEmpty())
        d << QObject::tr("piezas ocultas (visible=\"no\"), que no van a la bandeja: %1")
                 .arg(ocultas.join(QStringLiteral(", ")));
    for (const QString& s : tabla_rota) d << QObject::tr("tabla de enlaces: %1").arg(s);
    if (!repetidos.isEmpty())
        d << QObject::tr("ids repetidos en el dibujo (se usa el primero): %1")
                 .arg(repetidos.join(QStringLiteral(", ")));
    d << avisos;
    return d;
}

// -----------------------------------------------------------------------------
DibujoPlaca::DibujoPlaca() = default;
DibujoPlaca::~DibujoPlaca() = default;

bool DibujoPlaca::carga(const QByteArray& svg, QString& error)
{
    svg_.clear();
    ancho_.clear();
    alto_.clear();
    ids_.clear();
    repetidos_.clear();
    avisos_.clear();
    rend_.reset();
    if (svg.size() > TAMANO_MAX) {
        error = QObject::tr("el dibujo pesa %1 MiB, y el maximo es %2")
                    .arg(double(svg.size()) / (1024.0 * 1024.0), 0, 'f', 1)
                    .arg(TAMANO_MAX / (1024 * 1024));
        return false;
    }

    // UNA PASADA para limpiar, mirando a la vez qué hay. Lo que se quita:
    //
    //   * una <image> que no lleve el dibujo DENTRO (`data:`): con una ruta o
    //     un `file:` leería un fichero de esta máquina, y el dibujo puede venir
    //     de otra;
    //   * un <use> que apunte a otro fichero: lo mismo;
    //   * un <script> o un <foreignObject>: Qt SVG no los ejecuta ni los
    //     pinta, pero no tienen nada que hacer en un dibujo de placa.
    bool es_svg = false, raiz = true;
    QHash<QString, int> vistos;
    QStringList quitados;
    QByteArray limpio;
    auto quita = [&](const QXmlStreamReader& r) {
        const QStringView n = r.name();
        if (raiz) {
            raiz = false;
            es_svg = n == QLatin1String("svg");
            ancho_ = r.attributes().value(QLatin1String("width")).toString().trimmed();
            alto_ = r.attributes().value(QLatin1String("height")).toString().trimmed();
        }
        bool fuera = false;
        if (n == QLatin1String("image")) {
            const QString h = href_de(r);
            fuera = !h.startsWith(QLatin1String("data:"), Qt::CaseInsensitive);
            if (fuera) quitados << QObject::tr("una <image> que apunta a \"%1\"").arg(h.left(60));
        } else if (n == QLatin1String("use")) {
            const QString h = href_de(r);
            fuera = !h.isEmpty() && !h.startsWith(QLatin1Char('#'));
            if (fuera) quitados << QObject::tr("un <use> que apunta a \"%1\"").arg(h.left(60));
        } else if (n == QLatin1String("script") || n == QLatin1String("foreignObject")) {
            fuera = true;
            quitados << QObject::tr("un <%1>").arg(n.toString());
        }
        if (!fuera) {
            const QString id = id_de(r);
            if (!id.isEmpty()) {
                const int n = ++vistos[id];
                if (n == 1) ids_ << id;
                else if (n == 2) repetidos_ << id;
            }
        }
        return fuera;
    };
    if (!filtra(svg, limpio, error, quita)) {
        ids_.clear();
        repetidos_.clear();
        return false;
    }
    if (!es_svg) {
        ids_.clear();
        repetidos_.clear();
        error = QObject::tr("no es un SVG: la raiz no es <svg>");
        return false;
    }
    for (const QString& q : quitados)
        avisos_ << QObject::tr("se ha quitado %1: un dibujo solo puede llevar lo suyo").arg(q);

    auto r = std::make_unique<QSvgRenderer>();
    if (!r->load(limpio) || !r->isValid()) {
        ids_.clear();
        repetidos_.clear();
        avisos_.clear();
        error = QObject::tr("Qt SVG no lo puede pintar");
        return false;
    }
    svg_ = limpio;
    rend_ = std::move(r);
    return true;
}

bool DibujoPlaca::existe(const QString& id) const
{
    return rend_ && !id.isEmpty() && rend_->elementExists(id);
}

QRectF DibujoPlaca::caja(const QString& id) const
{
    if (!existe(id)) return QRectF();
    return rend_->transformForElement(id).mapRect(rend_->boundsOnElement(id));
}

bool DibujoPlaca::girado(const QString& id) const
{
    if (!existe(id)) return false;
    // Un cuarto de vuelta, o media, no cuenta: la caja sigue siendo exacta. Es
    // lo que hace `mcu-sim` con una placa montada con `giro=` (plan §31)
    const QTransform t = rend_->transformForElement(id);
    const bool recto = qFuzzyIsNull(t.m12()) && qFuzzyIsNull(t.m21());
    const bool cuarto = qFuzzyIsNull(t.m11()) && qFuzzyIsNull(t.m22());
    return !recto && !cuarto;
}

namespace {
// "84mm" -> 84; "8.4cm" -> 84; "2in" -> 50,8; sin unidad, px o % -> 0
double en_mm(const QString& t)
{
    static const struct { const char* u; double mm; } unidades[] = {
        {"mm", 1.0}, {"cm", 10.0}, {"in", 25.4}, {"pt", 25.4 / 72.0}, {"pc", 25.4 / 6.0}};
    for (const auto& u : unidades)
        if (t.endsWith(QLatin1String(u.u))) {
            bool ok = false;
            const double v = t.chopped(2).trimmed().toDouble(&ok);
            return ok && v > 0 ? v * u.mm : 0.0;
        }
    return 0.0;
}
} // namespace

double DibujoPlaca::mm_por_unidad() const
{
    const QRectF l = lienzo();
    const double w = en_mm(ancho_), h = en_mm(alto_);
    if (w > 0 && l.width() > 0) return w / l.width();
    if (h > 0 && l.height() > 0) return h / l.height();
    return 0.0;
}

QRectF DibujoPlaca::lienzo() const
{
    return rend_ ? rend_->viewBoxF() : QRectF();
}

QByteArray DibujoPlaca::sin(const QSet<QString>& quitar) const
{
    if (quitar.isEmpty()) return svg_;
    QByteArray fondo;
    QString e;
    filtra(svg_, fondo, e, [&](const QXmlStreamReader& r) {
        return quitar.contains(id_de(r));
    });
    return fondo;
}

InformeDibujo DibujoPlaca::enlaza(const PlacaGui& placa, const QString& placa_id,
                                  const QVector<EnlaceTabla>& tabla) const
{
    InformeDibujo inf;
    inf.repetidos = repetidos_;
    inf.avisos = avisos_;

    // Las piezas de ESTA placa, por su nombre dentro de ella
    QHash<QString, int> suyas;           // id_local -> idx
    QVector<int> orden;
    for (const PiezaGui& pz : placa.piezas) {
        if (!placa_id.isEmpty() && pz.placa != placa_id) continue;
        suyas.insert(pz.id_local, pz.idx);
        orden.push_back(pz.idx);
    }

    // 1. La tabla, que manda. Una entrada rota se dice, y la pieza se busca
    //    después por su id, como si la entrada no estuviera.
    QHash<int, EnlaceDibujo> hecho;
    for (const EnlaceTabla& t : tabla) {
        const auto it = suyas.constFind(t.pieza);
        if (it == suyas.constEnd()) {
            inf.tabla_rota << QObject::tr("%1 → %2: la placa no tiene esa pieza")
                                  .arg(t.pieza, t.elemento);
            continue;
        }
        if (hecho.contains(*it)) {
            inf.tabla_rota << QObject::tr("%1 → %2: la pieza ya estaba en la tabla")
                                  .arg(t.pieza, t.elemento);
            continue;
        }
        if (!existe(t.elemento)) {
            inf.tabla_rota << QObject::tr("%1 → %2: el dibujo no tiene ese elemento")
                                  .arg(t.pieza, t.elemento);
            continue;
        }
        hecho.insert(*it, {*it, t.elemento, QStringLiteral("tabla"), t.efecto,
                           caja(t.elemento)});
    }

    // 2. El id, con el nombre de la pieza en su placa
    for (int idx : orden) {
        const PiezaGui& pz = placa.piezas[idx];
        if (!hecho.contains(idx) && existe(pz.id_local))
            hecho.insert(idx, {idx, pz.id_local, QStringLiteral("id"), QString(),
                               caja(pz.id_local)});
        if (hecho.contains(idx)) {
            inf.enlaces.push_back(hecho.value(idx));
            if (girado(hecho.value(idx).elemento))
                inf.avisos << QObject::tr("%1 está girado: sus efectos van a la caja que "
                                          "lo contiene, sin girar")
                                  .arg(pz.id_local);
        } else if (!pz.visible) {
            inf.ocultas << pz.id_local;      // plan §41: no se echa de menos
        } else {
            inf.sin_elemento << pz.id_local;
        }
    }
    return inf;
}

} // namespace mcusim
