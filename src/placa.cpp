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

const SubPlacaGui* PlacaGui::subplaca(const QString& id) const
{
    for (const SubPlacaGui& s : placas) if (s.id == id) return &s;
    return nullptr;
}

QVector<EnlaceGui> PlacaGui::enlaces() const
{
    QVector<EnlaceGui> v;
    for (const AcopleGui& a : acoples)
        for (int k = 1; k < a.conectores.size() && k < a.placas.size(); ++k)
            if (a.placas[k - 1] != a.placas[k])
                v.push_back({a.placas[k - 1], a.placas[k],
                             a.conectores[k - 1] + QStringLiteral(" ⇄ ") + a.conectores[k]});
    for (const HiloGui& h : hilos)
        if (!h.placa_a.isEmpty() && !h.placa_b.isEmpty() && h.placa_a != h.placa_b)
            v.push_back({h.placa_a, h.placa_b,
                         QStringLiteral("hilo %1 - %2").arg(h.a, h.b)});
    return v;
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
    int  sub = -1;                          // la <placa id> abierta de un sistema
    // La placa de un nombre cualificado (`N/CN5` -> `N`)
    auto placa_de = [](const QString& n) {
        const int b = n.indexOf(QLatin1Char('/'));
        return b > 0 ? n.left(b) : QString();
    };
    auto lista = [](const QString& s) {
        return s.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    };
    while (!r.atEnd()) {
        const QXmlStreamReader::TokenType t = r.readNext();
        if (t == QXmlStreamReader::EndElement && r.name() == QLatin1String("componente")) {
            actual = -1;
            continue;
        }
        if (t == QXmlStreamReader::EndElement && r.name() == QLatin1String("placa")) {
            sub = -1;
            continue;
        }
        if (t != QXmlStreamReader::StartElement) continue;
        const QXmlStreamAttributes a = r.attributes();
        if (!raiz && (r.name() == QLatin1String("placa") ||
                      r.name() == QLatin1String("sistema"))) {
            raiz = true;
            placa.nombre = texto(a, "nombre");
        } else if (r.name() == QLatin1String("placa")) {
            // Una placa DENTRO de un <sistema>, con lo que la describe dentro
            SubPlacaGui s;
            s.id      = texto(a, "id");
            s.nombre  = texto(a, "nombre");
            s.fichero = texto(a, "fichero");
            bool ok = false;
            const int n = a.value(QLatin1String("piezas")).toInt(&ok);
            if (ok) s.n_piezas = n;
            placa.placas.push_back(s);
            sub = int(placa.placas.size()) - 1;
        } else if (sub >= 0 && r.name() == QLatin1String("mcu")) {
            placa.placas[sub].mcus << QStringLiteral("%1 (%2)").arg(texto(a, "ref"),
                                                                    texto(a, "tipo"));
        } else if (sub >= 0 && r.name() == QLatin1String("conector")) {
            ConectorGui c;
            c.ref      = texto(a, "ref");
            c.filas    = a.value(QLatin1String("filas")).toInt();
            c.columnas = a.value(QLatin1String("columnas")).toInt();
            c.zigzag   = a.value(QLatin1String("numeracion")) != QLatin1String("filas");
            bool ok = false;
            const int k = a.value(QLatin1String("acople")).toInt(&ok);
            c.acople   = ok ? k : -1;
            placa.placas[sub].conectores.push_back(c);
        } else if (r.name() == QLatin1String("acopla")) {
            // conectores="A/J1 B/J1 C/J1" -una pila-, o solo a= y b= (un
            // mcu-sim de la primera versión de los sistemas)
            AcopleGui ac;
            ac.conectores = a.hasAttribute(QLatin1String("conectores"))
                                ? lista(texto(a, "conectores"))
                                : QStringList({texto(a, "a"), texto(a, "b")});
            if (a.hasAttribute(QLatin1String("placas")))
                ac.placas = lista(texto(a, "placas"));
            else
                for (const QString& c : ac.conectores) ac.placas << placa_de(c);
            ac.espejo = a.value(QLatin1String("espejo")) == QLatin1String("si");
            placa.acoples.push_back(ac);
        } else if (r.name() == QLatin1String("hilo")) {
            HiloGui h;
            h.a = texto(a, "a");
            h.b = texto(a, "b");
            h.placa_a = placa_de(h.a);
            h.placa_b = placa_de(h.b);
            placa.hilos.push_back(h);
            placa.n_hilos = int(placa.hilos.size());
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
