#include "ilustracion.h"

#include <QFrame>
#include <QGraphicsScene>
#include <QGraphicsSvgItem>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSvgRenderer>
#include <QVBoxLayout>

namespace mcusim {

// =============================================================================
// VistaPlaca
// =============================================================================
VistaPlaca::VistaPlaca(QWidget* padre) : QGraphicsView(padre)
{
    setScene(new QGraphicsScene(this));
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setMinimumHeight(260);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

VistaPlaca::~VistaPlaca()
{
    // La escena -y con ella los elementos, que usan los renderers- primero
    delete scene();
}

VistaPlaca* VistaPlaca::crea(const PlacaGui& placa, const QString& placa_id,
                             const QByteArray& svg, const QVector<EnlaceTabla>& tabla,
                             QString& error, QWidget* padre)
{
    auto d = std::make_unique<DibujoPlaca>();
    if (!d->carga(svg, error)) return nullptr;
    const InformeDibujo inf = d->enlaza(placa, placa_id, tabla);

    // El fondo, sin los elementos vivos
    QSet<QString> vivos;
    for (const EnlaceDibujo& e : inf.enlaces) vivos.insert(e.elemento);
    auto rf = std::make_unique<QSvgRenderer>();
    if (!rf->load(d->sin(vivos)) || !rf->isValid()) {
        error = QObject::tr("Qt SVG no puede pintar el dibujo sin sus piezas");
        return nullptr;
    }

    auto* v = new VistaPlaca(padre);
    v->informe_ = inf;
    const QRectF lienzo = d->lienzo();
    v->scene()->setSceneRect(lienzo);

    // El fondo pinta el documento entero en (0,0)-(tamaño por omisión), que
    // está en píxeles -o en lo que digan width y height-; la escena está en
    // las unidades del viewBox, que son las de los elementos. Se lleva de lo
    // uno a lo otro.
    v->fondo_ = new QGraphicsSvgItem;
    v->fondo_->setSharedRenderer(rf.get());
    v->fondo_->setObjectName(QStringLiteral("fondo"));
    const QSizeF ds = rf->defaultSize();
    if (ds.width() > 0 && ds.height() > 0)
        v->fondo_->setTransform(QTransform::fromScale(lienzo.width() / ds.width(),
                                                      lienzo.height() / ds.height()) *
                                QTransform::fromTranslate(lienzo.x(), lienzo.y()));
    v->fondo_->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    v->fondo_->setZValue(0);
    v->scene()->addItem(v->fondo_);

    // Cada elemento vivo, donde estaba: el item pinta el elemento en
    // (0,0)-(su caja sin las transformaciones de sus grupos); se le pone en el
    // origen de esa caja y, después, las transformaciones de sus grupos, que
    // `boundsOnElement` no aplica.
    QSvgRenderer* r = d->renderer();
    for (const EnlaceDibujo& e : inf.enlaces) {
        auto* it = new QGraphicsSvgItem;
        it->setSharedRenderer(r);
        it->setElementId(e.elemento);
        const QRectF b = r->boundsOnElement(e.elemento);
        it->setTransform(QTransform::fromTranslate(b.x(), b.y()) *
                         r->transformForElement(e.elemento));
        it->setObjectName(QStringLiteral("vivo:%1").arg(e.pieza));
        it->setData(0, e.pieza);
        it->setZValue(1);
        const PiezaGui& pz = placa.piezas[e.pieza];
        QString ayuda = QStringLiteral("%1 · %2").arg(pz.id_local, pz.tipo);
        if (e.por == QLatin1String("tabla"))
            ayuda += QObject::tr("\n(el elemento «%1», por la tabla de enlaces)").arg(e.elemento);
        it->setToolTip(ayuda);
        v->scene()->addItem(it);
        v->vivos_.insert(e.pieza, it);
    }
    v->dibujo_ = std::move(d);
    v->rend_fondo_ = std::move(rf);
    return v;
}

void VistaPlaca::encaja()
{
    if (scene()) fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
}

void VistaPlaca::resizeEvent(QResizeEvent* e)
{
    QGraphicsView::resizeEvent(e);
    encaja();
}

void VistaPlaca::showEvent(QShowEvent* e)
{
    QGraphicsView::showEvent(e);
    encaja();
}

QImage VistaPlaca::imagen(int ancho) const
{
    const QRectF l = scene()->sceneRect();
    const int alto = l.width() > 0 ? qMax(1, int(ancho * l.height() / l.width() + 0.5)) : ancho;
    QImage img(ancho, alto, QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    scene()->render(&p, QRectF(0, 0, ancho, alto), l);
    return img;
}

QPointF VistaPlaca::en_imagen(const QPointF& p, int ancho) const
{
    const QRectF l = scene()->sceneRect();
    const double k = l.width() > 0 ? ancho / l.width() : 1.0;
    return (p - l.topLeft()) * k;
}

// =============================================================================
// VistaIlustracion
// =============================================================================
VistaIlustracion::VistaIlustracion(const PlacaGui& placa, QWidget* padre)
    : QWidget(padre), placa_(placa)
{
    setObjectName(QStringLiteral("ilustracion"));
    auto* exterior = new QVBoxLayout(this);
    exterior->setContentsMargins(0, 0, 0, 0);
    auto* desliza = new QScrollArea(this);
    desliza->setWidgetResizable(true);
    desliza->setFrameShape(QFrame::NoFrame);
    exterior->addWidget(desliza);
    auto* dentro = new QWidget(desliza);
    auto* v = new QVBoxLayout(dentro);

    struct Cual { QString id, titulo, ayuda; };
    QVector<Cual> cuales;
    if (!placa.es_sistema()) {
        cuales.push_back({QString(), placa.nombre, QString()});
    } else {
        for (const SubPlacaGui& s : placa.placas)
            cuales.push_back({s.id, QStringLiteral("%1 · %2").arg(s.id, s.nombre), s.fichero});
    }
    for (const Cual& c : cuales) {
        // Un QFrame, no un QGroupBox: los recuadros del panel son QGroupBox,
        // y las pruebas los cuentan
        auto* marco = new QFrame(dentro);
        marco->setObjectName(QStringLiteral("dibujo:%1").arg(c.id));
        marco->setFrameShape(QFrame::StyledPanel);
        auto* caja = new QVBoxLayout(marco);
        auto* cabeza = new QHBoxLayout;
        auto* titulo = new QLabel(QStringLiteral("<b>%1</b>").arg(c.titulo.toHtmlEscaped()), marco);
        titulo->setToolTip(c.ayuda);
        auto* abrir = new QPushButton(tr("Abrir dibujo..."), marco);
        abrir->setObjectName(QStringLiteral("abrir:%1").arg(c.id));
        abrir->setToolTip(tr("Elegir el SVG de esta placa. Mientras mcu-sim no los mande, "
                             "los dibujos se abren a mano."));
        const QString id = c.id;
        connect(abrir, &QPushButton::clicked, this, [this, id] { emit pide_dibujo(id); });
        cabeza->addWidget(titulo, 1);
        cabeza->addWidget(abrir);
        caja->addLayout(cabeza);
        auto* hueco = new QLabel(tr("Esta placa no tiene dibujo. Sus piezas estan en la "
                                    "pestana Panel."), marco);
        hueco->setObjectName(QStringLiteral("hueco:%1").arg(c.id));
        hueco->setAlignment(Qt::AlignCenter);
        hueco->setEnabled(false);
        hueco->setMinimumHeight(80);
        caja->addWidget(hueco, 1);
        auto* inf = new QLabel(marco);
        inf->setObjectName(QStringLiteral("informe:%1").arg(c.id));
        inf->setWordWrap(true);
        inf->hide();
        caja->addWidget(inf);
        v->addWidget(marco, 1);
        recuadros_.insert(c.id, {marco, caja, hueco, inf});
    }
    desliza->setWidget(dentro);
}

QStringList VistaIlustracion::placas() const
{
    QStringList l;
    if (!placa_.es_sistema()) l << QString();
    for (const SubPlacaGui& s : placa_.placas) l << s.id;
    return l;
}

QString VistaIlustracion::nombre_de(const QString& placa_id) const
{
    if (!placa_.es_sistema()) return placa_.nombre;
    const SubPlacaGui* s = placa_.subplaca(placa_id);
    return s ? s->nombre : QString();
}

bool VistaIlustracion::pon_dibujo(const QString& placa_id, const QByteArray& svg,
                                  const QVector<EnlaceTabla>& tabla, QString& error)
{
    const auto it = recuadros_.find(placa_id);
    if (it == recuadros_.end()) {
        error = tr("no hay ninguna placa \"%1\"").arg(placa_id);
        return false;
    }
    Recuadro& r = *it;
    VistaPlaca* v = VistaPlaca::crea(placa_, placa_id, svg, tabla, error, r.marco);
    if (!v) {
        r.informe->setText(tr("<span style=\"color:#c62828\">El dibujo no sirve: %1</span>")
                               .arg(error.toHtmlEscaped()));
        r.informe->setToolTip(QString());
        r.informe->show();
        return false;
    }
    v->setObjectName(QStringLiteral("vista:%1").arg(placa_id));
    r.caja->replaceWidget(r.contenido, v);
    delete r.contenido;           // el hueco, o el dibujo de antes
    r.contenido = v;
    r.caja->setStretchFactor(v, 1);
    vistas_.insert(placa_id, v);

    const InformeDibujo& inf = v->informe();
    const QStringList det = inf.detalle();
    r.informe->setText(inf.resumen().toHtmlEscaped());
    r.informe->setToolTip(det.join(QLatin1Char('\n')));
    r.informe->show();
    return true;
}

} // namespace mcusim
