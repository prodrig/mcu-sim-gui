#include "generado.h"

#include <QSet>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace mcusim {

namespace {

// Las medidas, en milímetros
constexpr double MARGEN = 4.0;
constexpr double TITULO = 7.0;      // lo que ocupa el nombre arriba
constexpr double CELDA_X = 13.0;    // una pieza: su glifo y su nombre
constexpr double CELDA_Y = 13.0;
constexpr double CHIP = 14.0;
constexpr double PASO = 2.54;       // el de los pines de un conector
constexpr double ANCHO_MIN = 56.0;

QString esc(const QString& s) { return s.toHtmlEscaped(); }
QString n(double v) { return QString::number(v, 'f', 2); }

bool es_01(const ObservableGui& o)
{
    return o.min == 0 && o.max == 1 && o.unidad.isEmpty() && !o.alarma;
}

// El nombre local de algo cualificado: "N/CN5" -> "CN5"
QString local(const QString& ref)
{
    const int b = ref.indexOf(QLatin1Char('/'));
    return b < 0 ? ref : ref.mid(b + 1);
}

// Un glifo centrado en (x, y), con el id de la pieza, y su nombre debajo
QString glifo(const PiezaGui& p, double x, double y, bool claro)
{
    const QString g = glifo_de(p);
    const QString id = esc(p.id_local);
    QString s = QStringLiteral("  <g id=\"%1\">\n").arg(id);
    if (g == QLatin1String("pantalla")) {
        // Un vidrio oscuro con la forma de la imagen: encima va lo que enseñe
        const ImagenGui& im = p.imagenes[0];
        const double alto = 9.0, ancho = alto * im.ancho / std::max(1, im.alto);
        const double a = std::min(ancho, 11.0), h = alto * a / ancho;
        s += QStringLiteral("    <rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" "
                            "fill=\"#1b1e22\" stroke=\"#000000\" stroke-width=\"0.3\"/>\n")
                 .arg(n(x - a / 2), n(y - h / 2), n(a), n(h));
    } else if (g == QLatin1String("boton")) {
        s += QStringLiteral("    <rect x=\"%1\" y=\"%2\" width=\"7\" height=\"7\" rx=\"0.8\" "
                            "fill=\"#c9ccd1\" stroke=\"#7d8288\" stroke-width=\"0.3\"/>\n")
                 .arg(n(x - 3.5), n(y - 3.5));
        s += QStringLiteral("    <circle cx=\"%1\" cy=\"%2\" r=\"2.4\" fill=\"#3a6fc4\" "
                            "stroke=\"#28508f\" stroke-width=\"0.3\"/>\n").arg(n(x), n(y));
    } else if (g == QLatin1String("interruptor")) {
        s += QStringLiteral("    <rect x=\"%1\" y=\"%2\" width=\"8\" height=\"4\" rx=\"2\" "
                            "fill=\"#d8d8d8\" stroke=\"#6f6f6f\" stroke-width=\"0.3\"/>\n")
                 .arg(n(x - 4), n(y - 2));
        s += QStringLiteral("    <circle cx=\"%1\" cy=\"%2\" r=\"1.6\" fill=\"#555555\"/>\n")
                 .arg(n(x - 1.8), n(y));
    } else if (g == QLatin1String("mando")) {
        s += QStringLiteral("    <circle cx=\"%1\" cy=\"%2\" r=\"3.2\" fill=\"#9aa0a6\" "
                            "stroke=\"#5f6368\" stroke-width=\"0.3\"/>\n").arg(n(x), n(y));
        s += QStringLiteral("    <line x1=\"%1\" y1=\"%2\" x2=\"%3\" y2=\"%4\" stroke=\"#202124\" "
                            "stroke-width=\"0.6\"/>\n")
                 .arg(n(x), n(y), n(x + 2.0), n(y - 2.0));
    } else if (g == QLatin1String("piloto")) {
        s += QStringLiteral("    <circle cx=\"%1\" cy=\"%2\" r=\"2.4\" fill=\"#f2b632\" "
                            "stroke=\"#a77a12\" stroke-width=\"0.3\"/>\n").arg(n(x), n(y));
    } else if (g == QLatin1String("medida")) {
        s += QStringLiteral("    <rect x=\"%1\" y=\"%2\" width=\"9\" height=\"5\" rx=\"0.5\" "
                            "fill=\"#f4f4f0\" stroke=\"#333333\" stroke-width=\"0.3\"/>\n")
                 .arg(n(x - 4.5), n(y - 2.5));
        s += QStringLiteral("    <line x1=\"%1\" y1=\"%2\" x2=\"%3\" y2=\"%4\" stroke=\"#c62828\" "
                            "stroke-width=\"0.4\"/>\n")
                 .arg(n(x), n(y + 1.8), n(x + 2.2), n(y - 1.2));
    } else {
        s += QStringLiteral("    <rect x=\"%1\" y=\"%2\" width=\"6\" height=\"2.4\" "
                            "fill=\"#c8b48a\" stroke=\"#7a6a48\" stroke-width=\"0.25\"/>\n")
                 .arg(n(x - 3), n(y - 1.2));
    }
    s += QStringLiteral("  </g>\n");
    s += QStringLiteral("  <text x=\"%1\" y=\"%2\" font-family=\"sans-serif\" font-size=\"2.4\" "
                        "text-anchor=\"middle\" fill=\"%3\">%4</text>\n")
             .arg(n(x), n(y + 5.6), claro ? QStringLiteral("#e8f0ea") : QStringLiteral("#333333"),
                  id);
    return s;
}

} // namespace

QString glifo_de(const PiezaGui& p)
{
    if (!p.imagenes.isEmpty()) return QStringLiteral("pantalla");
    if (!p.mandos.isEmpty()) {
        const QString& t = p.mandos[0].tipo;
        if (t == QLatin1String("interruptor")) return QStringLiteral("interruptor");
        if (t == QLatin1String("continuo") || t == QLatin1String("discreto"))
            return QStringLiteral("mando");
        return QStringLiteral("boton");
    }
    for (const ObservableGui& o : p.observables)
        if (es_01(o)) return QStringLiteral("piloto");
    if (!p.observables.isEmpty()) return QStringLiteral("medida");
    return QStringLiteral("pieza");
}

QVector<int> para_bandeja(const PlacaGui& placa, const QString& placa_id,
                          const QStringList& sin_elemento)
{
    QVector<int> l;
    for (const PiezaGui& p : placa.piezas) {
        if (!placa_id.isEmpty() && p.placa != placa_id) continue;
        if (!sin_elemento.contains(p.id_local)) continue;
        if (!p.visible) continue;            // plan §41
        if (p.observables.isEmpty() && p.mandos.isEmpty() && p.imagenes.isEmpty()) continue;
        l.push_back(p.idx);
    }
    return l;
}

QByteArray dibujo_generado(const PlacaGui& placa, const QString& placa_id,
                           const OpcionesGenerado& o)
{
    const bool bandeja = !o.solo.isEmpty();
    const SubPlacaGui* sub = placa_id.isEmpty() ? nullptr : placa.subplaca(placa_id);

    // Lo que va: chips, conectores descritos y piezas
    QStringList chips;
    QVector<ConectorGui> conectores;
    if (!bandeja) {
        chips = sub ? sub->mcus : placa.mcus;
        if (sub) conectores = sub->conectores;
    }
    QSet<QString> son_conector;
    for (const ConectorGui& c : conectores) son_conector.insert(c.ref);
    QVector<const PiezaGui*> piezas;
    for (const PiezaGui& p : placa.piezas) {
        if (bandeja ? !o.solo.contains(p.idx)
                    : (!placa_id.isEmpty() && p.placa != placa_id))
            continue;
        if (son_conector.contains(p.id)) continue;
        if (!p.visible) continue;            // plan §41: no interesa verla
        piezas.push_back(&p);
    }

    // El ancho: el que pidan los conectores, y no menos que lo mínimo
    double ancho = bandeja ? 2 * MARGEN + std::max<qsizetype>(1, piezas.size()) * CELDA_X
                           : ANCHO_MIN;
    if (bandeja) ancho = std::min(ancho, 2 * MARGEN + 4 * CELDA_X);
    for (const ConectorGui& c : conectores)
        ancho = std::max(ancho, 2 * MARGEN + std::max(1, c.columnas) * PASO + 2);
    ancho = std::max(ancho, 2 * MARGEN + std::min<qsizetype>(chips.size(), 3) * (CHIP + 3));
    const int por_fila = std::max(1, int((ancho - 2 * MARGEN) / CELDA_X));

    // Y el alto, de arriba abajo
    double y = MARGEN + TITULO;
    const double y_chips = y;
    if (!chips.isEmpty()) y += CHIP + 7;
    const double y_piezas = y;
    const int filas = int((piezas.size() + por_fila - 1) / por_fila);
    y += filas * CELDA_Y;
    QVector<double> y_con;
    for (const ConectorGui& c : conectores) {
        y += 4;                                         // su nombre
        y_con.push_back(y);
        y += std::max(1, c.filas) * PASO + 1 + 2;
    }
    const double alto = std::max(y + MARGEN - 2, 30.0);

    const bool claro = !bandeja;                        // texto claro sobre la placa
    QString s;
    s += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    s += QStringLiteral("<!-- Generado por mcu-sim-gui: la placa no trae dibujo -->\n");
    s += QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1mm\" "
                        "height=\"%2mm\" viewBox=\"0 0 %1 %2\">\n").arg(n(ancho), n(alto));
    if (bandeja)
        s += QStringLiteral("  <rect x=\"0.5\" y=\"0.5\" width=\"%1\" height=\"%2\" rx=\"2\" "
                            "fill=\"#f2f2ee\" stroke=\"#9a9a9a\" stroke-width=\"0.5\" "
                            "stroke-dasharray=\"2 1.5\"/>\n").arg(n(ancho - 1), n(alto - 1));
    else
        s += QStringLiteral("  <rect x=\"0.5\" y=\"0.5\" width=\"%1\" height=\"%2\" rx=\"3\" "
                            "fill=\"#2e6b4f\" stroke=\"#1d4a36\" stroke-width=\"1\"/>\n")
                 .arg(n(ancho - 1), n(alto - 1));
    QString titulo = o.titulo;
    if (titulo.isEmpty())
        titulo = sub ? QStringLiteral("%1 · %2").arg(sub->id, sub->nombre) : placa.nombre;
    s += QStringLiteral("  <text x=\"%1\" y=\"%2\" font-family=\"sans-serif\" font-size=\"3.4\" "
                        "font-weight=\"bold\" fill=\"%3\">%4</text>\n")
             .arg(n(MARGEN), n(MARGEN + 3.6),
                  claro ? QStringLiteral("#e8f0ea") : QStringLiteral("#555555"), esc(titulo));

    // Los chips: decorado, con su tipo
    for (int k = 0; k < chips.size(); ++k) {
        const double x = MARGEN + k * (CHIP + 3);
        s += QStringLiteral("  <rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%3\" rx=\"0.6\" "
                            "fill=\"#22272e\" stroke=\"#9aa0a6\" stroke-width=\"0.6\" "
                            "stroke-dasharray=\"0.8 0.6\"/>\n")
                 .arg(n(x), n(y_chips), n(CHIP));
        // "N/u0 (STM32F446RE)" -> "u0" y "STM32F446RE"
        QString nombre = chips[k], tipo;
        const int p = nombre.indexOf(QLatin1String(" ("));
        if (p > 0) {
            tipo = nombre.mid(p + 2).chopped(1);
            nombre = nombre.left(p);
        }
        s += QStringLiteral("  <text x=\"%1\" y=\"%2\" font-family=\"sans-serif\" "
                            "font-size=\"2.6\" text-anchor=\"middle\" fill=\"#ffffff\">%3</text>\n")
                 .arg(n(x + CHIP / 2), n(y_chips + CHIP / 2), esc(local(nombre)));
        s += QStringLiteral("  <text x=\"%1\" y=\"%2\" font-family=\"sans-serif\" "
                            "font-size=\"1.9\" text-anchor=\"middle\" fill=\"#e8f0ea\">%3</text>\n")
                 .arg(n(x + CHIP / 2), n(y_chips + CHIP + 3), esc(tipo));
    }

    // Las piezas, en filas
    for (int k = 0; k < piezas.size(); ++k) {
        const double x = MARGEN + (k % por_fila) * CELDA_X + CELDA_X / 2;
        const double yc = y_piezas + (k / por_fila) * CELDA_Y + 4.5;
        s += glifo(*piezas[k], x, yc, claro);
    }

    // Los conectores, cada uno en su fila, con sus pines numerados como dicen
    for (int k = 0; k < conectores.size(); ++k) {
        const ConectorGui& c = conectores[k];
        const int fi = std::max(1, c.filas), co = std::max(1, c.columnas);
        const double x0 = MARGEN, y0 = y_con[k];
        const QString id = esc(local(c.ref));
        s += QStringLiteral("  <text x=\"%1\" y=\"%2\" font-family=\"sans-serif\" "
                            "font-size=\"2.4\" fill=\"#e8f0ea\">%3</text>\n")
                 .arg(n(x0), n(y0 - 1), id);
        s += QStringLiteral("  <g id=\"%1\">\n").arg(id);
        s += QStringLiteral("    <rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" "
                            "fill=\"#1b1b1b\" stroke=\"#000000\" stroke-width=\"0.2\"/>\n")
                 .arg(n(x0), n(y0), n(co * PASO + 1), n(fi * PASO + 1));
        for (int pin = 1; pin <= fi * co; ++pin) {
            // zigzag: 1 y 2 en la primera columna, 3 y 4 en la segunda...;
            // por filas: la primera fila entera, luego la segunda
            const int col = c.zigzag ? (pin - 1) / fi : (pin - 1) % co;
            const int fil = c.zigzag ? (pin - 1) % fi : (pin - 1) / co;
            s += QStringLiteral("    <rect id=\"%1.%2\" x=\"%3\" y=\"%4\" width=\"1.1\" "
                                "height=\"1.1\" fill=\"#d4af37\"/>\n")
                     .arg(id).arg(pin)
                     .arg(n(x0 + 0.5 + col * PASO + (PASO - 1.1) / 2),
                          n(y0 + 0.5 + fil * PASO + (PASO - 1.1) / 2));
        }
        s += QStringLiteral("  </g>\n");
    }
    s += QStringLiteral("</svg>\n");
    return s.toUtf8();
}

} // namespace mcusim
