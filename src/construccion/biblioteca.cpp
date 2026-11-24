#include "biblioteca.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QXmlStreamReader>
#include <QHash>
#include <QSet>

#include <algorithm>

namespace mcusim::build {

namespace {

const char* nombre_clase(Entrada::Clase c)
{
    switch (c) {
    case Entrada::Pieza: return "pieza";
    case Entrada::Mcu: return "mcu";
    case Entrada::Perfil: return "perfil";
    case Entrada::Conjunto: return "conjunto";
    case Entrada::Tarjeta: return "tarjeta";
    case Entrada::Sistema: return "sistema";
    }
    return "pieza";
}

Entrada::Clase clase_de(const QString& s)
{
    for (int c = Entrada::Pieza; c <= Entrada::Sistema; ++c)
        if (s == QLatin1String(nombre_clase(Entrada::Clase(c)))) return Entrada::Clase(c);
    return Entrada::Pieza;
}

// Sin los comentarios: algunas placas llevan «--» dentro, que mcu-sim acepta
// y un lector de XML estricto no
QByteArray sin_comentarios(QByteArray t)
{
    for (int i = t.indexOf("<!--"); i >= 0; i = t.indexOf("<!--", i)) {
        const int j = t.indexOf("-->", i + 4);
        if (j < 0) { t.truncate(i); break; }
        t.remove(i, j + 3 - i);
    }
    return t;
}

// La clase de un fichero por su nombre; los .xml sueltos, por su raíz
bool clasifica(const QString& ruta, Entrada::Clase& c, QString& nombre)
{
    const QString fn = QFileInfo(ruta).fileName();
    struct { const char* suf; Entrada::Clase c; } tabla[] = {
        {".pieza.xml", Entrada::Pieza}, {".mcu.xml", Entrada::Mcu},
        {".conector.xml", Entrada::Perfil}, {".conjunto.xml", Entrada::Conjunto}};
    for (const auto& t : tabla)
        if (fn.endsWith(QLatin1String(t.suf))) {
            c = t.c;
            nombre = fn.left(fn.size() - int(qstrlen(t.suf)));
            return true;
        }
    if (fn.endsWith(QLatin1String(".forma.xml")) || !fn.endsWith(QLatin1String(".xml"))) return false;
    // Una placa o un sistema: la raíz lo dice
    QFile f(ruta);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QXmlStreamReader x(sin_comentarios(f.read(256 * 1024)));
    while (!x.atEnd()) {
        x.readNext();
        if (x.isStartElement()) {
            nombre = QFileInfo(ruta).completeBaseName();
            if (x.name() == QLatin1String("placa")) { c = Entrada::Tarjeta; return true; }
            if (x.name() == QLatin1String("sistema")) { c = Entrada::Sistema; return true; }
            return false;
        }
    }
    return false;
}

QString resumen_de(const QByteArray& texto, Entrada::Clase c)
{
    QXmlStreamReader x(sin_comentarios(texto));
    while (!x.atEnd()) {
        x.readNext();
        if (!x.isStartElement()) continue;
        const auto at = x.attributes();
        switch (c) {
        case Entrada::Pieza:
            if (x.name() == QLatin1String("resumen")) return x.readElementText().simplified();
            break;
        case Entrada::Mcu:
            return QStringLiteral("%1, %2 (%3)").arg(at.value(QLatin1String("familia")).toString(),
                                                     at.value(QLatin1String("encapsulado")).toString(),
                                                     at.value(QLatin1String("fuente")).toString().section(QLatin1Char('@'), 0, 0));
        case Entrada::Perfil:
        case Entrada::Conjunto:
            return at.value(QLatin1String("titulo")).toString();
        case Entrada::Tarjeta:
        case Entrada::Sistema:
            return at.value(QLatin1String("nombre")).toString();
        }
    }
    return {};
}

} // namespace

Biblioteca::Biblioteca(const QString& datos) : datos_(datos)
{
    if (datos_.isEmpty())
        datos_ = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString Biblioteca::ruta_indice() const
{
    return QDir(datos_).filePath(QStringLiteral("biblioteca.json"));
}

void Biblioteca::pon_carpetas(const QStringList& c)
{
    carpetas_.clear();
    for (const QString& x : c) anade_carpeta(x);
}

void Biblioteca::anade_carpeta(const QString& c)
{
    const QString a = QDir::cleanPath(QFileInfo(c).absoluteFilePath());
    if (!carpetas_.contains(a)) carpetas_ << a;
}

void Biblioteca::quita_carpeta(const QString& c)
{
    carpetas_.removeAll(QDir::cleanPath(QFileInfo(c).absoluteFilePath()));
}

bool Biblioteca::lee(const QString& ruta, const QString& carpeta, Entrada& e)
{
    Entrada::Clase c;
    QString nombre;
    if (!clasifica(ruta, c, nombre)) return false;
    QFile f(ruta);
    if (!f.open(QIODevice::ReadOnly)) {
        errores_ << QStringLiteral("%1: no se puede leer").arg(ruta);
        return false;
    }
    const QByteArray t = f.readAll();
    ++leidos_;
    const QFileInfo fi(ruta);
    e.clase = c;
    e.nombre = nombre;
    e.ruta = ruta;
    e.carpeta = carpeta;
    QString rel = QDir(carpeta).relativeFilePath(ruta);
    for (const char* suf : {".pieza.xml", ".mcu.xml", ".conector.xml", ".conjunto.xml", ".xml"})
        if (rel.endsWith(QLatin1String(suf))) { rel.chop(int(qstrlen(suf))); break; }
    e.fuente = rel;
    e.resumen = resumen_de(t, c);
    e.mtime = fi.lastModified().toMSecsSinceEpoch();
    e.tamano = fi.size();
    e.huella = QString::fromLatin1(QCryptographicHash::hash(t, QCryptographicHash::Sha1).toHex());
    e.desaparecida = false;
    return true;
}

bool Biblioteca::carga_indice()
{
    QFile f(ruta_indice());
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    if (o.value(QStringLiteral("version")).toInt() != 1) return false;
    if (carpetas_.isEmpty())
        for (const QJsonValue& v : o.value(QStringLiteral("carpetas")).toArray()) carpetas_ << v.toString();
    entradas_.clear();
    for (const QJsonValue& v : o.value(QStringLiteral("entradas")).toArray()) {
        const QJsonObject x = v.toObject();
        Entrada e;
        e.clase = clase_de(x.value(QStringLiteral("clase")).toString());
        e.nombre = x.value(QStringLiteral("nombre")).toString();
        e.ruta = x.value(QStringLiteral("ruta")).toString();
        e.carpeta = x.value(QStringLiteral("carpeta")).toString();
        e.fuente = x.value(QStringLiteral("fuente")).toString();
        e.resumen = x.value(QStringLiteral("resumen")).toString();
        e.mtime = qint64(x.value(QStringLiteral("mtime")).toDouble());
        e.tamano = qint64(x.value(QStringLiteral("tamano")).toDouble());
        e.huella = x.value(QStringLiteral("huella")).toString();
        e.desaparecida = x.value(QStringLiteral("desaparecida")).toBool();
        entradas_.push_back(e);
    }
    return true;
}

bool Biblioteca::guarda() const
{
    QDir().mkpath(datos_);
    QJsonArray cs, es;
    for (const QString& c : carpetas_) cs.append(c);
    for (const Entrada& e : entradas_) {
        QJsonObject x{{QStringLiteral("clase"), QString::fromLatin1(nombre_clase(e.clase))},
                      {QStringLiteral("nombre"), e.nombre},
                      {QStringLiteral("ruta"), e.ruta},
                      {QStringLiteral("carpeta"), e.carpeta},
                      {QStringLiteral("fuente"), e.fuente},
                      {QStringLiteral("resumen"), e.resumen},
                      {QStringLiteral("mtime"), double(e.mtime)},
                      {QStringLiteral("tamano"), double(e.tamano)},
                      {QStringLiteral("huella"), e.huella}};
        if (e.desaparecida) x.insert(QStringLiteral("desaparecida"), true);
        es.append(x);
    }
    QFile f(ruta_indice());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(QJsonObject{{QStringLiteral("version"), 1},
                                      {QStringLiteral("carpetas"), cs},
                                      {QStringLiteral("entradas"), es}}).toJson());
    return true;
}

int Biblioteca::abre()
{
    carga_indice();
    return refresca(false);
}

int Biblioteca::refresca(bool completo)
{
    leidos_ = 0;
    errores_.clear();
    QHash<QString, int> por_ruta;
    for (int i = 0; i < entradas_.size(); ++i) por_ruta.insert(entradas_[i].ruta, i);
    QVector<Entrada> nuevas;
    QSet<QString> vistas;
    for (const QString& carpeta : std::as_const(carpetas_)) {
        if (!QFileInfo(carpeta).isDir()) {
            errores_ << QStringLiteral("%1: la carpeta no está").arg(carpeta);
            // Lo suyo se queda, marcado (salvo en un refresco completo)
            continue;
        }
        QDirIterator it(carpeta, {QStringLiteral("*.xml")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString ruta = QDir::cleanPath(it.next());
            const QFileInfo fi(ruta);
            const int i = por_ruta.value(ruta, -1);
            if (!completo && i >= 0 && entradas_[i].carpeta == carpeta &&
                entradas_[i].mtime == fi.lastModified().toMSecsSinceEpoch() &&
                entradas_[i].tamano == fi.size()) {
                Entrada e = entradas_[i];
                e.desaparecida = false;
                nuevas.push_back(e);
                vistas.insert(ruta);
                continue;
            }
            Entrada e;
            if (lee(ruta, carpeta, e)) {
                nuevas.push_back(e);
                vistas.insert(ruta);
            }
        }
    }
    if (!completo)
        for (Entrada e : std::as_const(entradas_))
            if (!vistas.contains(e.ruta)) {
                e.desaparecida = true;
                nuevas.push_back(e);
            }
    std::sort(nuevas.begin(), nuevas.end(), [](const Entrada& a, const Entrada& b) {
        return a.clase != b.clase ? a.clase < b.clase
                                  : a.nombre.compare(b.nombre, Qt::CaseInsensitive) < 0;
    });
    entradas_ = nuevas;
    guarda();
    return leidos_;
}

QVector<Entrada> Biblioteca::piezas() const
{
    QVector<Entrada> v;
    for (const Entrada& e : entradas_)
        if (e.de_piezas()) v.push_back(e);
    return v;
}

QVector<Entrada> Biblioteca::tarjetas() const
{
    QVector<Entrada> v;
    for (const Entrada& e : entradas_)
        if (e.clase == Entrada::Tarjeta) v.push_back(e);
    return v;
}

const Entrada* Biblioteca::busca(Entrada::Clase c, const QString& nombre) const
{
    for (const Entrada& e : entradas_)
        if (e.clase == c && e.nombre == nombre && !e.desaparecida) return &e;
    return nullptr;
}

const Entrada* Biblioteca::por_fuente(const QString& fuente) const
{
    for (const Entrada& e : entradas_)
        if (e.fuente == fuente && !e.desaparecida) return &e;
    // Por nombre, si la fuente es de otra carpeta: «piezas/Led» sirve para
    // cualquier biblioteca que tenga un Led
    const QString n = fuente.section(QLatin1Char('/'), -1);
    for (const Entrada& e : entradas_)
        if (e.nombre == n && !e.desaparecida && e.de_piezas()) return &e;
    return nullptr;
}

QString Biblioteca::svg_de(const Entrada& e) const
{
    const QFileInfo fi(e.ruta);
    const QDir d = fi.dir();
    switch (e.clase) {
    case Entrada::Pieza: return d.filePath(e.nombre + QStringLiteral(".svg"));
    case Entrada::Mcu: return d.filePath(e.nombre + QStringLiteral(".simple.svg"));
    case Entrada::Perfil:
    case Entrada::Conjunto:
    case Entrada::Tarjeta:
    case Entrada::Sistema: return d.filePath(e.nombre + QStringLiteral(".svg"));
    }
    return {};
}

QImage Biblioteca::miniatura(const Entrada& e, int px) const
{
    const QString svg = svg_de(e);
    if (!QFileInfo::exists(svg)) return {};
    const QString cache = QDir(datos_).filePath(QStringLiteral("miniaturas"));
    const QString clave = QString::fromLatin1(QCryptographicHash::hash(
        (svg + QString::number(QFileInfo(svg).lastModified().toMSecsSinceEpoch()) +
         QString::number(px)).toUtf8(), QCryptographicHash::Sha1).toHex());
    const QString png = QDir(cache).filePath(clave + QStringLiteral(".png"));
    QImage img;
    if (img.load(png)) return img;
    QSvgRenderer r(svg);
    if (!r.isValid()) return {};
    QSizeF t = r.viewBoxF().size();
    if (t.isEmpty()) t = QSizeF(px, px);
    t.scale(px, px, Qt::KeepAspectRatio);
    img = QImage(std::max(1, int(t.width())), std::max(1, int(t.height())), QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        r.render(&p);
    }
    QDir().mkpath(cache);
    img.save(png);
    return img;
}

} // namespace mcusim::build
