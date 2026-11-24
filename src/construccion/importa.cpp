#include "importa.h"

#include <QMap>
#include <QRegularExpression>
#include <QXmlStreamReader>

#include <algorithm>

#include "exporta_placa.h"

namespace mcusim::build {

namespace {

// (visible=, conectada= y contenido= son atributos de cualquier componente:
// pasan tal cual, como los demás)
const QStringList ATRS_NO_SOPORTADOS = {};

// Cómo se llama el pin `k` de un conector en su forma canónica: su número
QString pin_canonico(const PlacaLeida::Componente& co, const QString& k)
{
    if (co.tipo != QLatin1String("Conector") && co.tipo != QLatin1String("Jumper")) return k;
    const QStringList ps = pines_conector(co.atributos);
    const int i = ps.indexOf(k);
    return i >= 0 ? QString::number(i + 1) : k;
}

} // namespace

PlacaLeida lee_placa(const QString& texto_original, QString* error)
{
    PlacaLeida p;
    QString texto = texto_original;
    texto.remove(QRegularExpression(QStringLiteral("<!--.*?-->"),
                                    QRegularExpression::DotMatchesEverythingOption));
    QXmlStreamReader x(texto);
    int prof = 0;
    PlacaLeida::Componente* co = nullptr;
    while (!x.atEnd()) {
        x.readNext();
        if (x.isEndElement()) {
            --prof;
            if (x.name() == QLatin1String("componente")) co = nullptr;
            continue;
        }
        if (!x.isStartElement()) continue;
        ++prof;
        const auto at = x.attributes();
        const QString n = x.name().toString();
        if (prof == 1) {
            if (n != QLatin1String("placa")) {
                if (error) *error = QStringLiteral("la raíz no es <placa>");
                return {};
            }
            p.nombre = at.value(QLatin1String("nombre")).toString();
        } else if (prof == 2 && n == QLatin1String("mcu")) {
            p.mcus.push_back({at.value(QLatin1String("tipo")).toString(),
                              at.value(QLatin1String("id")).toString()});
            for (const auto& a : at)
                if (a.name() != QLatin1String("tipo") && a.name() != QLatin1String("id"))
                    p.no_soportado << QStringLiteral("<mcu %1=>").arg(a.name().toString());
        } else if (prof == 2 && n == QLatin1String("nodo")) {
            PlacaLeida::Nodo nd;
            nd.id = at.value(QLatin1String("id")).toString();
            nd.externo = at.value(QLatin1String("externo")) == QLatin1String("si");
            nd.bus = at.value(QLatin1String("bus")) == QLatin1String("si");
            nd.une = at.value(QLatin1String("une")).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
            p.nodos.push_back(nd);
        } else if (prof == 2 && n == QLatin1String("componente")) {
            PlacaLeida::Componente c;
            for (const auto& a : at) {
                const QString an = a.name().toString();
                if (an == QLatin1String("tipo")) c.tipo = a.value().toString();
                else if (an == QLatin1String("id")) c.id = a.value().toString();
                else {
                    if (ATRS_NO_SOPORTADOS.contains(an))
                        p.no_soportado << QStringLiteral("<componente %1=>").arg(an);
                    c.atributos.insert(an, a.value().toString());
                }
            }
            p.componentes.push_back(c);
            co = &p.componentes.back();
        } else if (prof == 3 && n == QLatin1String("pin") && co) {
            co->pines.push_back({at.value(QLatin1String("nombre")).toString(),
                                 at.value(QLatin1String("nodo")).toString()});
        } else if (prof == 2 || (prof == 3 && n != QLatin1String("pin"))) {
            p.no_soportado << QStringLiteral("<%1>").arg(n);
        }
    }
    if (x.hasError()) {
        if (error) *error = x.errorString();
        return {};
    }
    p.no_soportado.removeDuplicates();
    return p;
}

// Las redes, como claves -> miembros
// ¿Tiene la pieza ese terminal? Si la biblioteca no la conoce, se da por bueno
static bool tiene_terminal(const PlacaLeida::Componente& co, const QString& pin, const Catalogo& c)
{
    const FichaPieza* f = c.pieza(co.tipo);
    if (!f) return true;
    if (co.tipo == QLatin1String("Conector") || co.tipo == QLatin1String("Jumper")) return true;
    for (const Serie& se : f->series)          // las series, abiertas: d0, d1...
        if (pin.startsWith(se.prefijo)) {
            bool ok = false;
            const int n = pin.mid(se.prefijo.size()).toInt(&ok);
            if (ok && n >= se.desde) return true;
        }
    return terminales_de(*f, co.atributos, 0).contains(pin);
}

static QMap<QString, QStringList> redes_de(const PlacaLeida& p, const Catalogo& c)
{
    // Los nodos que son de un MCU: el nombre de cada patilla
    // (y sus alias: en el ESP32-C3 la patilla «SPICS0/GPIO14» es el nodo
    // GPIO14, y mcu-sim acepta SPICS0 por él; c3_desc.h)
    QSet<QString> de_mcu;
    QMap<QString, QString> alias;
    for (const PlacaLeida::Mcu& m : p.mcus)
        if (const FichaMcu* f = c.mcu(m.tipo))
            for (const Patilla& pt : f->patillas) {
                if (pt.nodo.isEmpty()) continue;
                de_mcu.insert(pt.nodo);
                const QStringList otros = pt.nombre.split(QLatin1Char('/'));
                if (otros.size() > 1)
                    for (const QString& o : otros)
                        if (o != pt.nodo) alias.insert(o, pt.nodo);
            }
    QMap<QString, const PlacaLeida::Componente*> conectores;
    for (const PlacaLeida::Componente& co : p.componentes)
        if (co.tipo == QLatin1String("Conector") || co.tipo == QLatin1String("Jumper"))
            conectores.insert(co.id, &co);
    QMap<QString, QStringList> une;
    for (const PlacaLeida::Nodo& nd : p.nodos)
        if (!nd.une.isEmpty()) une.insert(nd.id, nd.une);

    QMap<QString, QStringList> redes;
    auto clave_y_miembros = [&](QString nodo, QStringList& implicitos) -> QString {
        if (!de_mcu.contains(nodo) && alias.contains(nodo)) nodo = alias.value(nodo);
        if (une.contains(nodo)) {
            for (const QString& pd : une.value(nodo)) implicitos << QStringLiteral("mcu:") + pd;
            return QStringLiteral("une:") + nodo;
        }
        if (de_mcu.contains(nodo)) {
            implicitos << QStringLiteral("mcu:") + nodo;
            return QStringLiteral("mcu:") + nodo;
        }
        const QString id = nodo.section(QLatin1Char('.'), 0, 0);
        if (conectores.contains(id) && nodo.contains(QLatin1Char('.'))) {
            const QString t = id + QLatin1Char('.') + pin_canonico(*conectores.value(id), nodo.section(QLatin1Char('.'), 1));
            implicitos << t;
            return QStringLiteral("con:") + t;
        }
        return QStringLiteral("ext:") + nodo;
    };
    for (const PlacaLeida::Componente& co : p.componentes)
        for (const auto& [pin, nodo] : co.pines) {
            // Un <pin> que la pieza no tiene, mcu-sim lo pasa por alto sin
            // decir nada: no une nada y no cuenta
            if (!tiene_terminal(co, pin, c)) continue;
            QStringList imp;
            const QString k = clave_y_miembros(nodo, imp);
            QStringList& r = redes[k];
            for (const QString& i : imp)
                if (!r.contains(i)) r << i;
            const QString t = co.id + QLatin1Char('.') + pin_canonico(co, pin);
            if (!r.contains(t)) r << t;
        }
    return redes;
}

Particion particion(const PlacaLeida& p, const Catalogo& c)
{
    Particion out;
    const auto redes = redes_de(p, c);
    for (QStringList r : redes) {
        if (r.size() < 2) continue;
        std::sort(r.begin(), r.end());
        out.insert(r);
    }
    return out;
}

Documento reconstruye(const PlacaLeida& p, const Catalogo& c, QString* motivo)
{
    Documento d;
    d.nombre = p.nombre.isEmpty() ? QStringLiteral("placa") : p.nombre;
    if (!p.no_soportado.isEmpty()) {
        if (motivo) *motivo = QStringLiteral("lleva %1").arg(p.no_soportado.join(QStringLiteral(", ")));
        return {};
    }
    // Las piezas, en una rejilla de 30 mm
    int k = 0;
    auto sitio = [&]() { const QPointF q((k % 5) * 30.0 + 15, (k / 5) * 30.0 + 15); ++k; return q; };
    QMap<QString, QString> id_mcu;     // tipo -> id
    for (const PlacaLeida::Mcu& m : p.mcus) {
        if (!c.mcu(m.tipo)) {
            if (motivo) *motivo = QStringLiteral("la biblioteca no tiene el MCU %1").arg(m.tipo);
            return {};
        }
        Instancia i;
        i.clase = Instancia::Mcu;
        i.id = m.id.isEmpty() ? QStringLiteral("u0") : m.id;
        i.tipo = m.tipo;
        i.fuente = QStringLiteral("mcus/") + m.tipo;
        i.pos = sitio();
        d.instancias.push_back(i);
        id_mcu.insert(m.tipo, i.id);
    }
    for (const PlacaLeida::Componente& co : p.componentes) {
        if (!c.pieza(co.tipo)) {
            if (motivo) *motivo = QStringLiteral("la biblioteca no tiene el tipo %1").arg(co.tipo);
            return {};
        }
        Instancia i;
        i.id = co.id;
        i.tipo = co.tipo;
        i.fuente = QStringLiteral("piezas/") + co.tipo;
        i.atributos = co.atributos;
        i.pos = sitio();
        // Una serie abierta: tantos como el mayor que use
        int mayor = 0;
        if (const FichaPieza* f = c.pieza(co.tipo))
            for (const Serie& s : f->series)
                for (const auto& pn : co.pines)
                    if (pn.first.startsWith(s.prefijo)) {
                        bool ok = false;
                        const int v = pn.first.mid(s.prefijo.size()).toInt(&ok);
                        if (ok) mayor = std::max(mayor, v - s.desde + 1);
                    }
        if (mayor) i.n_serie = mayor;
        d.instancias.push_back(i);
    }
    if (p.mcus.size() > 1) {
        if (motivo) *motivo = QStringLiteral("tiene más de un MCU");
        return {};
    }
    // Cada red, una estrella desde su primer terminal. Un miembro mcu:NODO es
    // la patilla de ese nodo; el pin de un conector, por su NOMBRE (con
    // nombres="VCC GND", P1.1 es P1.VCC: P1.1 sería el pad PB1)
    QMap<QString, QStringList> nombres_pines;
    for (const PlacaLeida::Componente& co : p.componentes)
        if (co.tipo == QLatin1String("Conector") || co.tipo == QLatin1String("Jumper"))
            nombres_pines.insert(co.id, pines_conector(co.atributos));
    auto por_nombre = [&](const QString& t) {
        const QString id = t.section(QLatin1Char('.'), 0, 0);
        bool ok = false;
        const int k = t.section(QLatin1Char('.'), 1).toInt(&ok);
        const QStringList& ns = nombres_pines[id];
        if (!nombres_pines.contains(id) || !ok || k < 1 || k > ns.size()) return t;
        return id + QLatin1Char('.') + ns[k - 1];
    };
    const auto redes = redes_de(p, c);
    for (auto it = redes.cbegin(); it != redes.cend(); ++it) {
        QStringList ts;
        for (const QString& m : it.value()) {
            if (m.startsWith(QLatin1String("mcu:"))) {
                const Instancia& u = d.instancias.front();
                const QString pos = c.posicion_mcu(u, m.mid(4));
                if (pos.isEmpty()) {
                    if (motivo) *motivo = QStringLiteral("%1 no es una patilla de %2").arg(m.mid(4), u.tipo);
                    return {};
                }
                ts << u.id + QLatin1Char('.') + pos;
            } else {
                ts << por_nombre(m);
            }
        }
        for (int j = 1; j < ts.size(); ++j) {
            Conexion x;
            x.a = ts.front();
            x.b = ts[j];
            d.conexiones.push_back(x);
        }
    }
    for (const PlacaLeida::Nodo& nd : p.nodos) {
        if (!nd.bus) continue;
        for (auto it = redes.cbegin(); it != redes.cend(); ++it)
            if (it.key().endsWith(QLatin1Char(':') + nd.id) && !it.value().isEmpty()) {
                QString t = it.value().front();
                if (t.startsWith(QLatin1String("mcu:")))
                    t = d.instancias.front().id + QLatin1Char('.') + c.posicion_mcu(d.instancias.front(), t.mid(4));
                d.bus << por_nombre(t);
            }
    }
    (void)id_mcu;
    return d;
}

} // namespace mcusim::build
