#include "exporta_placa.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTransform>

#include <cmath>

namespace mcusim::build {

namespace {

QString n(double v)
{
    QString s = QString::number(std::round(v * 100) / 100, 'f', 2);
    while (s.contains(QLatin1Char('.')) && (s.endsWith(QLatin1Char('0')) || s.endsWith(QLatin1Char('.'))))
        s.chop(1);
    return s == QLatin1String("-0") ? QStringLiteral("0") : s;
}

QString esc(const QString& s)
{
    return s.toHtmlEscaped();
}

// El contenido de la pieza: su <g id="$" ...> ... </g>, sin el id de la raíz
// y con los de dentro con el id de la pieza
QString contenido(const QString& svg, const QString& id)
{
    const int i = svg.indexOf(QLatin1String("<g id=\"$\""));
    const int j = svg.lastIndexOf(QLatin1String("</g>"));
    if (i < 0 || j < i) return {};
    QString g = svg.mid(i, j + 4 - i);
    g.replace(0, int(qstrlen("<g id=\"$\"")), QStringLiteral("<g"));
    g.replace(QStringLiteral("id=\"$."), QStringLiteral("id=\"%1.").arg(id));
    return g;
}

} // namespace

QRectF caja_instancia(const Instancia& i, const Catalogo& c)
{
    const QSizeF t = c.dibujo(i).tam * i.escala;
    const bool de_lado = i.giro == 90 || i.giro == 270;
    const QSizeF s = de_lado ? QSizeF(t.height(), t.width()) : t;
    return QRectF(i.pos.x() - s.width() / 2, i.pos.y() - s.height() / 2, s.width(), s.height());
}

QRectF marco_efectivo(const Documento& d, const Catalogo& c)
{
    if (d.marco.isValid() && !d.marco.isEmpty()) return d.marco;
    QRectF r;
    for (const Instancia& i : d.instancias) r = r.isNull() ? caja_instancia(i, c) : r.united(caja_instancia(i, c));
    if (r.isNull()) return QRectF(0, 0, 50, 30);
    return r.adjusted(-3, -3, 3, 3);
}

QString xml_placa(const Documento& d, const Catalogo& c, const Redes& r)
{
    QString o;
    o += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    o += QStringLiteral("<!-- %1: placa hecha con %2 (doc/analisis_build.md de mcu-sim).\n"
                        "     Al reabrirla, el lienzo sale del <build>; los <pin> y los <nodo> se\n"
                        "     vuelven a calcular de las conexiones: lo que se cambie a mano fuera\n"
                        "     del <build> y de los atributos se pierde al exportarla otra vez. -->\n").arg(esc(d.nombre), QLatin1String(PROGRAMA));
    o += QStringLiteral("<placa nombre=\"%1\">\n").arg(esc(d.nombre));
    for (const Instancia& i : d.instancias)
        if (i.clase == Instancia::Mcu)
            o += QStringLiteral("  <mcu tipo=\"%1\" id=\"%2\"/>\n").arg(esc(i.tipo), esc(i.id));
    for (const NodoDeclarado& nd : r.nodos) {
        o += QStringLiteral("  <nodo id=\"%1\"").arg(esc(nd.id));
        if (!nd.une.isEmpty()) o += QStringLiteral(" une=\"%1\"").arg(esc(nd.une.join(QLatin1Char(' '))));
        if (nd.externo) o += QStringLiteral(" externo=\"si\"");
        if (nd.bus) o += QStringLiteral(" bus=\"si\"");
        o += QStringLiteral("/>\n");
    }
    for (const Instancia& i : d.instancias) {
        if (i.clase == Instancia::Mcu) continue;
        const FichaPieza* f = c.pieza(i.tipo);
        o += QStringLiteral("  <componente tipo=\"%1\" id=\"%2\"").arg(esc(i.tipo), esc(i.id));
        for (auto it = i.atributos.cbegin(); it != i.atributos.cend(); ++it) {
            const Atributo* a = f ? f->atributo(it.key()) : nullptr;
            if (it.value().isEmpty() || (a && a->omision == it.value())) continue;
            o += QStringLiteral(" %1=\"%2\"").arg(it.key(), esc(it.value()));
        }
        QStringList pines;
        for (const QString& t : c.terminales(i)) {
            const QString ref = i.id + QLatin1Char('.') + t;
            if (r.nodo_de.contains(ref))
                pines << QStringLiteral("    <pin nombre=\"%1\" nodo=\"%2\"/>\n").arg(esc(t), esc(r.nodo_de.value(ref)));
        }
        if (pines.isEmpty()) {
            o += QStringLiteral("/>\n");
        } else {
            o += QStringLiteral(">\n") + pines.join(QString()) + QStringLiteral("  </componente>\n");
        }
    }
    // El <build> (§7): lo que solo usa mcu-sim-build
    const QRectF m = marco_efectivo(d, c);
    o += QStringLiteral("\n  <!-- Lo que sigue solo lo usa mcu-sim-build: mcu-sim lo comprueba y no lo\n"
                        "       manda nunca a la ventana -->\n");
    o += QStringLiteral("  <build version=\"1\" programa=\"%1\">\n").arg(QLatin1String(PROGRAMA));
    o += QStringLiteral("    <marco x=\"%1\" y=\"%2\" ancho=\"%3\" alto=\"%4\"/>\n")
             .arg(n(m.x()), n(m.y()), n(m.width()), n(m.height()));
    for (const Instancia& i : d.instancias) {
        o += QStringLiteral("    <instancia id=\"%1\" fuente=\"%2\" x=\"%3\" y=\"%4\"")
                 .arg(esc(i.id), esc(i.fuente), n(i.pos.x()), n(i.pos.y()));
        if (i.giro) o += QStringLiteral(" giro=\"%1\"").arg(i.giro);
        if (i.escala != 1.0) o += QStringLiteral(" escala=\"%1\"").arg(n(i.escala));
        if (i.clase == Instancia::Mcu && i.detalle) o += QStringLiteral(" dibujo=\"detalle\"");
        o += QStringLiteral("/>\n");
    }
    for (const Conexion& x : d.conexiones) {
        o += QStringLiteral("    <conexion a=\"%1\" b=\"%2\"").arg(esc(x.a), esc(x.b));
        if (x.tramos) {
            o += QStringLiteral(" trazado=\"tramos\" eje=\"%1\"").arg(x.horizontal ? QLatin1Char('h') : QLatin1Char('v'));
            if (!x.codos.isEmpty()) {
                QStringList cs;
                for (double v : x.codos) cs << n(v);
                o += QStringLiteral(" codos=\"%1\"").arg(cs.join(QLatin1Char(' ')));
            }
        }
        if (!x.visible) o += QStringLiteral(" visible=\"no\"");
        o += QStringLiteral("/>\n");
    }
    o += QStringLiteral("  </build>\n</placa>\n");
    return o;
}

QString svg_placa(const Documento& d, const Catalogo& c)
{
    const QRectF m = marco_efectivo(d, c);
    const double W = m.width() * 10, H = m.height() * 10;
    QString o;
    o += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    o += QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.2\" baseProfile=\"tiny\" "
                        "viewBox=\"0 0 %1 %2\" width=\"%3mm\" height=\"%4mm\">\n")
             .arg(n(W), n(H), QString::number(m.width(), 'f', 2), QString::number(m.height(), 'f', 2));
    o += QStringLiteral("<!--\n  %1: el dibujo de la placa, compuesto por %2 con los dibujos de su\n"
                        "  biblioteca (doc/analisis_build.md de mcu-sim, §5.3). Unidades de 0,1 mm.\n"
                        "  Cada pieza es un <g> con su id; sus patillas, ID.terminal. Las\n"
                        "  conexiones de la placa no se dibujan: solo se ven en mcu-sim-build.\n-->\n")
             .arg(esc(d.nombre), QLatin1String(PROGRAMA));
    o += QStringLiteral("<rect x=\"0\" y=\"0\" width=\"%1\" height=\"%2\" rx=\"20\" fill=\"#1d5c3a\" "
                        "stroke=\"#0f3a22\" stroke-width=\"4\"/>\n").arg(n(W), n(H));
    for (const Instancia& i : d.instancias) {
        const Dibujo dib = c.dibujo(i);
        if (!dib.valido()) continue;
        const double cx = (i.pos.x() - m.x()) * 10, cy = (i.pos.y() - m.y()) * 10;
        const double w = dib.tam.width() * 10, h = dib.tam.height() * 10;
        o += QStringLiteral("<g id=\"%1\" transform=\"translate(%2,%3)").arg(esc(i.id), n(cx), n(cy));
        if (i.giro) o += QStringLiteral(" rotate(%1)").arg(i.giro);
        if (i.escala != 1.0) o += QStringLiteral(" scale(%1)").arg(n(i.escala));
        o += QStringLiteral(" translate(%1,%2)\">\n").arg(n(-w / 2), n(-h / 2));
        o += contenido(dib.svg, i.id) + QStringLiteral("\n</g>\n");
    }
    o += QStringLiteral("</svg>\n");
    return o;
}

QString exporta(const Documento& d, const Catalogo& c, const QString& ruta_xml)
{
    const Redes r = calcula_redes(d, c);
    const QFileInfo fi(ruta_xml);
    const QString ruta_svg = fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".svg"));
    for (const auto& [ruta, texto] : {std::make_pair(ruta_xml, xml_placa(d, c, r)),
                                      std::make_pair(ruta_svg, svg_placa(d, c))}) {
        QFile f(ruta);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return QStringLiteral("%1: no se puede escribir").arg(ruta);
        f.write(texto.toUtf8());
    }
    return {};
}

} // namespace mcusim::build
