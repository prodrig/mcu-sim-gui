#include "ilustracion.h"

#include <QFrame>
#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsSvgItem>
#include <QHBoxLayout>
#include <QContextMenuEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QSlider>
#include <QSpinBox>
#include <QWheelEvent>
#include <QWidgetAction>
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
    viewport()->setMouseTracking(true);
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
        for (const MandoGui& m : pz.mandos) {
            if (m.tipo == QLatin1String("boton")) boton = true;
            w.mandos.push_back(m);
            w.valor.push_back(float(m.valor));
            w.dedo.push_back(0);
            w.fijo.push_back(0);
            w.enviado.push_back(0);
        }
        // La ayuda dice qué hace el ratón
        if (!w.mandos.isEmpty()) {
            const MandoGui& m = w.mandos[0];
            const QString como =
                m.tipo == QLatin1String("interruptor") ? QObject::tr("clic: cambia %1")
              : m.tipo == QLatin1String("continuo") || m.tipo == QLatin1String("discreto")
                    ? QObject::tr("clic o rueda: %1")
                    : QObject::tr("clic: %1 · Ctrl+clic: lo deja hundido");
            w.ayuda += QLatin1Char('\n') + como.arg(m.nombre) +
                       QObject::tr(" · boton derecho: todos sus mandos");
            w.item->setToolTip(w.ayuda);
        }
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
    // La tapa hundida: lo dice la muestra o, sin esperarla, el ratón
    if (w.efecto == QLatin1String("hundido")) {
        w.hundido_obs = w.o01 >= 0 && w.valores.value(quint16(w.o01), sin_valor) >= 0.5f;
        bool mano = false;
        if (!w.mandos.isEmpty() && w.mandos[0].tipo == QLatin1String("boton"))
            mano = w.dedo[0] || w.fijo[0];
        const bool h = w.hundido_obs || mano;
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

// -----------------------------------------------------------------------------
// Fase 3: los mandos
// -----------------------------------------------------------------------------
int VistaPlaca::viva_en(const QPoint& p) const
{
    // Lo de encima -halos, contornos, etiquetas- no cuenta: solo un vivo
    for (QGraphicsItem* it : items(p)) {
        const QVariant d = it->data(0);
        if (d.isValid()) return viva_de_.value(d.toInt(), -1);
    }
    return -1;
}

QPoint VistaPlaca::donde(int pieza) const
{
    const int i = viva_de_.value(pieza, -1);
    return i < 0 ? QPoint(-1, -1) : mapFromScene(vivas_[i].caja.center());
}

void VistaPlaca::manda(Viva& w, int m, float v)
{
    w.valor[m] = v;
    emit orden(quint16(w.pieza), quint16(w.mandos[m].idx), v);
}

// Como en el panel: hundido si lo está el dedo O el «switch», y solo se
// ordena cuando eso cambia
void VistaPlaca::ordena_boton(Viva& w, int m)
{
    const bool ahora = w.dedo[m] || w.fijo[m];
    if (ahora != bool(w.enviado[m])) {
        w.enviado[m] = ahora;
        manda(w, m, float(ahora ? w.mandos[m].max : w.mandos[m].min));
    }
    repinta(w);
}

void VistaPlaca::mueve(Viva& w, int m, float v)
{
    const MandoGui& md = w.mandos[m];
    v = std::clamp(v, float(md.min), float(md.max));
    if (md.tipo == QLatin1String("discreto")) v = float(std::lround(v));
    if (v != w.valor[m]) manda(w, m, v);
}

// El control de un mando continuo o discreto, para un menú: su nombre, un
// deslizador o una caja numérica, y su valor
QWidget* VistaPlaca::control(Viva& w, int m, QWidget* padre)
{
    const int i = viva_de_.value(w.pieza);
    const MandoGui md = w.mandos[m];
    auto* fila = new QWidget(padre);
    auto* h = new QHBoxLayout(fila);
    h->setContentsMargins(8, 4, 8, 4);
    h->addWidget(new QLabel(md.nombre, fila));
    const QString nombre = QStringLiteral("mando:%1:%2").arg(w.pieza).arg(md.idx);
    if (md.tipo == QLatin1String("discreto")) {
        auto* sb = new QSpinBox(fila);
        sb->setObjectName(nombre);
        sb->setRange(int(std::ceil(md.min)), int(std::floor(md.max)));
        sb->setValue(int(std::lround(w.valor[m])));
        connect(sb, &QSpinBox::valueChanged, this,
                [this, i, m](int v) { mueve(vivas_[i], m, float(v)); });
        sb->setEnabled(activos_);
        h->addWidget(sb);
    } else {
        auto* s = new QSlider(Qt::Horizontal, fila);
        s->setObjectName(nombre);
        s->setRange(0, 1000);
        s->setMinimumWidth(160);
        auto* num = new QLabel(fila);
        num->setObjectName(QStringLiteral("valor:%1:%2").arg(w.pieza).arg(md.idx));
        num->setMinimumWidth(num->fontMetrics().horizontalAdvance(QStringLiteral("00000.0")));
        const float lo = float(md.min), hi = float(md.max);
        const double f = hi > lo ? (w.valor[m] - lo) / (hi - lo) : 0.0;
        s->setValue(qBound(0, int(f * 1000.0 + 0.5), 1000));
        num->setText(QString::number(double(w.valor[m]), 'g', 4));
        connect(s, &QSlider::valueChanged, this, [this, i, m, lo, hi, num](int v) {
            const float x = lo + (hi - lo) * float(v) / 1000.f;
            num->setText(QString::number(double(x), 'g', 4));
            mueve(vivas_[i], m, x);
        });
        s->setEnabled(activos_);
        h->addWidget(s, 1);
        h->addWidget(num);
    }
    return fila;
}

void VistaPlaca::abre_menu(QMenu* m, const QPoint& donde)
{
    if (menu_) menu_->close();
    m->setAttribute(Qt::WA_DeleteOnClose);
    menu_ = m;
    m->popup(donde);
}

void VistaPlaca::mousePressEvent(QMouseEvent* e)
{
    const int i = e->button() == Qt::LeftButton && activos_ ? viva_en(e->pos()) : -1;
    if (i < 0 || vivas_[i].mandos.isEmpty()) {
        QGraphicsView::mousePressEvent(e);
        return;
    }
    Viva& w = vivas_[i];
    const MandoGui& m = w.mandos[0];
    if (m.tipo == QLatin1String("interruptor")) {
        manda(w, 0, float(w.valor[0] >= (m.min + m.max) / 2 ? m.min : m.max));
    } else if (m.tipo == QLatin1String("continuo") || m.tipo == QLatin1String("discreto")) {
        auto* menu = new QMenu(this);
        menu->setObjectName(QStringLiteral("emergente:%1").arg(w.pieza));
        auto* a = new QWidgetAction(menu);
        a->setDefaultWidget(control(w, 0, menu));
        menu->addAction(a);
        abre_menu(menu, e->globalPosition().toPoint());
    } else {                                    // "boton", y lo que no se conozca
        if (e->modifiers() & Qt::ControlModifier) {
            w.fijo[0] = !w.fijo[0];
        } else {
            w.dedo[0] = 1;
            pulsada_ = i;
            mando_pulsado_ = 0;
        }
        ordena_boton(w, 0);
    }
    e->accept();
}

void VistaPlaca::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && pulsada_ >= 0) {
        Viva& w = vivas_[pulsada_];
        const int m = mando_pulsado_;
        w.dedo[m] = 0;
        pulsada_ = mando_pulsado_ = -1;
        ordena_boton(w, m);
        e->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(e);
}

// Un doble clic es, para Qt, una pulsación que no viene como tal: sin esto,
// dos clics rápidos a un interruptor lo cambiarían una sola vez
void VistaPlaca::mouseDoubleClickEvent(QMouseEvent* e)
{
    mousePressEvent(e);
}

void VistaPlaca::mouseMoveEvent(QMouseEvent* e)
{
    const int i = activos_ ? viva_en(e->pos()) : -1;
    const bool mano = i >= 0 && !vivas_[i].mandos.isEmpty();
    viewport()->setCursor(mano ? Qt::PointingHandCursor : Qt::ArrowCursor);
    QGraphicsView::mouseMoveEvent(e);
}

void VistaPlaca::wheelEvent(QWheelEvent* e)
{
    const int i = activos_ ? viva_en(e->position().toPoint()) : -1;
    if (i >= 0 && !vivas_[i].mandos.isEmpty()) {
        Viva& w = vivas_[i];
        const MandoGui& m = w.mandos[0];
        const bool discreto = m.tipo == QLatin1String("discreto");
        if (discreto || m.tipo == QLatin1String("continuo")) {
            const int pasos = e->angleDelta().y() / 120;
            const double paso = discreto ? 1.0 : (m.max - m.min) / 20.0;
            if (pasos != 0) mueve(w, 0, float(w.valor[0] + pasos * paso));
            e->accept();
            return;
        }
    }
    QGraphicsView::wheelEvent(e);
}

// Todos los mandos de la pieza, con el botón derecho. Apagados si el modelo
// no espera ni corre, pero se ven: así se sabe qué tiene.
void VistaPlaca::contextMenuEvent(QContextMenuEvent* e)
{
    const int i = viva_en(e->pos());
    if (i < 0 || vivas_[i].mandos.isEmpty()) {
        QGraphicsView::contextMenuEvent(e);
        return;
    }
    Viva& w = vivas_[i];
    auto* menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("menu:%1").arg(w.pieza));
    menu->addSection(w.ayuda.section(QLatin1Char('\n'), 0, 0));
    for (int m = 0; m < w.mandos.size(); ++m) {
        const MandoGui& md = w.mandos[m];
        if (md.tipo == QLatin1String("continuo") || md.tipo == QLatin1String("discreto")) {
            auto* a = new QWidgetAction(menu);
            a->setDefaultWidget(control(w, m, menu));
            menu->addAction(a);
            continue;
        }
        const bool interruptor = md.tipo == QLatin1String("interruptor");
        QAction* a = menu->addAction(interruptor ? md.nombre
                                                 : tr("%1: dejarlo hundido").arg(md.nombre));
        a->setObjectName(QStringLiteral("mando:%1:%2").arg(w.pieza).arg(md.idx));
        a->setCheckable(true);
        a->setChecked(interruptor ? w.valor[m] >= (md.min + md.max) / 2 : bool(w.fijo[m]));
        a->setEnabled(activos_);
        connect(a, &QAction::toggled, this, [this, i, m, interruptor](bool si) {
            Viva& x = vivas_[i];
            if (interruptor) {
                manda(x, m, float(si ? x.mandos[m].max : x.mandos[m].min));
            } else {
                x.fijo[m] = si;
                ordena_boton(x, m);
            }
        });
    }
    abre_menu(menu, e->globalPos());
    e->accept();
}

void VistaPlaca::activa_mandos(bool si)
{
    activos_ = si;
    if (si) return;
    // Apagados: lo que estaba a medias se olvida, y el menú se cierra
    if (pulsada_ >= 0) {
        vivas_[pulsada_].dedo[mando_pulsado_] = 0;
        repinta(vivas_[pulsada_]);
    }
    pulsada_ = mando_pulsado_ = -1;
    if (menu_) menu_->close();
    viewport()->setCursor(Qt::ArrowCursor);
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
    connect(v, &VistaPlaca::orden, this, &VistaIlustracion::orden);
    v->activa_mandos(activos_);

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

void VistaIlustracion::activa_mandos(bool si)
{
    activos_ = si;
    for (VistaPlaca* v : std::as_const(vistas_)) v->activa_mandos(si);
}

} // namespace mcusim
