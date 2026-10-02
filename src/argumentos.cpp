#include "argumentos.h"

#include <QXmlStreamReader>

namespace mcusim {

bool lee_argumentos(const QByteArray& salida, ArgumentosCli& a, QString& error)
{
    a = ArgumentosCli();
    const int i = salida.indexOf("<argumentos");
    if (i < 0) {
        error = QStringLiteral("no contesta con una lista de opciones: no es un mcu-sim, o "
                               "es uno anterior a --argumentos");
        return false;
    }
    QXmlStreamReader x(salida.mid(i));
    OpcionCli* actual = nullptr;
    while (!x.atEnd()) {
        x.readNext();
        if (x.isStartElement()) {
            const auto at = x.attributes();
            const QString n = x.name().toString();
            if (n == QLatin1String("argumentos")) {
                a.programa = at.value(QLatin1String("programa")).toString();
                a.version  = at.value(QLatin1String("version")).toString();
            } else if (n == QLatin1String("posicional")) {
                PosicionalCli p;
                p.nombre      = at.value(QLatin1String("nombre")).toString();
                p.tipo        = at.value(QLatin1String("tipo")).toString();
                p.filtro      = at.value(QLatin1String("filtro")).toString();
                p.ayuda       = at.value(QLatin1String("ayuda")).toString();
                p.obligatorio = at.value(QLatin1String("obligatorio")) == QLatin1String("si");
                if (p.nombre.isEmpty()) { error = QStringLiteral("un <posicional> sin nombre"); return false; }
                a.posicionales.push_back(p);
            } else if (n == QLatin1String("opcion")) {
                OpcionCli o;
                o.nombre    = at.value(QLatin1String("nombre")).toString();
                o.forma     = at.value(QLatin1String("forma")).toString();
                o.tipo      = at.value(QLatin1String("tipo")).toString();
                o.omision   = at.value(QLatin1String("omision")).toString();
                o.unidad    = at.value(QLatin1String("unidad")).toString();
                o.grupo     = at.value(QLatin1String("grupo")).toString();
                o.ejemplo   = at.value(QLatin1String("ejemplo")).toString();
                o.ayuda     = at.value(QLatin1String("ayuda")).toString();
                o.repetible = at.value(QLatin1String("repetible")) == QLatin1String("si");
                o.con_gui   = at.value(QLatin1String("con_gui")) != QLatin1String("no");
                if (!o.nombre.startsWith(QLatin1String("--"))) {
                    error = QStringLiteral("una <opcion> cuyo nombre no empieza por --: '%1'")
                                .arg(o.nombre);
                    return false;
                }
                a.opciones.push_back(o);
                actual = &a.opciones.back();
            } else if (n == QLatin1String("valor") && actual) {
                actual->valores.push_back(x.readElementText());
            }
        } else if (x.isEndElement()) {
            if (x.name() == QLatin1String("opcion")) actual = nullptr;
            if (x.name() == QLatin1String("argumentos")) break;
        }
    }
    if (x.hasError() && x.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
        error = QStringLiteral("la lista de opciones no es XML valido: %1").arg(x.errorString());
        return false;
    }
    if (a.posicionales.isEmpty()) {
        error = QStringLiteral("la lista de opciones no trae ningun argumento posicional");
        return false;
    }
    return true;
}

QStringList trocea(const QString& texto)
{
    QStringList r;
    QString actual;
    bool hay = false;
    QChar comilla;
    for (const QChar c : texto) {
        if (!comilla.isNull()) {
            if (c == comilla) comilla = QChar();
            else actual += c;
        } else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            comilla = c;
            hay = true;
        } else if (c.isSpace()) {
            if (hay) r << actual;
            actual.clear();
            hay = false;
        } else {
            actual += c;
            hay = true;
        }
    }
    if (hay) r << actual;
    return r;
}

QStringList linea_de_ordenes(const ArgumentosCli& a, const ValoresCli& v,
                             const QString& a_mano, QString* error)
{
    QStringList r;
    QStringList pos;
    // Los posicionales, en su orden. Uno vacío corta: el siguiente ya no
    // estaría en su sitio.
    const QStringList nombres_pos =
        a.posicionales.isEmpty() ? QStringList{QStringLiteral("placa"), QStringLiteral("firmware")}
                                 : [&] {
                                       QStringList l;
                                       for (const PosicionalCli& p : a.posicionales) l << p.nombre;
                                       return l;
                                   }();
    for (int i = 0; i < nombres_pos.size(); ++i) {
        const QString val = v.value(nombres_pos[i]).trimmed();
        const bool obligatorio =
            a.posicionales.isEmpty() ? i == 0 : a.posicionales[i].obligatorio;
        if (val.isEmpty()) {
            if (obligatorio) {
                if (error) *error = QStringLiteral("falta '%1'").arg(nombres_pos[i]);
                return {};
            }
            break;
        }
        pos << val;
    }
    r << pos;
    for (const OpcionCli& o : a.opciones) {
        if (o.forma == QLatin1String("accion")) continue;
        const QString val = v.value(o.nombre).trimmed();
        if (val.isEmpty()) continue;
        if (o.forma == QLatin1String("bandera")) {
            if (val == QLatin1String("si")) r << o.nombre;
            continue;
        }
        const QStringList vals = o.repetible ? val.split(QLatin1Char(';'), Qt::SkipEmptyParts)
                                             : QStringList{val};
        for (const QString& x : vals) r << o.nombre + QLatin1Char('=') + x.trimmed();
    }
    r << trocea(a_mano);
    if (error) error->clear();
    return r;
}

QString como_texto(const QString& programa, const QStringList& args)
{
    QStringList t{programa};
    for (const QString& s : args)
        t << ((s.isEmpty() || s.contains(QLatin1Char(' ')) || s.contains(QLatin1Char('"')))
                  ? QLatin1Char('\'') + s + QLatin1Char('\'')
                  : s);
    return t.join(QLatin1Char(' '));
}

} // namespace mcusim
