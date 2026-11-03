// =============================================================================
// escena3d.cpp — la vista 3D de una placa, resuelta contra el catálogo
// (véase escena3d.h)
// =============================================================================
#include "escena3d.h"

#include <QRegularExpression>

#include <algorithm>

namespace mcusim {

namespace {

QVector<double> numeros(const QString& t, bool* ok = nullptr)
{
    QVector<double> v;
    bool bien = true;
    for (const QString& x : t.split(QRegularExpression(QStringLiteral("\\s+")),
                                    Qt::SkipEmptyParts)) {
        bool o = false;
        v.push_back(x.toDouble(&o));
        bien = bien && o;
    }
    if (ok) *ok = bien;
    return v;
}

Vec3 vec3(const QString& t, const Vec3& omision = Vec3())
{
    bool ok = false;
    const QVector<double> v = numeros(t, &ok);
    if (!ok || v.size() != 3) return omision;
    return Vec3{v[0], v[1], v[2]};
}

const PiezaGui* busca_pieza(const PlacaGui& p, const QString& id)
{
    for (const PiezaGui& x : p.piezas)
        if (x.id == id) return &x;
    return nullptr;
}

} // namespace

Escena3d::Escena3d(const PlacaGui& p, QObject* padre) : QObject(padre)
{
    for (const Vista3dGui& v : p.vistas3d) {
        const QString pref = v.placa.isEmpty() ? QString() : v.placa + QLatin1Char('/');
        if (!hay_camara && !v.camara.isEmpty() && !v.mira.isEmpty()) {
            hay_camara = true;
            camara = vec3(v.camara);
            mira = vec3(v.mira);
        }
        for (const Elemento3dGui& e : v.elementos) {
            const QString nombre_pieza = e.attrs.value(QStringLiteral("pieza"));
            const QString pieza = nombre_pieza.isEmpty() ? QString() : pref + nombre_pieza;
            const PiezaGui* pz = pieza.isEmpty() ? nullptr : busca_pieza(p, pieza);
            if (!pieza.isEmpty() && !pz)
                avisos << tr("%1: la pieza %2 no esta en el catalogo").arg(e.etiqueta, pieza);
            // Los observables que lo mueven, con su vector
            QVector<quint16> obs;
            QVector<Vec3> vec;
            {
                const QStringList nombres = e.attrs.value(QStringLiteral("observable"))
                                                .split(QLatin1Char(' '), Qt::SkipEmptyParts);
                const QVector<double> vv = numeros(e.attrs.value(QStringLiteral("vector")));
                for (int k = 0; k < nombres.size() && 3 * k + 2 < vv.size() && pz; ++k) {
                    int id = -1;
                    for (const ObservableGui& o : pz->observables)
                        if (o.nombre == nombres[k]) id = o.id_obs;
                    if (id < 0) {
                        avisos << tr("%1: %2 no tiene el observable %3: no lo mueve")
                                      .arg(e.etiqueta, pieza, nombres[k]);
                        continue;
                    }
                    obs.push_back(quint16(id));
                    vec.push_back(Vec3{vv[3 * k], vv[3 * k + 1], vv[3 * k + 2]});
                    mio_.insert(quint16(id), true);
                }
            }
            const QString color = e.attrs.value(QStringLiteral("color"), QStringLiteral("#9e9e9e"));
            if (e.etiqueta == QLatin1String("solido")) {
                Solido3d s;
                s.cilindro = e.attrs.value(QStringLiteral("forma")) == QLatin1String("cilindro");
                const QVector<double> t = numeros(e.attrs.value(QStringLiteral("tam")));
                if (t.size() != (s.cilindro ? 2 : 3)) {
                    avisos << tr("solido: tam=\"%1\" no vale").arg(e.attrs.value(QStringLiteral("tam")));
                    continue;
                }
                s.tam = Vec3{t[0], t[1], s.cilindro ? 0.0 : t[2]};
                const QString eje = e.attrs.value(QStringLiteral("eje"), QStringLiteral("z"));
                s.eje = eje.isEmpty() ? 'z' : eje[0].toLatin1();
                s.en = vec3(e.attrs.value(QStringLiteral("en")));
                s.color = color;
                s.pieza = pieza;
                s.obs = obs;
                s.vec = vec;
                const QString gira = e.attrs.value(QStringLiteral("gira"));
                if (!gira.isEmpty() && pz) {
                    for (const ObservableGui& o : pz->observables)
                        if (o.nombre == gira) s.gira = o.id_obs;
                    if (s.gira < 0)
                        avisos << tr("solido: %1 no tiene el observable %2: no gira")
                                      .arg(pieza, gira);
                    else
                        mio_.insert(quint16(s.gira), true);
                }
                solidos.push_back(s);
            } else {
                Superficie3d s;
                s.pieza = pieza;
                const QVector<double> t = numeros(e.attrs.value(QStringLiteral("tam")));
                if (t.size() != 2) {
                    avisos << tr("superficie: tam=\"%1\" no vale").arg(e.attrs.value(QStringLiteral("tam")));
                    continue;
                }
                s.largo = t[0];
                s.ancho = t[1];
                s.en = vec3(e.attrs.value(QStringLiteral("en")));
                s.color = color;
                s.obs = obs;
                s.vec = vec;
                const QString img = e.attrs.value(QStringLiteral("imagen"));
                if (pz)
                    for (const ImagenGui& im : pz->imagenes)
                        if (im.nombre == img) {
                            s.id_img = im.id_obs;
                            s.nx = im.ancho;
                            s.ny = im.alto;
                        }
                if (s.id_img < 0 && pz)
                    avisos << tr("superficie: %1 no tiene la imagen %2: no se dibuja").arg(pieza, img);
                superficies.push_back(s);
            }
        }
    }
}

QVector<quint16> Escena3d::observados(bool con_imagenes) const
{
    QVector<quint16> ids;
    for (auto i = mio_.constBegin(); i != mio_.constEnd(); ++i) ids.push_back(i.key());
    if (con_imagenes)
        for (const Superficie3d& s : superficies)
            if (s.id_img >= 0 && !ids.contains(quint16(s.id_img))) ids.push_back(quint16(s.id_img));
    std::sort(ids.begin(), ids.end());
    return ids;
}

void Escena3d::pon_valor(quint16 id, float v)
{
    if (!mio_.contains(id)) return;
    const auto i = valor_.constFind(id);
    if (i != valor_.constEnd() && *i == v) return;
    valor_.insert(id, v);
    emit movida();
}

void Escena3d::pon_imagen(quint16 id, int ancho, int alto, const QByteArray& rgb)
{
    for (int k = 0; k < superficies.size(); ++k) {
        Superficie3d& s = superficies[k];
        if (s.id_img != int(id)) continue;
        if (ancho <= 0 || alto <= 0 || rgb.size() < 3 * ancho * alto) return;
        s.nx = ancho;
        s.ny = alto;
        s.alturas.resize(ancho * alto);
        s.paso.resize(ancho * alto);
        const uchar* d = reinterpret_cast<const uchar*>(rgb.constData());
        for (int fila = 0; fila < alto; ++fila) {
            const int j = alto - 1 - fila;                  // la de arriba, la de más Y
            for (int i = 0; i < ancho; ++i) {
                const uchar* px = d + 3 * (fila * ancho + i);
                const float h = float((unsigned(px[0]) << 8) | px[1]) / 100.f;
                s.alturas[j * ancho + i] = h;
                s.paso[j * ancho + i] = px[2] ? 1 : 0;
                if (h > s.maxima) s.maxima = h;
            }
        }
        ++s.version;
        emit superficie(k);
    }
}

Vec3 Escena3d::mueve(const Vec3& en, const QVector<quint16>& obs, const QVector<Vec3>& vec) const
{
    Vec3 r = en;
    for (int k = 0; k < obs.size(); ++k) {
        const double v = valor_.value(obs[k]);
        r.x += v * vec[k].x;
        r.y += v * vec[k].y;
        r.z += v * vec[k].z;
    }
    return r;
}

} // namespace mcusim
