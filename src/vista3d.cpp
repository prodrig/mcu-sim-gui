// =============================================================================
// vista3d.cpp — la ventana 3D (véase vista3d.h)
// =============================================================================
#include "vista3d.h"

#include <QColor>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QQuick3DGeometry>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QVector3D>

#include <cmath>
#include <cstring>

#include "escena3d.h"

namespace mcusim {

namespace {

// De la máquina, con Z hacia arriba, a Qt Quick 3D, con Y hacia arriba
QVector3D q3d(const Vec3& v) { return QVector3D(float(v.x), float(v.z), float(-v.y)); }

} // namespace

// -----------------------------------------------------------------------------
// LA MALLA DE UNA SUPERFICIE: un vértice por celda, en su centro y a su
// altura, y alrededor un anillo a la altura de la base, que hace de costados.
// Cada vértice lleva su normal -de las alturas de sus vecinos- y su color.
class SuperficieGeo : public QQuick3DGeometry {
public:
    // false si aún no hay alturas que dibujar
    bool pon(const Superficie3d& s)
    {
        const int nx = s.nx, ny = s.ny;
        if (nx < 2 || ny < 2 || s.alturas.size() < nx * ny) return false;
        const int gx = nx + 2, gy = ny + 2;            // con el anillo
        const float cx = float(s.largo / nx), cy = float(s.ancho / ny);
        const QColor base(s.color);
        const float br = float(base.redF()), bg = float(base.greenF()), bb = float(base.blueF());
        const float grueso = s.maxima > 0 ? s.maxima : 1.f;
        // La altura y la posición de cada punto de la rejilla con su anillo
        auto h_de = [&](int i, int j) -> float {
            if (i <= 0 || j <= 0 || i >= gx - 1 || j >= gy - 1) return 0.f;
            return s.alturas[(j - 1) * nx + (i - 1)];
        };
        auto x_de = [&](int i) {
            if (i <= 0) return 0.f;
            if (i >= gx - 1) return float(s.largo);
            return (float(i) - 0.5f) * cx;
        };
        auto y_de = [&](int j) {
            if (j <= 0) return 0.f;
            if (j >= gy - 1) return float(s.ancho);
            return (float(j) - 0.5f) * cy;
        };
        constexpr int F = 10;                          // floats por vértice
        QByteArray vb;
        vb.resize(gx * gy * F * int(sizeof(float)));
        float* v = reinterpret_cast<float*>(vb.data());
        for (int j = 0; j < gy; ++j)
            for (int i = 0; i < gx; ++i) {
                const float h = h_de(i, j);
                const float x = x_de(i), y = y_de(j);
                // La normal, con las diferencias centrales (en ejes de la máquina)
                const int i0 = std::max(0, i - 1), i1 = std::min(gx - 1, i + 1);
                const int j0 = std::max(0, j - 1), j1 = std::min(gy - 1, j + 1);
                const float dx = std::max(1e-3f, x_de(i1) - x_de(i0));
                const float dy = std::max(1e-3f, y_de(j1) - y_de(j0));
                const float hx = (h_de(i1, j) - h_de(i0, j)) / dx;
                const float hy = (h_de(i, j1) - h_de(i, j0)) / dy;
                QVector3D n(-hx, 1.f, hy);             // (-dh/dx, 1, -(-dh/dy)) en Qt Quick 3D
                n.normalize();
                // El color, como el de la pieza en el dibujo
                float r = br, g = bg, b = bb;
                const bool anillo = i == 0 || j == 0 || i == gx - 1 || j == gy - 1;
                if (anillo) {
                    r *= 0.7f; g *= 0.7f; b *= 0.7f;
                } else {
                    const int k = (j - 1) * nx + (i - 1);
                    const float d = grueso - h;
                    if (h <= 1e-4f) {
                        r = 0.27f; g = 0.28f; b = 0.31f;              // de lado a lado: la mesa
                    } else if (d > 1e-3f) {
                        const float f = 0.82f - 0.5f * (d / grueso);  // más oscuro cuanto más hondo
                        r *= f; g *= f; b *= f;
                    } else if (s.paso.value(k)) {
                        r = 0.7f * r + 0.3f * 0.35f; g = 0.7f * g + 0.3f * 0.55f;
                        b = 0.7f * b + 0.3f * 0.86f;
                    }
                }
                float* p = v + (j * gx + i) * F;
                p[0] = x; p[1] = h; p[2] = -y;
                p[3] = n.x(); p[4] = n.y(); p[5] = n.z();
                p[6] = r; p[7] = g; p[8] = b; p[9] = 1.f;
            }
        QByteArray ib;
        ib.resize((gx - 1) * (gy - 1) * 6 * int(sizeof(quint32)));
        quint32* t = reinterpret_cast<quint32*>(ib.data());
        for (int j = 0; j < gy - 1; ++j)
            for (int i = 0; i < gx - 1; ++i) {
                const quint32 a = quint32(j * gx + i), b = a + 1, c = a + quint32(gx), d = c + 1;
                // En sentido contrario a las agujas visto desde arriba (+Y de Qt)
                *t++ = a; *t++ = b; *t++ = c;
                *t++ = b; *t++ = d; *t++ = c;
            }
        clear();
        setStride(F * int(sizeof(float)));
        setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
        addAttribute(QQuick3DGeometry::Attribute::PositionSemantic, 0,
                     QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::NormalSemantic, 3 * int(sizeof(float)),
                     QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::ColorSemantic, 6 * int(sizeof(float)),
                     QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::IndexSemantic, 0,
                     QQuick3DGeometry::Attribute::U32Type);
        setVertexData(vb);
        setIndexData(ib);
        setBounds(QVector3D(0, 0, -float(s.ancho)), QVector3D(float(s.largo), grueso, 0));
        update();
        return true;
    }
};

// -----------------------------------------------------------------------------
Vista3d::Vista3d(Escena3d* escena, QWidget* padre) : QWidget(padre), esc_(escena)
{
    auto* c = new QVBoxLayout(this);
    c->setContentsMargins(0, 0, 0, 0);
    c->setSpacing(0);
    auto* barra = new QHBoxLayout;
    barra->setContentsMargins(6, 4, 6, 4);
    auto* inicial = new QPushButton(tr("Vista inicial"), this);
    inicial->setObjectName(QStringLiteral("vista_inicial"));
    inicial->setToolTip(tr("La camara como al principio. Arrastrar gira, la rueda acerca y "
                           "aleja, y con el boton derecho se desplaza."));
    connect(inicial, &QPushButton::clicked, this, &Vista3d::vista_inicial);
    barra->addWidget(inicial);
    aviso_ = new QLabel(this);
    aviso_->setObjectName(QStringLiteral("avisos3d"));
    barra->addWidget(aviso_, 1);
    c->addLayout(barra);

    qw_ = new QQuickWidget(this);
    qw_->setObjectName(QStringLiteral("quick3d"));
    qw_->setResizeMode(QQuickWidget::SizeRootObjectToView);
    qw_->setSource(QUrl(QStringLiteral("qrc:/mcu-sim-gui/escena3d.qml")));
    c->addWidget(qw_, 1);
    if (qw_->status() != QQuickWidget::Ready) {
        QStringList e;
        for (const QQmlError& x : qw_->errors()) e << x.toString();
        error_ = tr("no se puede cargar la escena de Qt Quick 3D: %1").arg(e.join(QStringLiteral("; ")));
        aviso_->setText(error_);
        return;
    }
    rehacer_ = new QTimer(this);
    rehacer_->setSingleShot(true);
    rehacer_->setInterval(100);                      // como mucho, diez veces por segundo
    connect(rehacer_, &QTimer::timeout, this, &Vista3d::rehaz_superficies);
    construye();
    if (!esc_->avisos.isEmpty()) {
        aviso_->setText(tr("%n aviso(s): pasa el raton", nullptr, int(esc_->avisos.size())));
        aviso_->setToolTip(esc_->avisos.join(QLatin1Char('\n')));
    }
    connect(esc_, &Escena3d::movida, this, &Vista3d::mueve);
    connect(esc_, &Escena3d::superficie, this, [this](int) {
        if (!rehacer_->isActive()) rehacer_->start();
    });
}

void Vista3d::construye()
{
    QQuickItem* raiz = qw_->rootObject();
    for (const Solido3d& s : esc_->solidos) {
        QVariantMap p;
        p.insert(QStringLiteral("cilindro"), s.cilindro);
        // El tamaño en ejes de Qt Quick 3D: una caja, (largo, alto, ancho); un
        // cilindro, (diámetro, largo, diámetro) con el eje en la Y de Qt
        const QVector3D tam = s.cilindro ? QVector3D(float(s.tam.x), float(s.tam.y), float(s.tam.x))
                                         : QVector3D(float(s.tam.x), float(s.tam.z), float(s.tam.y));
        p.insert(QStringLiteral("tam"), tam);
        QVector3D orient;                            // el eje del cilindro
        if (s.cilindro && s.eje == 'x') orient = QVector3D(0, 0, -90);
        if (s.cilindro && s.eje == 'y') orient = QVector3D(-90, 0, 0);
        p.insert(QStringLiteral("orient"), orient);
        p.insert(QStringLiteral("color"), QColor(s.color));
        p.insert(QStringLiteral("marca"), s.gira >= 0);
        p.insert(QStringLiteral("position"), q3d(esc_->posicion(s)));
        QVariant obj;
        QMetaObject::invokeMethod(raiz, "nuevoSolido", Q_RETURN_ARG(QVariant, obj),
                                  Q_ARG(QVariant, p));
        solidos_.push_back(obj.value<QObject*>());
    }
    for (const Superficie3d& s : esc_->superficies) {
        QVariantMap p;
        p.insert(QStringLiteral("position"), q3d(esc_->posicion(s)));
        QVariant obj;
        QMetaObject::invokeMethod(raiz, "nuevaSuperficie", Q_RETURN_ARG(QVariant, obj),
                                  Q_ARG(QVariant, p));
        QObject* o = obj.value<QObject*>();
        // La geometría se le pone al llegar sus primeras alturas: una vacía,
        // sin su formato, no la acepta Qt Quick 3D
        auto* g = new SuperficieGeo;
        g->setParent(o);
        superficies_.push_back(o);
        geos_.push_back(g);
        hechas_.push_back(0);
    }
    vista_inicial();
    rehaz_superficies();
}

void Vista3d::vista_inicial()
{
    if (!qw_ || !qw_->rootObject()) return;
    Vec3 cam = esc_->camara, mira = esc_->mira;
    if (!esc_->hay_camara) {
        // Sin decirlo: la caja de todo, mirada desde delante, a la derecha y arriba
        double x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9, z0 = 1e9, z1 = -1e9;
        auto mete = [&](const Vec3& c, double r) {
            x0 = std::min(x0, c.x - r); x1 = std::max(x1, c.x + r);
            y0 = std::min(y0, c.y - r); y1 = std::max(y1, c.y + r);
            z0 = std::min(z0, c.z - r); z1 = std::max(z1, c.z + r);
        };
        for (const Solido3d& s : esc_->solidos)
            mete(esc_->posicion(s), std::max(s.tam.x, std::max(s.tam.y, s.tam.z)) / 2);
        for (const Superficie3d& s : esc_->superficies) {
            const Vec3 p = esc_->posicion(s);
            mete(Vec3{p.x + s.largo / 2, p.y + s.ancho / 2, p.z}, std::max(s.largo, s.ancho) / 2);
        }
        if (x0 > x1) { x0 = y0 = z0 = -100; x1 = y1 = z1 = 100; }
        mira = Vec3{(x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2};
        const double r = std::max(x1 - x0, std::max(y1 - y0, z1 - z0));
        cam = Vec3{mira.x + 1.1 * r, mira.y - 1.4 * r, mira.z + 1.0 * r};
    }
    QMetaObject::invokeMethod(qw_->rootObject(), "ponCamara",
                              Q_ARG(QVariant, QVariant::fromValue(q3d(cam))),
                              Q_ARG(QVariant, QVariant::fromValue(q3d(mira))));
}

void Vista3d::mueve()
{
    for (int k = 0; k < solidos_.size() && k < esc_->solidos.size(); ++k) {
        QObject* o = solidos_[k];
        if (!o) continue;
        const Solido3d& s = esc_->solidos[k];
        if (!s.obs.isEmpty()) o->setProperty("position", q3d(esc_->posicion(s)));
        if (s.gira >= 0) o->setProperty("giro", esc_->giro(s));
    }
    for (int k = 0; k < superficies_.size() && k < esc_->superficies.size(); ++k)
        if (superficies_[k] && !esc_->superficies[k].obs.isEmpty())
            superficies_[k]->setProperty("position", q3d(esc_->posicion(esc_->superficies[k])));
}

void Vista3d::rehaz_superficies()
{
    for (int k = 0; k < geos_.size() && k < esc_->superficies.size(); ++k) {
        const Superficie3d& s = esc_->superficies[k];
        if (s.version == hechas_[k] || s.alturas.isEmpty()) continue;
        hechas_[k] = s.version;
        if (geos_[k]->pon(s) && superficies_[k] &&
            !superficies_[k]->property("geometry").value<QQuick3DGeometry*>())
            superficies_[k]->setProperty(
                "geometry", QVariant::fromValue(static_cast<QQuick3DGeometry*>(geos_[k])));
    }
}

QImage Vista3d::captura()
{
    return qw_ ? qw_->grabFramebuffer() : QImage();
}

} // namespace mcusim
