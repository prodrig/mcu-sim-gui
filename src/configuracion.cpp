#include "configuracion.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace mcusim {

QStringList Configuracion::rutas_candidatas()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return {QDir::current().absoluteFilePath(QStringLiteral("config.json")),
            QDir(dir.isEmpty() ? QDir::homePath() : dir).absoluteFilePath(QStringLiteral("config.json"))};
}

QString Configuracion::ruta_por_omision()
{
    const QStringList c = rutas_candidatas();
    for (const QString& r : c)
        if (QFileInfo::exists(r)) return r;
    return c.last();          // ninguna: se guardará en la del usuario
}

QString Configuracion::absoluta(const QString& r) const
{
    if (r.isEmpty()) return r;
    if (QDir::isAbsolutePath(r)) return QDir::cleanPath(r);
    const QDir base = ruta.isEmpty() ? QDir::current() : QFileInfo(ruta).absoluteDir();
    return QDir::cleanPath(base.absoluteFilePath(r));
}

bool Configuracion::lee(const QString& ruta, Configuracion& c, QString& error)
{
    c = Configuracion();
    c.ruta = QFileInfo(ruta).absoluteFilePath();
    c.buscadas = {c.ruta};
    QFile f(ruta);
    if (!f.exists()) return true;
    c.existia = true;
    if (!f.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("no se puede abrir %1: %2").arg(ruta, f.errorString());
        return false;
    }
    QJsonParseError e{};
    const QJsonDocument d = QJsonDocument::fromJson(f.readAll(), &e);
    if (e.error != QJsonParseError::NoError || !d.isObject()) {
        error = QStringLiteral("%1 no es JSON valido: %2 (en el caracter %3)")
                    .arg(ruta, e.errorString()).arg(e.offset);
        return false;
    }
    const QJsonObject r = d.object();
    const QJsonObject m = r.value(QStringLiteral("mcu_sim")).toObject();
    c.ejecutable = m.value(QStringLiteral("ejecutable")).toString();
    c.directorio = m.value(QStringLiteral("directorio_de_trabajo")).toString();
    const QJsonObject a = m.value(QStringLiteral("argumentos")).toObject();
    for (auto it = a.begin(); it != a.end(); ++it) {
        if (it.key().startsWith(QLatin1Char('_'))) continue;      // comentarios
        const QJsonValue v = it.value();
        QString s;
        if (v.isBool())        s = v.toBool() ? QStringLiteral("si") : QString();
        else if (v.isDouble()) s = QString::number(v.toDouble());
        else if (v.isArray()) {
            QStringList l;
            for (const QJsonValue& x : v.toArray()) l << x.toString();
            s = l.join(QLatin1Char(';'));
        } else s = v.toString();
        if (!s.isEmpty()) c.argumentos.insert(it.key(), s);
    }
    c.a_mano = m.value(QStringLiteral("a_mano")).toString();
    const QJsonObject en = r.value(QStringLiteral("enlace")).toObject();
    c.host = en.value(QStringLiteral("host")).toString(c.host);
    const int p = en.value(QStringLiteral("puerto")).toInt(c.puerto);
    if (p >= 0 && p <= 65535) c.puerto = quint16(p);
    const QJsonObject vi = r.value(QStringLiteral("vista")).toObject();
    c.ritmo = vi.value(QStringLiteral("ritmo")).toString(c.ritmo);
    const double per = vi.value(QStringLiteral("periodo_ms")).toDouble(c.periodo_ms);
    if (per > 0) c.periodo_ms = per;
    c.siempre_encima = vi.value(QStringLiteral("siempre_encima")).toBool(c.siempre_encima);
    return true;
}

bool Configuracion::guarda(QString& error) const
{
    // Sobre lo que ya hubiera: así sobreviven los "_comentario" de la
    // plantilla y cualquier clave que esta versión no conozca.
    QJsonObject r;
    {
        QFile viejo(ruta);
        if (viejo.open(QIODevice::ReadOnly)) {
            const QJsonDocument d = QJsonDocument::fromJson(viejo.readAll());
            if (d.isObject()) r = d.object();
        }
    }
    auto pon = [&r](const QString& sec, const QString& k, const QJsonValue& v) {
        QJsonObject o = r.value(sec).toObject();
        o.insert(k, v);
        r.insert(sec, o);
    };
    QJsonObject args;
    {
        const QJsonObject viejos = r.value(QStringLiteral("mcu_sim")).toObject()
                                       .value(QStringLiteral("argumentos")).toObject();
        for (auto it = viejos.begin(); it != viejos.end(); ++it)
            if (it.key().startsWith(QLatin1Char('_'))) args.insert(it.key(), it.value());
    }
    for (auto it = argumentos.begin(); it != argumentos.end(); ++it)
        if (!it.value().isEmpty()) args.insert(it.key(), it.value());
    pon(QStringLiteral("mcu_sim"), QStringLiteral("ejecutable"), ejecutable);
    pon(QStringLiteral("mcu_sim"), QStringLiteral("directorio_de_trabajo"), directorio);
    pon(QStringLiteral("mcu_sim"), QStringLiteral("argumentos"), args);
    pon(QStringLiteral("mcu_sim"), QStringLiteral("a_mano"), a_mano);
    pon(QStringLiteral("enlace"), QStringLiteral("host"), host);
    pon(QStringLiteral("enlace"), QStringLiteral("puerto"), int(puerto));
    pon(QStringLiteral("vista"), QStringLiteral("ritmo"), ritmo);
    pon(QStringLiteral("vista"), QStringLiteral("periodo_ms"), periodo_ms);
    pon(QStringLiteral("vista"), QStringLiteral("siempre_encima"), siempre_encima);
    QDir().mkpath(QFileInfo(ruta).absolutePath());
    QSaveFile f(ruta);
    if (!f.open(QIODevice::WriteOnly)) {
        error = QStringLiteral("no se puede escribir %1: %2").arg(ruta, f.errorString());
        return false;
    }
    f.write(QJsonDocument(r).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        error = QStringLiteral("no se puede escribir %1: %2").arg(ruta, f.errorString());
        return false;
    }
    return true;
}

} // namespace mcusim
