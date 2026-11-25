#include "abre.h"

#include <QRegularExpression>
#include <QXmlStreamReader>

#include <cmath>

#include "biblioteca.h"
#include "catalogo.h"
#include "exporta_placa.h"
#include "ficha.h"
#include "importa.h"

namespace mcusim::build {

namespace {
QString sin_comentarios(QString t)
{
    t.remove(QRegularExpression(QStringLiteral("<!--.*?-->"),
                                QRegularExpression::DotMatchesEverythingOption));
    return t;
}
double num(const QXmlStreamAttributes& a, const char* n, double omision = 0)
{
    bool ok = false;
    const double v = a.value(QLatin1String(n)).toDouble(&ok);
    return ok ? v : omision;
}
bool parecidos(const QRectF& a, const QRectF& b)
{
    return std::abs(a.x() - b.x()) < 0.05 && std::abs(a.y() - b.y()) < 0.05 &&
           std::abs(a.width() - b.width()) < 0.05 && std::abs(a.height() - b.height()) < 0.05;
}
} // namespace

bool tiene_build(const QString& texto)
{
    return sin_comentarios(texto).contains(QLatin1String("<build"));
}

Documento abre_build(const QString& texto, const Catalogo& c, QString* error)
{
    auto falla = [&](const QString& e) {
        if (error) *error = e;
        return Documento();
    };
    QString err;
    const PlacaLeida p = lee_placa(texto, &err);
    if (!err.isEmpty()) return falla(err);

    Documento d;
    d.nombre = p.nombre.isEmpty() ? QStringLiteral("placa") : p.nombre;
    QRectF marco;
    bool hay_build = false;
    QXmlStreamReader x(sin_comentarios(texto));
    bool dentro = false;
    while (!x.atEnd()) {
        x.readNext();
        if (x.isEndElement() && x.name() == QLatin1String("build")) dentro = false;
        if (!x.isStartElement()) continue;
        const auto at = x.attributes();
        if (x.name() == QLatin1String("build")) {
            dentro = hay_build = true;
        } else if (dentro && x.name() == QLatin1String("marco")) {
            marco = QRectF(num(at, "x"), num(at, "y"), num(at, "ancho"), num(at, "alto"));
        } else if (dentro && x.name() == QLatin1String("instancia")) {
            const QString fuente = at.value(QLatin1String("fuente")).toString();
            const Entrada* e = c.biblioteca()->por_fuente(fuente);
            if (!e) return falla(QStringLiteral("la biblioteca no tiene %1").arg(fuente));
            Instancia i = c.nueva(*e);
            i.id = at.value(QLatin1String("id")).toString();
            i.pos = QPointF(num(at, "x"), num(at, "y"));
            i.giro = int(num(at, "giro")) % 360;
            i.escala = num(at, "escala", 1.0);
            i.detalle = at.value(QLatin1String("dibujo")) == QLatin1String("detalle");
            for (const PlacaLeida::Componente& co : p.componentes)
                if (co.id == i.id)
                    for (auto it = co.atributos.cbegin(); it != co.atributos.cend(); ++it)
                        i.atributos.insert(it.key(), it.value());
            d.instancias.push_back(i);
        } else if (dentro && x.name() == QLatin1String("conexion")) {
            Conexion k;
            k.a = at.value(QLatin1String("a")).toString();
            k.b = at.value(QLatin1String("b")).toString();
            k.tramos = at.value(QLatin1String("trazado")) == QLatin1String("tramos");
            k.horizontal = at.value(QLatin1String("eje")) != QLatin1String("v");
            for (const QString& v : at.value(QLatin1String("codos")).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts))
                k.codos << v.toDouble();
            k.visible = at.value(QLatin1String("visible")) != QLatin1String("no");
            d.conexiones.push_back(k);
        }
    }
    if (x.hasError()) return falla(x.errorString());
    if (!hay_build) return falla(QStringLiteral("la placa no lleva <build>: no se hizo con mcu-sim-build"));

    // Cuántos de cada serie abierta: los que llegue a usar alguna conexión
    for (Instancia& i : d.instancias) {
        const FichaPieza* f = i.clase == Instancia::Pieza ? c.pieza(i.tipo) : nullptr;
        if (!f || f->series.isEmpty()) continue;
        int hasta = 0;
        for (const Conexion& k : d.conexiones)
            for (const QString& r : {k.a, k.b}) {
                if (id_de(r) != i.id) continue;
                const QString t = terminal_de(r);
                for (const Serie& s : f->series)
                    if (t.startsWith(s.prefijo)) {
                        bool ok = false;
                        const int n = t.mid(s.prefijo.size()).toInt(&ok);
                        if (ok) hasta = std::max(hasta, n - s.desde + 1);
                    }
            }
        i.n_serie = std::max(i.n_serie, hasta);
    }
    // Las redes que son bus: un terminal de cada una
    for (const PlacaLeida::Nodo& nd : p.nodos) {
        if (!nd.bus) continue;
        QString t;
        for (const PlacaLeida::Componente& co : p.componentes)
            for (const auto& [pin, nodo] : co.pines)
                if (t.isEmpty() && nodo == nd.id) t = co.id + QLatin1Char('.') + pin;
        for (const Instancia& i : d.instancias)
            if (t.isEmpty() && i.clase == Instancia::Mcu) {
                const QString pos = c.posicion_mcu(i, nd.id);
                if (!pos.isEmpty()) t = i.id + QLatin1Char('.') + pos;
            }
        if (t.isEmpty() && nd.id.contains(QLatin1Char('.'))) t = nd.id;   // un pin de conector
        if (!t.isEmpty()) d.bus << t;
    }
    // El marco es explícito solo si no es el automático
    if (!marco.isEmpty() && !parecidos(marco, marco_efectivo(d, c))) d.marco = marco;
    return d;
}

} // namespace mcusim::build
