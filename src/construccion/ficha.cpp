#include "ficha.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QXmlStreamReader>

#include <cmath>

namespace mcusim::build {

namespace {

QString lee_todo(const QString& ruta, QString* error)
{
    QFile f(ruta);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("%1: no se puede leer").arg(ruta);
        return {};
    }
    return QString::fromUtf8(f.readAll());
}

QString a(const QXmlStreamReader& x, const char* n)
{
    return x.attributes().value(QLatin1String(n)).toString();
}

QPointF dir_de(const QString& lado)
{
    if (lado == QLatin1String("arriba")) return {0, -1};
    if (lado == QLatin1String("abajo")) return {0, 1};
    if (lado == QLatin1String("izquierda")) return {-1, 0};
    return {1, 0};
}

QString f1(double v)
{
    QString s = QString::number(v, 'f', 1);
    if (s.endsWith(QLatin1String(".0"))) s.chop(2);
    return s;
}

} // namespace

const Atributo* FichaPieza::atributo(const QString& n) const
{
    for (const Atributo& x : atributos)
        if (x.nombre == n) return &x;
    return nullptr;
}

const Ancla* Forma::ancla(const QString& t) const
{
    for (const Ancla& x : anclas)
        if (x.terminal == t) return &x;
    return nullptr;
}

const Patilla* FichaMcu::patilla(const QString& n) const
{
    for (const Patilla& p : patillas)
        if (p.n == n) return &p;
    return nullptr;
}

FichaPieza lee_pieza(const QString& ruta, QString* error)
{
    FichaPieza f;
    const QString t = lee_todo(ruta, error);
    if (t.isEmpty()) return f;
    QXmlStreamReader x(t);
    QString cuando, fila;
    bool alternativos = false;
    QString texto_de;                 // de qué es el texto que se lee
    Atributo* atr = nullptr;
    QVector<int> del_atributo;
    while (!x.atEnd()) {
        x.readNext();
        if (x.isStartElement()) {
            const auto n = x.name();
            if (n == QLatin1String("pieza")) {
                f.tipo = a(x, "tipo");
            } else if (n == QLatin1String("resumen")) {
                f.resumen = x.readElementText().simplified();
            } else if (n == QLatin1String("sin_terminales")) {
                f.sin_terminales = true;
            } else if (n == QLatin1String("grupo")) {
                cuando = a(x, "cuando");
                alternativos = a(x, "alternativos") == QLatin1String("si");
            } else if (n == QLatin1String("terminal")) {
                Terminal tt;
                tt.nombre = a(x, "nombre");
                tt.cuando = cuando;
                tt.alternativo = alternativos;
                f.terminales.push_back(tt);
            } else if (n == QLatin1String("serie")) {
                const QString h = a(x, "hasta");
                const int desde = a(x, "desde").toInt();
                if (h.isEmpty() || a(x, "abierta") == QLatin1String("si")) {
                    f.series.push_back({a(x, "prefijo"), desde});
                } else if (!h.startsWith(QLatin1String("atributo"))) {
                    for (int k = desde; k <= h.toInt(); ++k) {
                        Terminal tt;
                        tt.nombre = a(x, "prefijo") + QString::number(k);
                        tt.cuando = cuando;
                        f.terminales.push_back(tt);
                    }
                } else {
                    f.series.push_back({a(x, "prefijo"), desde});
                }
            } else if (n == QLatin1String("atributo")) {
                // «columnas, filas, numeracion, nombres, acodado»: uno por nombre
                const QString om = a(x, "omision");
                const QStringList ns = a(x, "nombre").split(QLatin1Char(','), Qt::SkipEmptyParts);
                const QString txt = x.readElementText().simplified();
                for (QString nn : ns) {
                    Atributo at;
                    at.nombre = nn.trimmed();
                    at.omision = om;
                    at.texto = txt;
                    f.atributos.push_back(at);
                }
            }
        }
    }
    if (x.hasError()) {
        if (error) *error = QStringLiteral("%1: %2").arg(ruta, x.errorString());
        return {};
    }
    (void)atr;
    (void)del_atributo;
    // Los repetidos (anodo/catodo ya vienen una vez cada uno)
    QVector<Terminal> u;
    for (const Terminal& tt : f.terminales) {
        bool esta = false;
        for (const Terminal& v : u) esta = esta || v.nombre == tt.nombre;
        if (!esta) u.push_back(tt);
    }
    f.terminales = u;
    return f;
}

Forma lee_forma(const QString& ruta_forma, QString* error)
{
    Forma f;
    const QString t = lee_todo(ruta_forma, error);
    if (t.isEmpty()) return f;
    QXmlStreamReader x(t);
    QString svg;
    while (!x.atEnd()) {
        x.readNext();
        if (!x.isStartElement()) continue;
        if (x.name() == QLatin1String("forma")) {
            f.generador = a(x, "generador");
            svg = a(x, "svg");
            f.tam = QSizeF(a(x, "ancho").toDouble() / 10, a(x, "alto").toDouble() / 10);
        } else if (x.name() == QLatin1String("ancla")) {
            Ancla an;
            an.terminal = a(x, "terminal");
            an.p = QPointF(a(x, "x").toDouble(), a(x, "y").toDouble());
            an.dir = dir_de(a(x, "lado"));
            f.anclas.push_back(an);
        }
    }
    if (!svg.isEmpty()) {
        f.svg = lee_todo(QFileInfo(ruta_forma).dir().filePath(svg), error);
    }
    return f;
}

FichaMcu lee_mcu(const QString& ruta, QString* error)
{
    FichaMcu m;
    const QString t = lee_todo(ruta, error);
    if (t.isEmpty()) return m;
    QXmlStreamReader x(t);
    Patilla* actual = nullptr;
    bool en_mapeo = false;
    while (!x.atEnd()) {
        x.readNext();
        if (x.isEndElement() && x.name() == QLatin1String("patilla")) actual = nullptr;
        if (x.isEndElement() && x.name() == QLatin1String("mapeo")) en_mapeo = false;
        if (!x.isStartElement()) continue;
        const auto n = x.name();
        if (n == QLatin1String("mcu")) {
            m.tipo = a(x, "tipo");
            m.encapsulado = a(x, "encapsulado");
            m.familia = a(x, "familia");
            m.fuente = a(x, "fuente");
        } else if (n == QLatin1String("patilla")) {
            Patilla p;
            p.n = a(x, "n");
            p.nombre = a(x, "nombre");
            p.nodo = a(x, "nodo");
            p.clase = a(x, "clase");
            m.patillas.push_back(p);
            actual = &m.patillas.back();
        } else if (n == QLatin1String("mapeo")) {
            en_mapeo = true;
        } else if (n == QLatin1String("funcion") && actual && !en_mapeo) {
            Funcion fu;
            fu.nombre = a(x, "nombre");
            fu.modo = a(x, "modo").isEmpty() ? a(x, "af") : a(x, "modo");
            fu.simulada = a(x, "simulada") == QLatin1String("si");
            fu.analogica = a(x, "analogica") == QLatin1String("si");
            actual->funciones.push_back(fu);
        }
    }
    if (x.hasError()) {
        if (error) *error = QStringLiteral("%1: %2").arg(ruta, x.errorString());
        return {};
    }
    const QString base = QFileInfo(ruta).dir().filePath(m.tipo);
    m.svg_detalle = base + QStringLiteral(".detalle.svg");
    m.svg_simple = base + QStringLiteral(".simple.svg");
    return m;
}

Perfil lee_perfil(const QString& ruta, QString* error)
{
    Perfil p;
    const QString t = lee_todo(ruta, error);
    if (t.isEmpty()) return p;
    QXmlStreamReader x(t);
    while (!x.atEnd()) {
        x.readNext();
        if (!x.isStartElement()) continue;
        if (x.name() == QLatin1String("conector")) {
            p.nombre = a(x, "perfil");
            p.titulo = a(x, "titulo");
            p.fuente = a(x, "fuente");
            p.filas = a(x, "filas").toInt();
            p.columnas = a(x, "columnas").toInt();
        } else if (x.name() == QLatin1String("pin")) {
            p.pines.push_back({a(x, "n").toInt(), a(x, "uso"), a(x, "clase"), a(x, "otros")});
        }
    }
    return p;
}

QStringList pines_conector(const QMap<QString, QString>& at)
{
    const int filas = std::max(1, at.value(QStringLiteral("filas"), QStringLiteral("1")).toInt());
    const int col = std::max(1, at.value(QStringLiteral("columnas"), QStringLiteral("1")).toInt());
    const QStringList nombres = at.value(QStringLiteral("nombres")).split(QLatin1Char(' '),
                                                                           Qt::SkipEmptyParts);
    QStringList out;
    for (int k = 1; k <= filas * col; ++k) {
        const QString nm = k - 1 < nombres.size() ? nombres[k - 1] : QString();
        out << (nm.isEmpty() || nm == QLatin1String("-") ? QString::number(k) : nm);
    }
    return out;
}

QStringList terminales_de(const FichaPieza& f, const QMap<QString, QString>& at, int n_serie)
{
    if (f.tipo == QLatin1String("Conector") || f.tipo == QLatin1String("Jumper"))
        return pines_conector(at);
    QStringList out;
    for (const Terminal& t : f.terminales) out << t.nombre;
    for (const Serie& s : f.series)
        for (int k = 0; k < n_serie; ++k) out << s.prefijo + QString::number(s.desde + k);
    return out;
}

Forma genera_forma(const QString& gen, const FichaPieza& ficha, const QMap<QString, QString>& at,
                   int n_serie)
{
    // Unidades de 0,1 mm, como los dibujos de B5
    const double paso = 25.4, pad = 8;
    Forma f;
    f.generador = gen;
    QString cuerpo;
    double ancho = 0, alto = 0;
    if (gen == QLatin1String("conector")) {
        const int filas = std::max(1, at.value(QStringLiteral("filas"), QStringLiteral("1")).toInt());
        const int col = std::max(1, at.value(QStringLiteral("columnas"), QStringLiteral("1")).toInt());
        const bool por_filas = at.value(QStringLiteral("numeracion")) == QLatin1String("filas");
        const QStringList pines = pines_conector(at);
        ancho = col * paso + 20;
        alto = filas * paso + 20;
        cuerpo += QStringLiteral("  <rect x=\"0\" y=\"0\" width=\"%1\" height=\"%2\" rx=\"3\" fill=\"%3\"/>\n")
                      .arg(f1(ancho), f1(alto),
                           ficha.tipo == QLatin1String("Jumper") ? QStringLiteral("#3a3a3a")
                                                                 : QStringLiteral("#1a1a1a"));
        for (int k = 1; k <= filas * col; ++k) {
            // zigzag: el 1 y el 2 enfrentados; filas: la primera fila entera
            const int c = filas == 1 ? k - 1 : por_filas ? (k - 1) % col : (k - 1) / filas;
            const int r = filas == 1 ? 0 : por_filas ? (k - 1) / col : (k - 1) % filas;
            const double x = 10 + paso / 2 + c * paso, y = 10 + paso / 2 + r * paso;
            const QString t = pines[k - 1];
            if (k == 1)
                cuerpo += QStringLiteral("  <rect id=\"$.%1\" x=\"%2\" y=\"%3\" width=\"16\" height=\"16\" "
                                         "fill=\"#c9a227\"/>\n").arg(t, f1(x - pad), f1(y - pad));
            else
                cuerpo += QStringLiteral("  <circle id=\"$.%1\" cx=\"%2\" cy=\"%3\" r=\"%4\" "
                                         "fill=\"#c9a227\"/>\n").arg(t, f1(x), f1(y), f1(pad));
            const bool abajo = filas == 1 || r == filas - 1;
            f.anclas.push_back({t, QPointF(x, y), QPointF(0, abajo ? 1 : -1)});
        }
    } else {
        // bus y modulo: una placa pequeña con una tira de todos sus terminales
        const QStringList ts = terminales_de(ficha, at, n_serie);
        ancho = std::max(160.0, ts.size() * paso + 30);
        alto = 160;
        const QString color = gen == QLatin1String("bus") ? QStringLiteral("#59626b")
                                                          : QStringLiteral("#5b3e8a");
        cuerpo += QStringLiteral("  <rect x=\"0\" y=\"0\" width=\"%1\" height=\"%2\" rx=\"12\" fill=\"%3\"/>\n")
                      .arg(f1(ancho), f1(alto), color);
        cuerpo += QStringLiteral("  <text x=\"%1\" y=\"36\" font-size=\"14\" text-anchor=\"middle\" "
                                 "fill=\"#fff\" font-weight=\"bold\">%2</text>\n")
                      .arg(f1(ancho / 2), ficha.tipo.toHtmlEscaped());
        const double x0 = (ancho - ts.size() * paso) / 2 + paso / 2, y = alto - 22;
        for (int i = 0; i < ts.size(); ++i) {
            const double x = x0 + i * paso;
            cuerpo += QStringLiteral("  <circle id=\"$.%1\" cx=\"%2\" cy=\"%3\" r=\"%4\" fill=\"#c9a227\"/>\n")
                          .arg(ts[i], f1(x), f1(y), f1(pad));
            cuerpo += QStringLiteral("  <text x=\"%1\" y=\"%2\" font-size=\"11\" fill=\"#fff\" "
                                     "transform=\"rotate(-90 %1 %2)\">%3</text>\n")
                          .arg(f1(x + 4), f1(y - pad - 6), ts[i].toHtmlEscaped());
            f.anclas.push_back({ts[i], QPointF(x, y), QPointF(0, 1)});
        }
    }
    f.tam = QSizeF(ancho / 10, alto / 10);
    f.svg = QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.2\" baseProfile=\"tiny\" "
                           "viewBox=\"0 0 %1 %2\" width=\"%3mm\" height=\"%4mm\">\n"
                           "<g id=\"$\" font-family=\"sans-serif\">\n%5</g>\n</svg>\n")
                .arg(f1(ancho), f1(alto), QString::number(ancho / 10, 'f', 2),
                     QString::number(alto / 10, 'f', 2), cuerpo);
    return f;
}

} // namespace mcusim::build
