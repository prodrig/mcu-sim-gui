#include "ilustracion.h"

#include <QFrame>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
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
#include <QSvgRenderer>
#include <QVBoxLayout>

#include <cmath>

#include "generado.h"
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
VistaPlaca::VistaPlaca(const PlacaGui& placa, QWidget* padre)
    : QGraphicsView(padre), placa_(placa)
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
    auto* v = new VistaPlaca(placa, padre);
    if (!v->pon_capa(placa_id, false, svg, tabla, QStringLiteral("svg"), error)) {
        delete v;
        return nullptr;
    }
    return v;
}

VistaPlaca::Capa* VistaPlaca::capa(const QString& placa_id, bool bandeja) const
{
    for (const auto& c : capas_)
        if (c->placa_id == placa_id && c->bandeja == bandeja) return c.get();
    return nullptr;
}

bool VistaPlaca::tiene(const QString& placa_id, bool bandeja) const
{
    return capa(placa_id, bandeja) != nullptr;
}

QString VistaPlaca::origen(const QString& placa_id) const
{
    const Capa* c = capa(placa_id, false);
    return c ? c->origen : QString();
}

QRectF VistaPlaca::en_escena(const QString& placa_id, bool bandeja) const
{
    const Capa* c = capa(placa_id, bandeja);
    return c ? c->raiz->mapRectToScene(c->lienzo) : QRectF();
}

const InformeDibujo& VistaPlaca::informe(const QString& placa_id) const
{
    static const InformeDibujo vacio;
    const Capa* c = capa(placa_id, false);
    return c ? c->informe : vacio;
}

const InformeDibujo& VistaPlaca::informe() const
{
    static const InformeDibujo vacio;
    for (const auto& c : capas_)
        if (!c->bandeja) return c->informe;
    return vacio;
}

const DibujoPlaca& VistaPlaca::dibujo() const
{
    return *capas_.front()->dibujo;
}

QGraphicsSvgItem* VistaPlaca::fondo() const
{
    return capas_.empty() ? nullptr : capas_.front()->fondo;
}

bool VistaPlaca::pon_capa(const QString& placa_id, bool bandeja, const QByteArray& svg,
                          const QVector<EnlaceTabla>& tabla, const QString& origen,
                          QString& error)
{
    auto d = std::make_unique<DibujoPlaca>();
    if (!d->carga(svg, error)) return false;
    // Una bandeja solo lleva sus piezas; la tabla es del dibujo de la placa
    InformeDibujo inf = d->enlaza(placa_, placa_id, bandeja ? QVector<EnlaceTabla>() : tabla);

    // El fondo, sin los elementos vivos
    QSet<QString> vivos;
    for (const EnlaceDibujo& e : inf.enlaces) vivos.insert(e.elemento);
    auto rf = std::make_unique<QSvgRenderer>();
    if (!rf->load(d->sin(vivos)) || !rf->isValid()) {
        error = QObject::tr("Qt SVG no puede pintar el dibujo sin sus piezas");
        return false;
    }
    // Sirve: fuera el que hubiera
    quita_capa(placa_id, bandeja);

    auto cp = std::make_unique<Capa>();
    cp->placa_id = placa_id;
    cp->bandeja = bandeja;
    cp->origen = origen;
    cp->informe = inf;
    cp->lienzo = d->lienzo();
    cp->mm = d->mm_por_unidad();
    // §10: varias placas van a la misma escala si sus dibujos dicen cuánto
    // miden; el que no lo dice se iguala en altura, y se avisa
    if (!bandeja && placa_.es_sistema() && origen == QLatin1String("svg") && cp->mm <= 0)
        cp->informe.avisos << QObject::tr("el dibujo no dice su tamano en milimetros (width y "
                                          "height en mm): se le pone el alto de la placa mas "
                                          "alta que si lo dice, o 80 mm");
    cp->raiz = new QGraphicsRectItem;
    cp->raiz->setPen(Qt::NoPen);
    cp->raiz->setBrush(Qt::NoBrush);
    cp->raiz->setData(1, placa_id);
    scene()->addItem(cp->raiz);

    // El fondo pinta el documento entero en (0,0)-(tamaño por omisión), que
    // está en píxeles -o en lo que digan width y height-; la capa está en las
    // unidades del viewBox, que son las de los elementos. Se lleva de lo uno a
    // lo otro.
    const QRectF lienzo = cp->lienzo;
    cp->fondo = new QGraphicsSvgItem(cp->raiz);
    cp->fondo->setSharedRenderer(rf.get());
    cp->fondo->setObjectName(bandeja ? QStringLiteral("bandeja") : QStringLiteral("fondo"));
    const QSizeF ds = rf->defaultSize();
    if (ds.width() > 0 && ds.height() > 0)
        cp->fondo->setTransform(QTransform::fromScale(lienzo.width() / ds.width(),
                                                      lienzo.height() / ds.height()) *
                                QTransform::fromTranslate(lienzo.x(), lienzo.y()));
    cp->fondo->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    cp->fondo->setZValue(0);

    // Cada elemento vivo, donde estaba: el item pinta el elemento en
    // (0,0)-(su caja sin las transformaciones de sus grupos); se le pone en el
    // origen de esa caja y, después, las transformaciones de sus grupos, que
    // `boundsOnElement` no aplica.
    QSvgRenderer* r = d->renderer();
    for (const EnlaceDibujo& e : inf.enlaces) {
        auto* it = new QGraphicsSvgItem(cp->raiz);
        it->setSharedRenderer(r);
        it->setElementId(e.elemento);
        const QRectF b = r->boundsOnElement(e.elemento);
        it->setTransform(QTransform::fromTranslate(b.x(), b.y()) *
                         r->transformForElement(e.elemento));
        it->setObjectName(QStringLiteral("vivo:%1").arg(e.pieza));
        it->setData(0, e.pieza);
        it->setZValue(1);
        const PiezaGui& pz = placa_.piezas[e.pieza];
        QString ayuda = QStringLiteral("%1 · %2").arg(pz.id_local, pz.tipo);
        if (e.por == QLatin1String("tabla"))
            ayuda += QObject::tr("\n(el elemento «%1», por la tabla de enlaces)").arg(e.elemento);
        it->setToolTip(ayuda);
        vivos_.insert(e.pieza, it);
    }
    cp->dibujo = std::move(d);
    cp->rend_fondo = std::move(rf);
    Capa& ref = *cp;
    capas_.push_back(std::move(cp));
    prepara(ref);
    coloca();
    return true;
}

void VistaPlaca::quita_capa(const QString& placa_id, bool bandeja)
{
    Capa* c = capa(placa_id, bandeja);
    if (!c) return;
    // Lo que estuviera a medias con sus piezas se olvida
    if (menu_) menu_->close();
    pulsada_ = mando_pulsado_ = -1;
    QVector<Viva> quedan;
    for (const Viva& w : std::as_const(vivas_)) {
        if (w.capa != c) {
            quedan.push_back(w);
            continue;
        }
        vivos_.remove(w.pieza);
        for (quint16 id : w.obs) {
            etiquetas_.remove(id);
            decl_.remove(id);
        }
    }
    vivas_ = quedan;
    reindexa();
    delete c->raiz;                      // y con ella todo lo suyo
    capas_.erase(std::find_if(capas_.begin(), capas_.end(),
                              [c](const auto& x) { return x.get() == c; }));
    bool alguna = false;
    for (const Viva& w : std::as_const(vivas_)) alguna = alguna || w.alarma;
    if (!alguna) parpadeo_.stop();
    coloca();
}

// §10: si el dibujo dice su tamaño en milímetros, ese; si no, se le supone el
// alto de la placa más alta que sí lo diga -o 80 mm, lo de una placa de
// tamaño corriente-, que es «igualar la altura». La escena va en las unidades
// de la primera placa.
double VistaPlaca::mm_de(const Capa& x) const
{
    if (x.mm > 0) return x.mm;
    double alto_ref = 0;
    for (const auto& o : capas_)
        if (!o->bandeja && o->mm > 0) alto_ref = std::max(alto_ref, o->lienzo.height() * o->mm);
    if (alto_ref <= 0) alto_ref = 80.0;
    return x.lienzo.height() > 0 ? alto_ref / x.lienzo.height() : 1.0;
}

const VistaPlaca::Capa* VistaPlaca::primera() const
{
    // La primera placa del sistema, que es la que se pone a la izquierda; si
    // no es un sistema, la primera que llegó
    for (const SubPlacaGui& sp : placa_.placas)
        if (const Capa* c = capa(sp.id, false)) return c;
    for (const auto& x : capas_)
        if (!x->bandeja) return x.get();
    return capas_.empty() ? nullptr : capas_.front().get();
}

double VistaPlaca::escala(const Capa& c) const
{
    return mm_de(c) / mm_de(*primera());
}

void VistaPlaca::coloca()
{
    if (capas_.empty()) return;
    // Las placas en el orden del sistema -o en el que llegaron, si no lo
    // es-, centradas en vertical; la bandeja de cada una a su derecha,
    // arriba. La primera, donde está: la escena son sus coordenadas.
    QVector<Capa*> orden;
    for (const SubPlacaGui& sp : placa_.placas)
        if (Capa* c = capa(sp.id, false)) orden.push_back(c);
    for (const auto& c : capas_)
        if (!c->bandeja && !orden.contains(c.get())) orden.push_back(c.get());
    const Capa* p1 = orden.isEmpty() ? capas_.front().get() : orden.front();
    const double mm = mm_de(*p1);
    const double hueco = 8.0 / mm;                     // 8 mm de la placa a su bandeja
    const double entre = 30.0 / mm;                    // 30 mm de placa a placa: las líneas
    double alto = 0;
    for (Capa* c : orden) alto = std::max(alto, c->lienzo.height() * escala(*c));
    double x = p1->lienzo.left();
    const double y0 = p1->lienzo.top() - (alto - p1->lienzo.height()) / 2;
    QRectF todo;
    auto pon = [&](Capa& c, double y) {
        const double k = escala(c);
        c.raiz->setScale(k);
        c.raiz->setPos(QPointF(x, y) - c.lienzo.topLeft() * k);
        const QRectF r = c.raiz->mapRectToScene(c.lienzo);
        todo = todo.isNull() ? r : todo.united(r);
        return r;
    };
    for (Capa* c : orden) {
        const double y = y0 + (alto - c->lienzo.height() * escala(*c)) / 2;
        QRectF r = pon(*c, y);
        if (Capa* b = capa(c->placa_id, true)) {
            x = r.right() + hueco;
            r = pon(*b, r.top());
        }
        x = r.right() + entre;
    }
    // Una sola capa: la escena es su dibujo, justo
    if (capas_.size() == 1) scene()->setSceneRect(p1->lienzo);
    else scene()->setSceneRect(todo.adjusted(-hueco / 2, -hueco / 2, hueco / 2, hueco / 2));
    traza_lineas();
    encaja();
}

QRectF VistaPlaca::caja_de(const QString& ref) const
{
    const int b = ref.indexOf(QLatin1Char('/'));
    const QString pl = b > 0 ? ref.left(b) : QString();
    QString el = b > 0 ? ref.mid(b + 1) : ref;
    const Capa* c = capa(pl, false);
    if (!c) return QRectF();
    for (int vuelta = 0; vuelta < 2; ++vuelta) {
        if (c->dibujo->existe(el)) return c->raiz->mapRectToScene(c->dibujo->caja(el));
        const int p = el.lastIndexOf(QLatin1Char('.'));
        if (p <= 0) break;
        el = el.left(p);
    }
    return QRectF();
}

// Fase 6: una línea por cada par de conectores enchufados y por cada hilo
// entre placas, de lo que las une en una a lo que las une en la otra. Si el
// dibujo no tiene el conector, sale del borde de la placa que mira a la otra.
void VistaPlaca::traza_lineas()
{
    for (const Linea& l : std::as_const(lineas_)) delete l.item;
    lineas_.clear();
    if (!placa_.es_sistema()) return;
    const double mm = mm_de(*primera());
    auto placa_de = [](const QString& r) {
        const int b = r.indexOf(QLatin1Char('/'));
        return b > 0 ? r.left(b) : QString();
    };
    // Dónde engancha `ref` cuando la línea va hacia `hacia`
    auto punto = [&](const QString& ref, const QPointF& hacia) {
        QRectF r = caja_de(ref);
        if (r.isNull()) r = en_escena(placa_de(ref));
        if (r.isNull()) return QPointF();
        const bool derecha = hacia.x() > r.center().x();
        // Un conector de un dibujo -una tira- engancha por su lado; el borde
        // de una placa entera, por el centro de ese lado
        return QPointF(derecha ? r.right() : r.left(), r.center().y());
    };
    auto centro = [&](const QString& ref) {
        QRectF r = caja_de(ref);
        if (r.isNull()) r = en_escena(placa_de(ref));
        return r.center();
    };
    // ¿Es un PIN dibujado -`F/P1.TX`, y el dibujo tiene un `P1.TX`-? Ahí la
    // línea llega al borde del pin y no lleva punto: un punto de 2,6 mm tapa
    // un pin de 1 mm, y ese pin se vería distinto de sus vecinos
    auto pin_dibujado = [&](const QString& ref) {
        const int b = ref.indexOf(QLatin1Char('/'));
        const QString el = b > 0 ? ref.mid(b + 1) : ref;
        const Capa* c = capa(placa_de(ref), false);
        return c && el.contains(QLatin1Char('.')) && c->dibujo->existe(el);
    };
    static const QColor colores[] = {QColor(0xd9, 0x48, 0x1c), QColor(0x1f, 0x77, 0xb4),
                                     QColor(0x8e, 0x44, 0xad), QColor(0x16, 0xa0, 0x85),
                                     QColor(0xc0, 0x39, 0x2b), QColor(0x2c, 0x3e, 0x50)};
    auto traza = [&](const QString& a, const QString& b, bool hilo, const QColor& col,
                     const QString& ayuda) {
        if (placa_de(a) == placa_de(b) || !capa(placa_de(a), false) || !capa(placa_de(b), false))
            return;
        const QPointF pa = punto(a, centro(b)), pb = punto(b, centro(a));
        if (pa.isNull() || pb.isNull()) return;
        // Una curva que sale y entra en horizontal: se ve de dónde a dónde va
        // aunque cruce por encima de una placa
        QPainterPath camino(pa);
        const double dx = std::max(std::abs(pb.x() - pa.x()) * 0.45, 10.0 / mm);
        const double sa = pb.x() >= pa.x() ? 1 : -1;
        camino.cubicTo(pa + QPointF(sa * dx, 0), pb - QPointF(sa * dx, 0), pb);
        auto* it = new QGraphicsPathItem(camino);
        QPen lapiz(col, 0.8 / mm);
        if (hilo) lapiz.setStyle(Qt::DashLine);
        lapiz.setCapStyle(Qt::RoundCap);
        it->setPen(lapiz);
        it->setOpacity(0.85);
        it->setZValue(10);
        it->setAcceptedMouseButtons(Qt::NoButton);
        it->setToolTip(ayuda);
        // Y un punto en cada extremo que no sea un pin dibujado
        for (const auto& [p, ref] : {std::make_pair(pa, a), std::make_pair(pb, b)}) {
            if (pin_dibujado(ref)) continue;
            const double r = 1.3 / mm;
            auto* d = new QGraphicsEllipseItem(QRectF(p.x() - r, p.y() - r, 2 * r, 2 * r), it);
            d->setBrush(col);
            d->setPen(Qt::NoPen);
            d->setAcceptedMouseButtons(Qt::NoButton);
        }
        scene()->addItem(it);
        lineas_.push_back({a, b, hilo, pa, pb, it});
    };
    for (int k = 0; k < placa_.acoples.size(); ++k) {
        const AcopleGui& ac = placa_.acoples[k];
        const QColor col = colores[k % 6];
        QString como = ac.espejo ? tr(" (en espejo)")
                     : ac.conectores.size() > 2 ? tr(" (en pila)") : QString();
        for (int i = 0; i + 1 < ac.conectores.size(); ++i)
            traza(ac.conectores[i], ac.conectores[i + 1], false, col,
                  QStringLiteral("%1 ⇄ %2").arg(ac.conectores[i], ac.conectores[i + 1]) + como);
    }
    for (const HiloGui& h : placa_.hilos)
        traza(h.a, h.b, true, QColor(0x55, 0x55, 0x55),
              tr("hilo %1 - %2").arg(h.a, h.b));
}

// -----------------------------------------------------------------------------
// Fase 2: qué efecto lleva cada pieza dibujada, sin conocer su tipo, y lo que
// se le pone encima. Todo nace apagado: lo enciende la primera muestra.
void VistaPlaca::prepara(Capa& cp)
{
    const QRectF lienzo = cp.lienzo;
    const double grosor = std::max(lienzo.width(), lienzo.height()) / 300.0;
    for (const EnlaceDibujo& e : cp.informe.enlaces) {
        const PiezaGui& pz = placa_.piezas[e.pieza];
        Viva w;
        w.capa = &cp;
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
                cp.informe.avisos << QObject::tr("%1: el efecto «%2» no existe; se usa el que "
                                               "toca").arg(pz.id_local, e.efecto);
            w.efecto = w.o01 < 0 ? QStringLiteral("ninguno")
                     : boton     ? QStringLiteral("hundido")
                                 : QStringLiteral("brillo");
        }
        if (w.efecto == QLatin1String("brillo")) {
            w.color = color_de(cp.dibujo->renderer(), e.elemento);
            // Un halo redondo, tres veces más ancho que el elemento, del color
            // de este y más claro en el centro
            const double r = std::max(w.caja.width(), w.caja.height()) * 1.5;
            const QPointF c = w.caja.center();
            QRadialGradient g(c, r);
            g.setColorAt(0.0, mezcla(w.color, Qt::white, 0.55, 235));
            g.setColorAt(0.35, mezcla(w.color, Qt::white, 0.2, 170));
            g.setColorAt(1.0, mezcla(w.color, Qt::white, 0.0, 0));
            w.halo = new QGraphicsEllipseItem(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r),
                                              cp.raiz);
            w.halo->setBrush(g);
            w.halo->setPen(Qt::NoPen);
            w.halo->setOpacity(0);
            w.halo->setZValue(2);
            w.halo->setAcceptedMouseButtons(Qt::NoButton);
        } else if (w.efecto == QLatin1String("hundido")) {
            w.item->setTransformOriginPoint(w.item->boundingRect().center());
        }
        if (alarmas) {
            const double m = grosor * 2;
            w.contorno = new QGraphicsRectItem(w.caja.adjusted(-m, -m, m, m), cp.raiz);
            w.contorno->setPen(QPen(QColor(0xc6, 0x28, 0x28), grosor * 1.5));
            w.contorno->setBrush(Qt::NoBrush);
            w.contorno->setZValue(3);
            w.contorno->setVisible(false);
            w.contorno->setAcceptedMouseButtons(Qt::NoButton);
        }
        // Las etiquetas, una debajo de otra, bajo el elemento; de un alto que
        // se lea al tamaño de la placa entera
        double y = w.caja.bottom() + grosor;
        const double alto = std::max(lienzo.height() / 40.0, w.caja.height() * 0.3);
        for (const ObservableGui* o : rotulos) {
            auto* t = new QGraphicsSimpleTextItem(QStringLiteral("—"), cp.raiz);
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
            etiquetas_.insert(quint16(o->id_obs), t);
            y += alto;
        }
        vivas_.push_back(w);
    }
    reindexa();
}

// Los índices de pieza y de observable a su viva, después de poner o quitar
// una capa
void VistaPlaca::reindexa()
{
    viva_de_.clear();
    obs_de_.clear();
    for (int i = 0; i < vivas_.size(); ++i) {
        viva_de_.insert(vivas_[i].pieza, i);
        for (quint16 id : vivas_[i].obs) obs_de_.insert(id, i);
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
    return i < 0 ? QPoint(-1, -1)
                 : mapFromScene(vivas_[i].capa->raiz->mapToScene(vivas_[i].caja.center()));
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
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);

    struct Cual { QString id, titulo, ayuda; };
    QVector<Cual> cuales;
    if (!placa.es_sistema()) {
        cuales.push_back({QString(), placa.nombre, QString()});
    } else {
        const QVector<EnlaceGui> enlaces = placa.enlaces();
        for (const SubPlacaGui& s : placa.placas) {
            QStringList ayuda;
            if (!s.fichero.isEmpty()) ayuda << s.fichero;
            for (const EnlaceGui& e : enlaces) {
                if (e.placa_a == s.id) ayuda << tr("unida a %1 por %2").arg(e.placa_b, e.por);
                else if (e.placa_b == s.id) ayuda << tr("unida a %1 por %2").arg(e.placa_a, e.por);
            }
            cuales.push_back({s.id, QStringLiteral("%1 · %2").arg(s.id, s.nombre),
                              ayuda.join(QLatin1Char('\n'))});
        }
    }
    // Fase 6: UN dibujo para todas las placas, una al lado de otra con las
    // líneas de lo que las une; y encima, una fila por placa
    vista_ = new VistaPlaca(placa_, this);
    vista_->setObjectName(QStringLiteral("vista:"));
    connect(vista_, &VistaPlaca::orden, this, &VistaIlustracion::orden);
    for (const Cual& c : cuales) {
        // Un QFrame, no un QGroupBox: los recuadros del panel son QGroupBox,
        // y las pruebas los cuentan
        auto* marco = new QFrame(this);
        marco->setObjectName(QStringLiteral("dibujo:%1").arg(c.id));
        marco->setFrameShape(QFrame::StyledPanel);
        auto* fila = new QHBoxLayout(marco);
        fila->setContentsMargins(6, 2, 6, 2);
        auto* titulo = new QLabel(QStringLiteral("<b>%1</b>").arg(c.titulo.toHtmlEscaped()), marco);
        titulo->setToolTip(c.ayuda);
        // Fase 5: nace con el dibujo GENERADO, que siempre se puede hacer;
        // uno de verdad lo sustituye
        auto* inf = new QLabel(tr("Dibujo generado: la placa no trae el suyo."), marco);
        inf->setObjectName(QStringLiteral("informe:%1").arg(c.id));
        inf->setWordWrap(true);
        inf->setEnabled(false);
        auto* abrir = new QPushButton(tr("Abrir dibujo..."), marco);
        abrir->setObjectName(QStringLiteral("abrir:%1").arg(c.id));
        abrir->setToolTip(tr("Elegir otro SVG para esta placa. El suyo, si lo tiene, lo "
                             "manda mcu-sim."));
        const QString id = c.id;
        connect(abrir, &QPushButton::clicked, this, [this, id] { emit pide_dibujo(id); });
        fila->addWidget(titulo);
        fila->addWidget(inf, 1);
        fila->addWidget(abrir);
        v->addWidget(marco);
        recuadros_.insert(c.id, {marco, inf});
        QString e;
        vista_->pon_capa(c.id, false, dibujo_generado(placa_, c.id), {},
                         QStringLiteral("generado"), e);
    }
    v->addWidget(vista_, 1);
}

bool VistaIlustracion::hay_dibujo() const
{
    for (const QString& id : placas())
        if (vista_->origen(id) == QLatin1String("svg")) return true;
    return false;
}

QString VistaIlustracion::origen(const QString& placa_id) const
{
    VistaPlaca* v = vista(placa_id);
    return v ? v->origen(placa_id) : QString();
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
    VistaPlaca* v = vista_;
    if (!v->pon_capa(placa_id, false, svg, tabla, QStringLiteral("svg"), error)) {
        // Se queda el que había -el generado, si no había otro-, y se dice
        r.informe->setText(tr("<span style=\"color:#c62828\">El dibujo no sirve: %1</span>")
                               .arg(error.toHtmlEscaped()));
        r.informe->setToolTip(QString());
        r.informe->setEnabled(true);
        return false;
    }
    // La BANDEJA: lo que el dibujo no trae y deja ver o tocar algo, al lado,
    // con sus glifos de siempre. Ninguna pieza se pierde por no estar dibujada
    const InformeDibujo& inf = v->informe(placa_id);
    const QVector<int> sueltas = para_bandeja(placa_, placa_id, inf.sin_elemento);
    QString en_bandeja;
    if (sueltas.isEmpty()) {
        v->quita_capa(placa_id, true);
    } else {
        OpcionesGenerado o;
        o.solo = sueltas;
        o.titulo = tr("sin dibujar");
        QString e;
        v->pon_capa(placa_id, true, dibujo_generado(placa_, placa_id, o), {},
                    QStringLiteral("generado"), e);
        QStringList l;
        for (int i : sueltas) l << placa_.piezas[i].id_local;
        en_bandeja = tr("; en la bandeja: %1").arg(l.join(QStringLiteral(", ")));
    }
    v->activa_mandos(activos_);
    const QStringList det = inf.detalle();
    r.informe->setText((inf.resumen() + en_bandeja).toHtmlEscaped());
    r.informe->setToolTip(det.join(QLatin1Char('\n')));
    r.informe->setEnabled(true);
    return true;
}

void VistaIlustracion::pon_valor(quint16 id_obs, float valor)
{
    vista_->pon_valor(id_obs, valor);
}

QVector<quint16> VistaIlustracion::observados() const
{
    QVector<quint16> l;
    for (quint16 id : vista_->observados())
        if (!l.contains(id)) l.push_back(id);
    return l;
}

void VistaIlustracion::activa_mandos(bool si)
{
    activos_ = si;
    vista_->activa_mandos(si);
}

InformeDibujo VistaIlustracion::informe(const QString& placa_id) const
{
    VistaPlaca* v = vista(placa_id);
    return v ? v->informe(placa_id) : InformeDibujo();
}

} // namespace mcusim
