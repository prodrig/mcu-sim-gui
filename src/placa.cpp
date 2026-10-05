#include "placa.h"

#include <QHash>
#include <QXmlStreamReader>

namespace mcusim {

int PlacaGui::n_observables() const
{
    int n = 0;
    for (const PiezaGui& p : piezas) n += p.observables.size();
    return n;
}

int PlacaGui::n_interesantes() const
{
    int n = 0;
    for (const PiezaGui& p : piezas)
        for (const ObservableGui& o : p.observables) n += o.interesante ? 1 : 0;
    return n;
}

int PlacaGui::n_mandos() const
{
    int n = 0;
    for (const PiezaGui& p : piezas) n += p.mandos.size();
    return n;
}

namespace {

int entero(const QXmlStreamAttributes& a, const char* n, bool& ok)
{
    bool b = false;
    const int v = a.value(QLatin1String(n)).toInt(&b);
    if (!b) ok = false;
    return v;
}

double real(const QXmlStreamAttributes& a, const char* n)
{
    return a.value(QLatin1String(n)).toDouble();
}

QString texto(const QXmlStreamAttributes& a, const char* n)
{
    return a.value(QLatin1String(n)).toString();
}

} // namespace

bool lee_catalogo(const QByteArray& xml, QVector<PiezaGui>& piezas, QString& error)
{
    piezas.clear();
    QXmlStreamReader r(xml);
    bool raiz = false, ok = true;
    while (!r.atEnd()) {
        if (r.readNext() != QXmlStreamReader::StartElement) continue;
        const QXmlStreamAttributes a = r.attributes();
        if (r.name() == QLatin1String("catalogo")) {
            raiz = true;
        } else if (r.name() == QLatin1String("pieza")) {
            PiezaGui p;
            p.idx  = entero(a, "idx", ok);
            p.id   = texto(a, "id");
            p.id_local = p.id;
            p.tipo = texto(a, "tipo");
            if (p.idx != piezas.size()) {
                error = QStringLiteral("la pieza %1 trae idx=%2, y le toca el %3")
                            .arg(p.id).arg(p.idx).arg(piezas.size());
                return false;
            }
            piezas.push_back(p);
        } else if (r.name() == QLatin1String("observable") && !piezas.isEmpty()) {
            ObservableGui o;
            o.idx         = entero(a, "idx", ok);
            o.id_obs      = entero(a, "id_obs", ok);
            o.nombre      = texto(a, "nombre");
            o.unidad      = texto(a, "unidad");
            o.min         = real(a, "min");
            o.max         = real(a, "max");
            o.interesante = a.value(QLatin1String("interesante")) == QLatin1String("si");
            o.alarma      = a.value(QLatin1String("alarma")) == QLatin1String("si");
            piezas.back().observables.push_back(o);
        } else if (r.name() == QLatin1String("mando") && !piezas.isEmpty()) {
            MandoGui m;
            m.idx    = entero(a, "idx", ok);
            m.nombre = texto(a, "nombre");
            m.tipo   = texto(a, "tipo");
            m.min    = real(a, "min");
            m.max    = real(a, "max");
            m.valor  = a.hasAttribute(QLatin1String("valor")) ? real(a, "valor") : m.min;
            piezas.back().mandos.push_back(m);
        }
    }
    if (r.hasError()) {
        error = QStringLiteral("el catalogo no es XML: %1").arg(r.errorString());
        return false;
    }
    if (!raiz) {
        error = QStringLiteral("el XML no es un <catalogo>");
        return false;
    }
    if (!ok) {
        error = QStringLiteral("el catalogo trae un indice que no es un numero");
        return false;
    }
    return true;
}

bool junta_placa(const QByteArray& xml, const QVector<PiezaGui>& catalogo,
                 PlacaGui& placa, QString& error)
{
    placa = PlacaGui();
    placa.piezas = catalogo;
    QHash<QString, int> por_id;
    for (int i = 0; i < placa.piezas.size(); ++i) por_id.insert(placa.piezas[i].id, i);

    QXmlStreamReader r(xml);
    bool raiz = false;
    int  actual = -1;                       // la pieza del <componente> abierto
    while (!r.atEnd()) {
        const QXmlStreamReader::TokenType t = r.readNext();
        if (t == QXmlStreamReader::EndElement && r.name() == QLatin1String("componente")) {
            actual = -1;
            continue;
        }
        if (t != QXmlStreamReader::StartElement) continue;
        const QXmlStreamAttributes a = r.attributes();
        if (!raiz && (r.name() == QLatin1String("placa") ||
                      r.name() == QLatin1String("sistema"))) {
            raiz = true;
            placa.nombre = texto(a, "nombre");
        } else if (r.name() == QLatin1String("placa")) {
            // Una placa DENTRO de un <sistema>: solo su id y su nombre
            placa.placas.push_back({texto(a, "id"), texto(a, "nombre"), texto(a, "fichero")});
        } else if (r.name() == QLatin1String("acopla")) {
            placa.acoples.push_back({texto(a, "a"), texto(a, "b"),
                                     a.value(QLatin1String("espejo")) == QLatin1String("si")});
        } else if (r.name() == QLatin1String("hilo")) {
            ++placa.n_hilos;
        } else if (r.name() == QLatin1String("mcu")) {
            const QString id = texto(a, "id"), tipo = texto(a, "tipo");
            placa.mcus << (id.isEmpty() ? tipo : QStringLiteral("%1 (%2)").arg(id, tipo));
        } else if (r.name() == QLatin1String("componente")) {
            const QString id = texto(a, "id");
            actual = por_id.value(id, -1);
            if (actual < 0) {
                placa.avisos << QStringLiteral("%1 (%2) esta en la placa y no en el catalogo")
                                    .arg(id, texto(a, "tipo"));
                continue;
            }
            PiezaGui& p = placa.piezas[actual];
            p.en_placa  = true;
            p.conectada = a.value(QLatin1String("conectada")) != QLatin1String("no");
        } else if (r.name() == QLatin1String("pin") && actual >= 0) {
            placa.piezas[actual].patillas.push_back({texto(a, "nombre"), texto(a, "nodo")});
        }
    }
    if (r.hasError()) {
        error = QStringLiteral("la placa no es XML: %1").arg(r.errorString());
        return false;
    }
    if (!raiz) {
        error = QStringLiteral("el XML no es una <placa> ni un <sistema>");
        return false;
    }
    // La placa de cada pieza, por el prefijo de su id. Solo cuenta si es una
    // de las placas declaradas: fuera de un sistema, un id con barra no dice
    // nada (no debería haberlo).
    for (PiezaGui& p : placa.piezas) {
        p.placa.clear();
        p.id_local = p.id;
        const int b = p.id.indexOf(QLatin1Char('/'));
        if (b <= 0) continue;
        const QString pl = p.id.left(b);
        for (const SubPlacaGui& s : placa.placas)
            if (s.id == pl) {
                p.placa    = pl;
                p.id_local = p.id.mid(b + 1);
            }
    }
    return true;
}

} // namespace mcusim
