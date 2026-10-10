#include "disposicion_xml.h"

#include <QHash>
#include <QObject>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace mcusim {

namespace {

QString escapa(QString s)
{
    s.replace(QLatin1Char('&'), QLatin1String("&amp;"));
    s.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    s.replace(QLatin1Char('>'), QLatin1String("&gt;"));
    s.replace(QLatin1Char('"'), QLatin1String("&quot;"));
    return s;
}

QString lienzo_de(const DisposicionXml& d)
{
    return QStringLiteral("%1 %2 %3 %4").arg(numero_xml(d.lienzo[0]), numero_xml(d.lienzo[1]),
                                             numero_xml(d.lienzo[2]), numero_xml(d.lienzo[3]));
}

QString ruta_de(const RutaGui& r)
{
    QStringList c;
    for (const double v : r.codos) c << numero_xml(v);
    return QStringLiteral("<ruta linea=\"%1\" eje=\"%2\" codos=\"%3\"/>")
        .arg(escapa(r.linea), r.horizontal ? QStringLiteral("h") : QStringLiteral("v"),
             c.join(QLatin1Char(' ')));
}

// Una etiqueta del texto: dónde empieza y acaba, cómo se llama, si cierra, si
// se cierra sola y a qué profundidad está
struct Etiqueta {
    qsizetype ini = 0, fin = 0;
    QString   nombre;
    bool      cierre = false, sola = false;
    int       prof = 0;
};

bool etiquetas(const QString& s, QVector<Etiqueta>& out, QString& error)
{
    int prof = 0;
    qsizetype i = 0;
    auto salta = [&](const QString& hasta, qsizetype desde) {
        const qsizetype j = s.indexOf(hasta, desde);
        if (j < 0) return qsizetype(-1);
        return j + hasta.size();
    };
    while ((i = s.indexOf(QLatin1Char('<'), i)) >= 0) {
        qsizetype j = -1;
        if (s.mid(i, 4) == QLatin1String("<!--")) j = salta(QStringLiteral("-->"), i + 4);
        else if (s.mid(i, 9) == QLatin1String("<![CDATA[")) j = salta(QStringLiteral("]]>"), i);
        else if (s.mid(i, 2) == QLatin1String("<?")) j = salta(QStringLiteral("?>"), i);
        else if (s.mid(i, 2) == QLatin1String("<!")) j = salta(QStringLiteral(">"), i);
        if (j >= 0) {
            i = j;
            continue;
        }
        if (s.mid(i, 2) == QLatin1String("<!") || s.mid(i, 2) == QLatin1String("<?")) {
            error = QObject::tr("un comentario o una declaracion sin cerrar");
            return false;
        }
        // Hasta el '>' que no esté entre comillas
        j = i + 1;
        QChar q;
        for (; j < s.size(); ++j) {
            const QChar c = s[j];
            if (q.isNull()) {
                if (c == QLatin1Char('"') || c == QLatin1Char('\'')) q = c;
                else if (c == QLatin1Char('>')) break;
            } else if (c == q) {
                q = QChar();
            }
        }
        if (j >= s.size()) {
            error = QObject::tr("una etiqueta sin cerrar");
            return false;
        }
        Etiqueta e;
        e.ini = i;
        e.fin = j + 1;
        e.cierre = s[i + 1] == QLatin1Char('/');
        qsizetype k = i + (e.cierre ? 2 : 1);
        while (k < j && !s[k].isSpace() && s[k] != QLatin1Char('/') && s[k] != QLatin1Char('>'))
            e.nombre += s[k++];
        e.sola = !e.cierre && s[j - 1] == QLatin1Char('/');
        if (e.cierre) {
            e.prof = --prof;
        } else {
            e.prof = prof;
            if (!e.sola) ++prof;
        }
        if (prof < 0) {
            error = QObject::tr("cierra una etiqueta que no se abrio: </%1>").arg(e.nombre);
            return false;
        }
        out.push_back(e);
        i = j + 1;
    }
    if (prof != 0) {
        error = QObject::tr("hay etiquetas sin cerrar");
        return false;
    }
    return true;
}

// El valor de un atributo de una etiqueta, o nulo si no lo tiene
QString atributo(const QString& t, const QString& nombre)
{
    const QRegularExpression re(QStringLiteral("\\s%1\\s*=\\s*(\"([^\"]*)\"|'([^']*)')")
                                    .arg(QRegularExpression::escape(nombre)));
    const QRegularExpressionMatch m = re.match(t);
    if (!m.hasMatch()) return QString();
    return m.captured(2).isNull() ? m.captured(3) : m.captured(2);
}

// Pone un atributo en una etiqueta, o lo quita si `v` no tiene valor
void pon_atributo(QString& t, const QString& nombre, const std::optional<QString>& v)
{
    const QRegularExpression re(QStringLiteral("(\\s+)%1\\s*=\\s*(\"[^\"]*\"|'[^']*')")
                                    .arg(QRegularExpression::escape(nombre)));
    const QRegularExpressionMatch m = re.match(t);
    if (m.hasMatch()) {
        if (v)
            t.replace(m.capturedStart(), m.capturedLength(),
                      m.captured(1) + nombre + QStringLiteral("=\"") + escapa(*v) +
                          QLatin1Char('"'));
        else
            t.remove(m.capturedStart(), m.capturedLength());
        return;
    }
    if (!v) return;
    qsizetype k = t.endsWith(QLatin1String("/>")) ? t.size() - 2 : t.size() - 1;
    while (k > 0 && t[k - 1].isSpace()) --k;
    t.insert(k, QStringLiteral(" %1=\"%2\"").arg(nombre, escapa(*v)));
}

struct Cambio {
    qsizetype ini, fin;
    QString   texto;
};

} // namespace

QString numero_xml(double v)
{
    if (std::abs(v - std::round(v)) < 1e-9) return QString::number(qint64(std::llround(v)));
    return QString::number(v, 'g', 10);
}

QString texto_disposicion(const DisposicionXml& d, const QString& nombre)
{
    QStringList l;
    if (d.sistema) {
        l << QStringLiteral("<!-- La disposicion de la ventana para \"%1\" (mcu-sim-gui): estos "
                            "atributos en el\n     <sistema> y en cada <placa id>, y las <ruta> "
                            "dentro del sistema -->")
                 .arg(nombre);
        l << (d.lienzo_fijo ? QStringLiteral("<sistema lienzo=\"%1\">").arg(lienzo_de(d))
                            : QStringLiteral("<sistema>"));
        for (const DisposicionXml::Placa& p : d.placas) {
            QString t = QStringLiteral("  <placa id=\"%1\"").arg(escapa(p.id));
            if (!p.fichero.isEmpty()) t += QStringLiteral(" fichero=\"%1\"").arg(escapa(p.fichero));
            if (p.colocada)
                t += QStringLiteral(" x=\"%1\" y=\"%2\"").arg(numero_xml(p.x), numero_xml(p.y));
            if (p.escala) t += QStringLiteral(" escala=\"%1\"").arg(numero_xml(*p.escala));
            if (p.giro) t += QStringLiteral(" giro=\"%1\"").arg(*p.giro);
            l << t + QStringLiteral("/>");
        }
        for (const RutaGui& r : d.rutas) l << QStringLiteral("  ") + ruta_de(r);
        l << QStringLiteral("</sistema>");
    } else {
        l << QStringLiteral("<!-- La disposicion de la ventana para \"%1\" (mcu-sim-gui): estos "
                            "atributos en la <placa> -->")
                 .arg(nombre);
        QString t = QStringLiteral("<placa nombre=\"%1\"").arg(escapa(nombre));
        if (!d.placas.isEmpty()) {
            const DisposicionXml::Placa& p = d.placas.front();
            if (p.escala && *p.escala != 1.0)
                t += QStringLiteral(" escala=\"%1\"").arg(numero_xml(*p.escala));
            if (p.giro && *p.giro != 0) t += QStringLiteral(" giro=\"%1\"").arg(*p.giro);
        }
        if (d.lienzo_fijo) t += QStringLiteral(" lienzo=\"%1\"").arg(lienzo_de(d));
        l << t + QStringLiteral(">");
    }
    return l.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

bool escribe_disposicion(QByteArray& xml, const DisposicionXml& d, QString& error)
{
    const QString s = QString::fromUtf8(xml);
    QVector<Etiqueta> et;
    if (!etiquetas(s, et, error)) return false;
    int raiz = -1;
    for (int i = 0; i < et.size(); ++i)
        if (!et[i].cierre && et[i].prof == 0) {
            raiz = i;
            break;
        }
    const QString se_espera = d.sistema ? QStringLiteral("sistema") : QStringLiteral("placa");
    if (raiz < 0 || et[raiz].nombre != se_espera) {
        error = QObject::tr("la raiz del XML no es un <%1>").arg(se_espera);
        return false;
    }
    const QString nl = s.contains(QLatin1String("\r\n")) ? QStringLiteral("\r\n")
                                                          : QStringLiteral("\n");
    QVector<Cambio> cambios;
    auto etiqueta = [&](int i) { return s.mid(et[i].ini, et[i].fin - et[i].ini); };

    // La raíz: el lienzo y, en una placa suelta, su giro y su escala
    {
        QString t = etiqueta(raiz);
        pon_atributo(t, QStringLiteral("lienzo"),
                     d.lienzo_fijo ? std::optional<QString>(lienzo_de(d)) : std::nullopt);
        if (!d.sistema && !d.placas.isEmpty()) {
            const DisposicionXml::Placa& p = d.placas.front();
            if (p.giro)
                pon_atributo(t, QStringLiteral("giro"),
                             *p.giro ? std::optional<QString>(QString::number(*p.giro))
                                     : std::nullopt);
            if (p.escala)
                pon_atributo(t, QStringLiteral("escala"),
                             *p.escala != 1.0 ? std::optional<QString>(numero_xml(*p.escala))
                                              : std::nullopt);
        }
        cambios.push_back({et[raiz].ini, et[raiz].fin, t});
    }
    if (d.sistema) {
        // Cada <placa id> del sistema
        QHash<QString, int> por_id;
        for (int i = 0; i < et.size(); ++i)
            if (!et[i].cierre && et[i].prof == 1 && et[i].nombre == QLatin1String("placa")) {
                const QString id = atributo(etiqueta(i), QStringLiteral("id"));
                if (!id.isEmpty()) por_id.insert(id, i);
            }
        for (const DisposicionXml::Placa& p : d.placas) {
            const int i = por_id.value(p.id, -1);
            if (i < 0) {
                error = QObject::tr("el XML no tiene ninguna <placa id=\"%1\">").arg(p.id);
                return false;
            }
            QString t = etiqueta(i);
            pon_atributo(t, QStringLiteral("x"),
                         p.colocada ? std::optional<QString>(numero_xml(p.x)) : std::nullopt);
            pon_atributo(t, QStringLiteral("y"),
                         p.colocada ? std::optional<QString>(numero_xml(p.y)) : std::nullopt);
            if (p.escala) pon_atributo(t, QStringLiteral("escala"), numero_xml(*p.escala));
            if (p.giro) pon_atributo(t, QStringLiteral("giro"), QString::number(*p.giro));
            cambios.push_back({et[i].ini, et[i].fin, t});
        }
        // Las <ruta> que había, fuera, con su línea si se queda vacía
        for (int i = 0; i < et.size(); ++i) {
            if (et[i].cierre || et[i].prof != 1 || et[i].nombre != QLatin1String("ruta")) continue;
            if (!et[i].sola) {
                error = QObject::tr("una <ruta> que no se cierra sola: no se sabe quitar");
                return false;
            }
            qsizetype ini = et[i].ini, fin = et[i].fin;
            qsizetype a = ini;
            while (a > 0 && (s[a - 1] == QLatin1Char(' ') || s[a - 1] == QLatin1Char('\t'))) --a;
            qsizetype b = fin;
            while (b < s.size() && (s[b] == QLatin1Char(' ') || s[b] == QLatin1Char('\t'))) ++b;
            const bool linea_sola = (a == 0 || s[a - 1] == QLatin1Char('\n')) &&
                                    (b >= s.size() || s[b] == QLatin1Char('\r') ||
                                     s[b] == QLatin1Char('\n'));
            if (linea_sola) {
                ini = a;
                fin = b;
                if (fin < s.size() && s[fin] == QLatin1Char('\r')) ++fin;
                if (fin < s.size() && s[fin] == QLatin1Char('\n')) ++fin;
            }
            cambios.push_back({ini, fin, QString()});
        }
        // Las nuevas, al final del sistema, con la sangría de sus placas
        if (!d.rutas.isEmpty()) {
            int fin_raiz = -1;
            for (int i = et.size() - 1; i >= 0; --i)
                if (et[i].cierre && et[i].prof == 0) {
                    fin_raiz = i;
                    break;
                }
            QString sangria = QStringLiteral("  ");
            if (!por_id.isEmpty()) {
                const qsizetype p = et[*std::min_element(por_id.begin(), por_id.end())].ini;
                qsizetype a = p;
                while (a > 0 && (s[a - 1] == QLatin1Char(' ') || s[a - 1] == QLatin1Char('\t'))) --a;
                if (a == 0 || s[a - 1] == QLatin1Char('\n')) sangria = s.mid(a, p - a);
            }
            // Al principio de la línea del cierre, si no tiene otra cosa delante
            qsizetype donde = et[fin_raiz].ini;
            qsizetype a = donde;
            while (a > 0 && (s[a - 1] == QLatin1Char(' ') || s[a - 1] == QLatin1Char('\t'))) --a;
            QString t;
            if (a == 0 || s[a - 1] == QLatin1Char('\n')) {
                donde = a;
            } else {
                t = nl;
            }
            for (const RutaGui& r : d.rutas) t += sangria + ruta_de(r) + nl;
            cambios.push_back({donde, donde, t});
        }
    }
    // De atrás adelante: así cada posición sigue valiendo
    std::sort(cambios.begin(), cambios.end(),
              [](const Cambio& x, const Cambio& y) { return x.ini > y.ini; });
    QString r = s;
    for (const Cambio& c : cambios) r.replace(c.ini, c.fin - c.ini, c.texto);
    xml = r.toUtf8();
    return true;
}

} // namespace mcusim
