#include "ilustracion.h"

#include <QFrame>
#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsSvgItem>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSvgRenderer>
#include <QVBoxLayout>

#include <cmath>

#include "panel.h"

namespace mcusim {

namespace {

// Un 0/1 sin unidad que no es una alarma: un sí o un no, como en el panel
bool es_01(const ObservableGui& o)
{
    return o.min == 0 && o.max == 1 && o.unidad.isEmpty() && !o.alarma;
}

// El color de un elemento: se pinta en una imagen pequeña y se promedian sus
// píxeles, pesados por lo opacos que son. Qt SVG no dice de qué color pinta
// algo, y en un dibujo con hoja de estilo el color va por clase.
QColor color_de(QSvgRenderer* r, const QString& id)
{
    QImage img(24, 24, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        r->render(&p, id, QRectF(0, 0, 24, 24));
    }
    double R = 0, G = 0, B = 0, A = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QRgb c = img.pixel(x, y);
            const double a = qAlpha(c) / 255.0;
            R += qRed(c) * a;
            G += qGreen(c) * a;
            B += qBlue(c) * a;
            A += a;
        }
    if (A < 1) return QColor(255, 220, 120);     // invisible: un ámbar cualquiera
    return QColor(int(R / A + 0.5), int(G / A + 0.5), int(B / A + 0.5));
}

QColor mezcla(const QColor& a, const QColor& b, double t, int alfa)
{
    return QColor(int(a.red() + (b.red() - a.red()) * t),
                  int(a.green() + (b.green() - a.green()) * t),
                  int(a.blue() + (b.blue() - a.blue()) * t), alfa);
}

} // namespace

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
    parpadeo_.setInterval(PARPADEO_MS);
    connect(&parpadeo_, &QTimer::timeout, this, &VistaPlaca::parpadea);
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
    v->prepara(placa);
    return v;
}

// -----------------------------------------------------------------------------
// Fase 2: qué efecto lleva cada pieza dibujada, sin conocer su tipo, y lo que
// se le pone encima. Todo nace apagado: lo enciende la primera muestra.
void VistaPlaca::prepara(const PlacaGui& placa)
{
    const QRectF lienzo = scene()->sceneRect();
    const double grosor = std::max(lienzo.width(), lienzo.height()) / 300.0;
    for (const EnlaceDibujo& e : informe_.enlaces) {
        const PiezaGui& pz = placa.piezas[e.pieza];
        Viva w;
        w.pieza = e.pieza;
        w.item = vivos_.value(e.pieza);
        w.caja = e.caja;
        w.ayuda = w.item->toolTip();
        bool boton = false;
        for (const MandoGui& m : pz.mandos)
            if (m.tipo == QLatin1String("boton")) boton = true;
        bool alarmas = false;
        QVector<const ObservableGui*> rotulos;
        for (const ObservableGui& o : pz.observables) {
            decl_.insert(quint16(o.id_obs), o);
            w.obs.push_back(quint16(o.id_obs));
            obs_de_.insert(quint16(o.id_obs), int(vivas_.size()));
            if (o.alarma) alarmas = true;
            else if (es_01(o)) { if (w.o01 < 0) w.o01 = o.id_obs; }
            else if (!o.unidad.isEmpty() && w.intensidad < 0 && o.max != o.min) {
                w.intensidad = o.id_obs;
                w.i_max = std::max(std::abs(o.min), std::abs(o.max));
            }
            if (o.interesante && !o.alarma && !es_01(o)) rotulos.push_back(&o);
        }
        // El efecto: el de la tabla, si lo dice; si no, el de la declaración
        w.efecto = e.efecto;
        if (w.efecto.isEmpty() || (w.efecto != QLatin1String("brillo") &&
                                   w.efecto != QLatin1String("hundido") &&
                                   w.efecto != QLatin1String("ninguno"))) {
            if (!e.efecto.isEmpty())
                informe_.avisos << QObject::tr("%1: el efecto «%2» no existe; se usa el que "
                                               "toca").arg(pz.id_local, e.efecto);
            w.efecto = w.o01 < 0 ? QStringLiteral("ninguno")
                     : boton     ? QStringLiteral("hundido")
                                 : QStringLiteral("brillo");
        }
        if (w.efecto == QLatin1String("brillo")) {
            w.color = color_de(dibujo_->renderer(), e.elemento);
            // Un halo redondo, tres veces más ancho que el elemento, del color
            // de este y más claro en el centro
            const double r = std::max(w.caja.width(), w.caja.height()) * 1.5;
            const QPointF c = w.caja.center();
            QRadialGradient g(c, r);
            g.setColorAt(0.0, mezcla(w.color, Qt::white, 0.55, 235));
            g.setColorAt(0.35, mezcla(w.color, Qt::white, 0.2, 170));
            g.setColorAt(1.0, mezcla(w.color, Qt::white, 0.0, 0));
            w.halo = new QGraphicsEllipseItem(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r));
            w.halo->setBrush(g);
            w.halo->setPen(Qt::NoPen);
            w.halo->setOpacity(0);
            w.halo->setZValue(2);
            w.halo->setAcceptedMouseButtons(Qt::NoButton);
            scene()->addItem(w.halo);
        } else if (w.efecto == QLatin1String("hundido")) {
            w.item->setTransformOriginPoint(w.item->boundingRect().center());
        }
        if (alarmas) {
            const double m = grosor * 2;
            w.contorno = new QGraphicsRectItem(w.caja.adjusted(-m, -m, m, m));
            w.contorno->setPen(QPen(QColor(0xc6, 0x28, 0x28), grosor * 1.5));
            w.contorno->setBrush(Qt::NoBrush);
            w.contorno->setZValue(3);
            w.contorno->setVisible(false);
            w.contorno->setAcceptedMouseButtons(Qt::NoButton);
            scene()->addItem(w.contorno);
        }
        // Las etiquetas, una debajo de otra, bajo el elemento; de un alto que
        // se lea al tamaño de la placa entera
        double y = w.caja.bottom() + grosor;
        const double alto = std::max(lienzo.height() / 40.0, w.caja.height() * 0.3);
        for (const ObservableGui* o : rotulos) {
            auto* t = new QGraphicsSimpleTextItem(QStringLiteral("—"));
            QFont f = t->font();
            f.setPixelSize(20);
            f.setBold(true);
            t->setFont(f);
            t->setBrush(QColor(0x20, 0x20, 0x20));
            const double k = alto / t->boundingRect().height();
            t->setScale(k);
            t->setPos(w.caja.center().x() - t->boundingRect().width() * k / 2, y);
            t->setZValue(4);
            t->setData(1, w.caja.center().x());
            t->setAcceptedMouseButtons(Qt::NoButton);
            scene()->addItem(t);
            etiquetas_.insert(quint16(o->id_obs), t);
            y += alto;
        }
        viva_de_.insert(w.pieza, int(vivas_.size()));
        vivas_.push_back(w);
    }
}

void VistaPlaca::pon_valor(quint16 id_obs, float valor)
{
    const auto it = obs_de_.constFind(id_obs);
    if (it == obs_de_.constEnd()) return;
    Viva& w = vivas_[*it];
    const auto ant = w.valores.constFind(id_obs);
    if (ant != w.valores.constEnd() && *ant == valor) return;   // nada que hacer
    w.valores.insert(id_obs, valor);
    if (QGraphicsSimpleTextItem* t = etiquetas_.value(id_obs, nullptr)) {
        const QString s = Panel::texto_de(decl_.value(id_obs), valor);
        if (t->text() != s) {
            t->setText(s);
            const double k = t->scale();
            t->setX(t->data(1).toDouble() - t->boundingRect().width() * k / 2);
        }
    }
    repinta(w);
}

void VistaPlaca::repinta(Viva& w)
{
    const float sin_valor = 0.f;
    // El brillo: apagado, nada; encendido, según la intensidad
    if (w.halo) {
        double op = 0;
        if (w.o01 >= 0 && w.valores.value(quint16(w.o01), sin_valor) >= 0.5f) {
            op = 1;
            if (w.intensidad >= 0 && w.valores.contains(quint16(w.intensidad))) {
                const double i = std::abs(double(w.valores.value(quint16(w.intensidad))));
                op = std::clamp(0.3 + 0.7 * std::sqrt(std::min(i / w.i_max, 1.0)), 0.0, 1.0);
            }
        }
        if (w.halo->opacity() != op) w.halo->setOpacity(op);
    }
    // La tapa hundida
    if (w.efecto == QLatin1String("hundido")) {
        const bool h = w.o01 >= 0 && w.valores.value(quint16(w.o01), sin_valor) >= 0.5f;
        if (h != w.hundido) {
            w.hundido = h;
            w.item->setScale(h ? 0.88 : 1.0);
            w.item->setOpacity(h ? 0.8 : 1.0);
        }
    }
    // La alarma
    bool alarma = false;
    for (auto it = w.valores.constBegin(); it != w.valores.constEnd(); ++it)
        if (Panel::en_alarma(decl_.value(it.key()), it.value())) alarma = true;
    if (w.contorno && alarma != w.alarma) {
        w.alarma = alarma;
        bool alguna = false;
        for (const Viva& x : std::as_const(vivas_)) alguna = alguna || x.alarma;
        if (alarma) {
            // Se ve al momento, y desde ahí parpadea
            w.contorno->setVisible(true);
            if (!parpadeo_.isActive()) {
                fase_ = true;
                parpadeo_.start();
            }
        } else {
            w.contorno->setVisible(false);
        }
        if (!alguna) parpadeo_.stop();
    }
    // La ayuda: todos sus valores
    QStringList l{w.ayuda};
    for (const quint16 id : w.obs) {
        const ObservableGui o = decl_.value(id);
        const auto v = w.valores.constFind(id);
        l << QStringLiteral("%1: %2").arg(o.nombre, v == w.valores.constEnd()
                                                        ? QStringLiteral("—")
                                                        : Panel::texto_de(o, *v));
    }
    const QString a = l.join(QLatin1Char('\n'));
    if (w.item->toolTip() != a) w.item->setToolTip(a);
}

void VistaPlaca::parpadea()
{
    fase_ = !fase_;
    for (const Viva& w : std::as_const(vivas_))
        if (w.contorno && w.alarma) w.contorno->setVisible(fase_);
}

QVector<quint16> VistaPlaca::observados() const
{
    QVector<quint16> l;
    for (const Viva& w : vivas_) l += w.obs;
    return l;
}

QString VistaPlaca::efecto(int pieza) const
{
    const int i = viva_de_.value(pieza, -1);
    return i < 0 ? QString() : vivas_[i].efecto;
}

QGraphicsEllipseItem* VistaPlaca::halo(int pieza) const
{
    const int i = viva_de_.value(pieza, -1);
    return i < 0 ? nullptr : vivas_[i].halo;
}

QGraphicsRectItem* VistaPlaca::contorno(int pieza) const
{
    const int i = viva_de_.value(pieza, -1);
    return i < 0 ? nullptr : vivas_[i].contorno;
}

QColor VistaPlaca::color(int pieza) const
{
    const int i = viva_de_.value(pieza, -1);
    return i < 0 ? QColor() : vivas_[i].color;
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

void VistaIlustracion::pon_valor(quint16 id_obs, float valor)
{
    for (VistaPlaca* v : std::as_const(vistas_)) v->pon_valor(id_obs, valor);
}

QVector<quint16> VistaIlustracion::observados() const
{
    QVector<quint16> l;
    for (VistaPlaca* v : vistas_)
        for (quint16 id : v->observados())
            if (!l.contains(id)) l.push_back(id);
    return l;
}

} // namespace mcusim
