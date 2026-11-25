#include "lienzo.h"

#include <QApplication>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QPainterPathStroker>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>
#include <QSvgRenderer>
#include <QWheelEvent>

#include <cmath>
#include <memory>

#include "biblioteca.h"
#include "catalogo.h"
#include "exporta_placa.h"
#include "ficha.h"
#include "trazado.h"

namespace mcusim::build {

namespace {

const char* const MIME = "application/x-mcu-sim-build-entrada";
const QColor COLOR_LINEA(30, 70, 160);
const QColor COLOR_ELEGIDO(230, 120, 0);
const QColor COLOR_FALTA(200, 0, 0);

double redondea(double v) { return std::round(v * 10.0) / 10.0; }

// El texto de 1 mm de alto, para pintar en milímetros
void texto(QPainter* p, const QPointF& centro, const QString& t, double alto, const QColor& c)
{
    p->save();
    p->translate(centro);
    p->scale(alto / 10.0, alto / 10.0);
    QFont f = p->font();
    f.setPixelSize(10);
    f.setBold(true);
    p->setFont(f);
    p->setPen(c);
    p->drawText(QRectF(-30, -6, 60, 12), Qt::AlignCenter, t);
    p->restore();
}

} // namespace

// --- Los elementos -----------------------------------------------------------

class ItemPieza : public QGraphicsItem {
public:
    ItemPieza(Lienzo* l, const Instancia& i, const Dibujo& d)
        : l_(l), id_(i.id), caja_(-d.tam.width() / 2, -d.tam.height() / 2, d.tam.width(), d.tam.height())
    {
        r_ = std::make_unique<QSvgRenderer>(d.svg.toUtf8());
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
        setPos(i.pos);
        setRotation(i.giro);
        setScale(i.escala);
        setData(0, id_);
        setToolTip(QStringLiteral("<b>%1</b> — %2").arg(i.id, i.tipo));
    }
    const QString& id() const { return id_; }
    QRectF boundingRect() const override { return caja_.adjusted(-0.5, -0.5, 0.5, 0.5); }
    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        if (r_->isValid()) r_->render(p, caja_);
        else p->drawRect(caja_);
        if (isSelected()) {
            QPen pen(COLOR_ELEGIDO, 0.3, Qt::DashLine);
            p->setPen(pen);
            p->setBrush(Qt::NoBrush);
            p->drawRect(caja_.adjusted(-0.3, -0.3, 0.3, 0.3));
        }
    }
protected:
    QVariant itemChange(GraphicsItemChange c, const QVariant& v) override
    {
        if (c == ItemPositionHasChanged) l_->pieza_movida(this);
        return QGraphicsItem::itemChange(c, v);
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override
    {
        QGraphicsItem::mouseReleaseEvent(e);
        l_->suelta_piezas();
    }
private:
    Lienzo* l_;
    QString id_;
    QRectF caja_;
    std::unique_ptr<QSvgRenderer> r_;
};

class ItemTerminal : public QGraphicsRectItem {
public:
    ItemTerminal(Lienzo* l, ItemPieza* padre, const QString& ref, const QPointF& c, double lado, const QPointF& dir)
        : QGraphicsRectItem(c.x() - lado / 2, c.y() - lado / 2, lado, lado, padre), l_(l), ref_(ref), dir_(dir)
    {
        setAcceptHoverEvents(true);
        setData(1, ref);
        setPen(Qt::NoPen);
    }
    const QString& ref() const { return ref_; }
    QPointF dir() const { return dir_; }
    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        const int orden = l_->orden_de(ref_);
        const QRectF r = rect();
        if (orden >= 0) {
            p->setPen(QPen(COLOR_ELEGIDO.darker(), 0.12));
            p->setBrush(COLOR_ELEGIDO);
            p->drawRect(r);
            texto(p, r.center() + QPointF(0, -r.height() - 0.6), QString::number(orden + 1), 1.6, COLOR_ELEGIDO.darker());
        } else if (l_->sin_conectar(ref_)) {
            p->setPen(QPen(COLOR_FALTA, 0.15));
            p->setBrush(Qt::NoBrush);
            p->drawRect(r);
        } else if (encima_) {
            p->setPen(QPen(COLOR_ELEGIDO, 0.12));
            p->setBrush(QColor(230, 120, 0, 80));
            p->drawRect(r);
        }
    }
protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override { encima_ = true; update(); }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override { encima_ = false; update(); }
private:
    Lienzo* l_;
    QString ref_;
    QPointF dir_;
    bool encima_ = false;
};

class ItemConexion : public QGraphicsPathItem {
public:
    ItemConexion(int k, const Conexion& c) : k_(k), c_(c)
    {
        setFlags(ItemIsSelectable);
        setZValue(5);
        setData(2, k);
        setToolTip(QStringLiteral("%1 — %2").arg(c.a, c.b));
    }
    int k() const { return k_; }
    const Conexion& conexion() const { return c_; }
    Conexion& conexion() { return c_; }
    QVector<QPointF> puntos;     // si es de tramos
    QPainterPath shape() const override
    {
        QPainterPathStroker s;
        s.setWidth(1.4);
        return s.createStroke(path());
    }
    QRectF boundingRect() const override { return shape().boundingRect(); }
    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        QPen pen(isSelected() ? COLOR_ELEGIDO : COLOR_LINEA, 0.35);
        pen.setCapStyle(Qt::RoundCap);
        if (!c_.visible) {
            pen.setStyle(Qt::DashLine);
            pen.setColor(isSelected() ? COLOR_ELEGIDO : QColor(150, 150, 150));
        }
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        p->drawPath(path());
    }
private:
    int k_;
    Conexion c_;
};

class ItemMarco : public QGraphicsRectItem {
public:
    class Asa : public QGraphicsRectItem {
    public:
        explicit Asa(ItemMarco* m) : QGraphicsRectItem(-0.8, -0.8, 1.6, 1.6, m), m_(m)
        {
            setFlags(ItemIsMovable | ItemSendsGeometryChanges);
            setBrush(COLOR_ELEGIDO);
            setPen(Qt::NoPen);
            setCursor(Qt::SizeFDiagCursor);
        }
    protected:
        QVariant itemChange(GraphicsItemChange c, const QVariant& v) override
        {
            if (c == ItemPositionChange) {
                QPointF q = v.toPointF();
                q = QPointF(std::max(5.0, redondea(q.x())), std::max(5.0, redondea(q.y())));
                return q;
            }
            if (c == ItemPositionHasChanged) m_->setRect(QRectF(QPointF(0, 0), pos()));
            return QGraphicsRectItem::itemChange(c, v);
        }
        void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override
        {
            QGraphicsRectItem::mouseReleaseEvent(e);
            m_->suelta();
        }
    private:
        ItemMarco* m_;
    };

    ItemMarco(Lienzo* l, const QRectF& r) : l_(l)
    {
        setPos(r.topLeft());
        setRect(QRectF(QPointF(0, 0), r.size()));
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
        setPen(QPen(QColor(0, 120, 0), 0.4, Qt::DashLine));
        setZValue(-5);
        asa_ = new Asa(this);
        asa_->setPos(r.width(), r.height());
        setToolTip(QStringLiteral("El marco de la placa: se arrastra por el borde y se cambia de tamaño por su esquina"));
    }
    QRectF marco() const { return QRectF(pos(), rect().size()); }
    QPainterPath shape() const override
    {
        QPainterPath c;
        c.addRect(rect());
        QPainterPathStroker s;
        s.setWidth(1.2);
        return s.createStroke(c);
    }
    void suelta() { l_->suelta_marco(); }
protected:
    QVariant itemChange(GraphicsItemChange c, const QVariant& v) override
    {
        if (c == ItemPositionChange) {
            const QPointF q = v.toPointF();
            return QPointF(redondea(q.x()), redondea(q.y()));
        }
        return QGraphicsRectItem::itemChange(c, v);
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override
    {
        QGraphicsRectItem::mouseReleaseEvent(e);
        l_->suelta_marco();
    }
private:
    Lienzo* l_;
    Asa* asa_;
};

// --- El lienzo ---------------------------------------------------------------

QString prefijo_id(const Instancia& i)
{
    if (i.clase == Instancia::Mcu) return QStringLiteral("u");
    static const QHash<QString, QString> tabla = {
        {QStringLiteral("Led"), QStringLiteral("LD")},       {QStringLiteral("Resistencia"), QStringLiteral("R")},
        {QStringLiteral("Rpull"), QStringLiteral("R")},      {QStringLiteral("Button"), QStringLiteral("SW")},
        {QStringLiteral("Conector"), QStringLiteral("J")},   {QStringLiteral("Jumper"), QStringLiteral("JP")},
        {QStringLiteral("Crystal"), QStringLiteral("X")},    {QStringLiteral("Servo"), QStringLiteral("SV")},
        {QStringLiteral("Condensador"), QStringLiteral("C")}, {QStringLiteral("Potenciometro"), QStringLiteral("RV")},
    };
    if (tabla.contains(i.tipo)) return tabla.value(i.tipo);
    QString p;
    for (const QChar ch : i.tipo)
        if (ch.isUpper()) p += ch;
    if (p.isEmpty()) p = i.tipo.left(2).toUpper();
    if (p.startsWith(QLatin1Char('P'))) p.prepend(QLatin1Char('U'));   // P1, PA... no valen
    return p.left(3);
}

Lienzo::Lienzo(const Catalogo* c, QWidget* padre) : QGraphicsView(padre), cat_(c)
{
    escena_ = new QGraphicsScene(this);
    escena_->setSceneRect(-500, -500, 1000, 1000);
    setScene(escena_);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
    setDragMode(RubberBandDrag);
    setAcceptDrops(true);
    setTransformationAnchor(AnchorUnderMouse);
    setViewportUpdateMode(FullViewportUpdate);
    pon_zoom(6);
    connect(escena_, &QGraphicsScene::selectionChanged, this, [this] {
        if (!reconstruyendo_) emit seleccion_cambiada();
    });
    reconstruye();
}

Lienzo::~Lienzo()
{
    // Al irse la escena, su selección cambia: que no avise a nadie
    escena_->disconnect(this);
    disconnect();
}

void Lienzo::pon_zoom(double px)
{
    px = std::clamp(px, 0.5, 80.0);
    setTransform(QTransform::fromScale(px, px));
}

double Lienzo::zoom() const { return transform().m11(); }

void Lienzo::ajusta_vista()
{
    const QRectF m = marco_efectivo(doc_, *cat_);
    fitInView(m.isEmpty() ? QRectF(0, 0, 100, 80) : m.adjusted(-5, -5, 5, 5), Qt::KeepAspectRatio);
}

void Lienzo::pon_documento(const Documento& d)
{
    const QStringList antes = instancias_elegidas();
    const QList<int> antes_c = conexiones_elegidas();
    doc_ = d;
    terminales_.erase(std::remove_if(terminales_.begin(), terminales_.end(), [&](const QString& r) {
                          const Instancia* i = doc_.instancia(id_de(r));
                          return !i || !cat_->terminales(*i).contains(terminal_de(r));
                      }),
                      terminales_.end());
    reconstruye();
    reconstruyendo_ = true;
    for (const QString& id : antes)
        if (piezas_.contains(id)) piezas_.value(id)->setSelected(true);
    for (int k : antes_c)
        if (k < lineas_.size()) lineas_[k]->setSelected(true);
    reconstruyendo_ = false;
    emit seleccion_cambiada();
}

void Lienzo::reconstruye()
{
    reconstruyendo_ = true;
    escena_->clear();
    piezas_.clear();
    terms_.clear();
    lineas_.clear();
    marco_ = nullptr;
    automatico_ = nullptr;
    redes_ = calcula_redes(doc_, *cat_);
    for (const Instancia& i : std::as_const(doc_.instancias)) {
        const Dibujo d = cat_->dibujo(i);
        auto* p = new ItemPieza(this, i, d);
        escena_->addItem(p);
        piezas_.insert(i.id, p);
        const QPointF origen(d.tam.width() / 2, d.tam.height() / 2);
        const double lado = i.clase == Instancia::Mcu ? 0.45 : 0.9;
        for (const Ancla& a : d.anclas) {
            const QString ref = i.id + QLatin1Char('.') + a.terminal;
            auto* t = new ItemTerminal(this, p, ref, a.p / 10.0 - origen, lado, a.dir);
            t->setToolTip(ayuda_terminal(i, a.terminal));
            terms_.insert(ref, t);
        }
    }
    for (int k = 0; k < doc_.conexiones.size(); ++k) {
        auto* l = new ItemConexion(k, doc_.conexiones[k]);
        escena_->addItem(l);
        lineas_.push_back(l);
    }
    actualiza_lineas();
    actualiza_marco();
    reconstruyendo_ = false;
}

void Lienzo::actualiza_marco()
{
    if (doc_.marco.isValid() && !doc_.marco.isEmpty()) {
        marco_ = new ItemMarco(this, doc_.marco);
        escena_->addItem(marco_);
    } else {
        automatico_ = escena_->addRect(marco_efectivo(doc_, *cat_), QPen(QColor(0, 120, 0, 90), 0.25, Qt::DotLine));
        automatico_->setZValue(-5);
        automatico_->setToolTip(QStringLiteral("El marco automático: lo que ocupan las piezas y 3 mm más"));
    }
}

QString Lienzo::ayuda_terminal(const Instancia& i, const QString& t) const
{
    QString h = QStringLiteral("<b>%1.%2</b>").arg(i.id, cat_->rotulo(i, t).toHtmlEscaped());
    if (i.clase == Instancia::Mcu) {
        const FichaMcu* m = cat_->mcu(i.tipo);
        const Patilla* p = m ? m->patilla(t) : nullptr;
        if (p) {
            h += QStringLiteral("<br>nodo %1, %2").arg(p->nodo.isEmpty() ? QStringLiteral("—") : p->nodo, p->clase);
            QStringList fs;
            for (const Funcion& f : p->funciones) {
                const QString n = f.nombre.toHtmlEscaped() + (f.analogica ? QStringLiteral(" ~") : QString());
                fs << (f.simulada ? n : QStringLiteral("<span style='color:#999'>%1</span>").arg(n));
            }
            if (!fs.isEmpty())
                h += QStringLiteral("<br>%1<br><small>en negro, lo que mcu-sim simula</small>").arg(fs.join(QStringLiteral(", ")));
        }
    } else if (const FichaPieza* f = cat_->pieza(i.tipo)) {
        for (const Terminal& x : f->terminales)
            if (x.nombre == t && !x.cuando.isEmpty()) h += QStringLiteral("<br><i>%1</i>").arg(x.cuando.toHtmlEscaped());
    }
    const QString nodo = redes_.nodo_de.value(i.id + QLatin1Char('.') + t);
    if (!nodo.isEmpty()) h += QStringLiteral("<br>→ %1").arg(nodo.toHtmlEscaped());
    return h;
}

QPointF Lienzo::punto_de(const QString& ref, QPointF* dir, QRectF* caja) const
{
    ItemTerminal* t = terms_.value(ref);
    if (!t) {
        if (ItemPieza* p = piezas_.value(id_de(ref))) {
            if (caja) *caja = p->sceneBoundingRect();
            if (dir) *dir = QPointF(0, 1);
            return p->sceneBoundingRect().center();
        }
        return {};
    }
    const QRectF r = t->sceneBoundingRect();
    if (caja) *caja = r;
    if (dir) {
        const auto* p = static_cast<const QGraphicsItem*>(t->parentItem());
        QPointF d = QTransform().rotate(p->rotation()).map(t->dir());
        if (d.isNull()) d = QPointF(0, 1);
        *dir = d;
    }
    return r.center();
}

void Lienzo::actualiza_lineas(const QString& id)
{
    for (ItemConexion* l : std::as_const(lineas_)) {
        const Conexion& c = l->conexion();
        if (!id.isEmpty() && id_de(c.a) != id && id_de(c.b) != id) continue;
        l->setVisible(c.visible || ocultas_);
        QPointF da, db;
        QRectF ra, rb;
        const QPointF pa = punto_de(c.a, &da, &ra), pb = punto_de(c.b, &db, &rb);
        if (c.tramos) {
            l->puntos = trazado::puntos_ruta(ra, rb, c.horizontal, c.codos);
            l->setPath(trazado::redondeada(l->puntos, 1.0));
        } else {
            l->puntos.clear();
            l->setPath(trazado::curva({pa, da}, {pb, db}, 4.0));
        }
    }
}

void Lienzo::actualiza_terminales()
{
    for (ItemTerminal* t : std::as_const(terms_)) t->update();
}

// --- Lo elegido --------------------------------------------------------------

QStringList Lienzo::instancias_elegidas() const
{
    QStringList r;
    for (const Instancia& i : doc_.instancias)
        if (piezas_.contains(i.id) && piezas_.value(i.id)->isSelected()) r << i.id;
    return r;
}

QList<int> Lienzo::conexiones_elegidas() const
{
    QList<int> r;
    for (ItemConexion* l : lineas_)
        if (l->isSelected()) r << l->k();
    return r;
}

int Lienzo::conexion_elegida() const
{
    const QList<int> r = conexiones_elegidas();
    return r.size() == 1 && instancias_elegidas().isEmpty() ? r.front() : -1;
}

void Lienzo::elige_instancia(const QString& id)
{
    escena_->clearSelection();
    if (ItemPieza* p = piezas_.value(id)) p->setSelected(true);
}

void Lienzo::elige_terminal(const QString& ref)
{
    if (terminales_.contains(ref)) terminales_.removeAll(ref);
    else terminales_ << ref;
    actualiza_terminales();
    emit seleccion_cambiada();
}

void Lienzo::olvida_terminales()
{
    terminales_.clear();
    actualiza_terminales();
    emit seleccion_cambiada();
}

// --- Las acciones ------------------------------------------------------------

QString Lienzo::coloca(const Entrada& e, const QPointF& centro, QString id)
{
    if (e.clase == Entrada::Tarjeta || e.clase == Entrada::Sistema) {
        emit mensaje(QStringLiteral("%1 es una tarjeta: se coloca en un sistema, no en una placa").arg(e.nombre));
        return {};
    }
    if (e.clase == Entrada::Conjunto) {
        emit mensaje(QStringLiteral("los conjuntos de conectores (%1) todavía no se colocan: pon cada conector de su perfil").arg(e.nombre));
        return {};
    }
    Instancia i = cat_->nueva(e);
    i.pos = QPointF(redondea(centro.x()), redondea(centro.y()));
    if (id.isEmpty()) {
        const QString pre = prefijo_id(i);
        QString propuesto = id_libre(doc_, pre, i.clase == Instancia::Mcu ? 0 : 1);
        QString aviso;
        for (;;) {
            bool ok = false;
            const QString t = QInputDialog::getText(this, QStringLiteral("Nueva pieza"),
                                                    aviso + QStringLiteral("El id de %1:").arg(e.nombre),
                                                    QLineEdit::Normal, propuesto, &ok).trimmed();
            if (!ok) return {};
            aviso = motivo_id_invalido(doc_, t);
            if (aviso.isEmpty()) { id = t; break; }
            aviso = QStringLiteral("<b>%1</b>: %2<br>").arg(t.toHtmlEscaped(), aviso.toHtmlEscaped());
            propuesto = t;
        }
    } else if (!motivo_id_invalido(doc_, id).isEmpty()) {
        emit mensaje(QStringLiteral("%1: %2").arg(id, motivo_id_invalido(doc_, id)));
        return {};
    }
    i.id = id;
    Documento d = doc_;
    d.instancias.push_back(i);
    emit cambiado(d, QStringLiteral("colocar %1").arg(id));
    elige_instancia(id);
    return id;
}

bool Lienzo::une(bool tramos)
{
    if (terminales_.size() < 2) {
        emit mensaje(QStringLiteral("para unir, elige dos terminales o más con Ctrl+clic"));
        return false;
    }
    Documento d = doc_;
    const QString a = terminales_.front();
    int nuevas = 0;
    for (int k = 1; k < terminales_.size(); ++k) {
        const QString b = terminales_[k];
        if (d.conectados(a, b)) continue;
        Conexion c;
        c.a = a;
        c.b = b;
        c.tramos = tramos;
        if (tramos) {
            QPointF da, db;
            const QPointF pa = punto_de(a, &da), pb = punto_de(b, &db);
            c.horizontal = std::abs(da.x()) >= std::abs(da.y());
            c.codos = {redondea(trazado::codo_medio(pa, pb, c.horizontal))};
        }
        d.conexiones.push_back(c);
        ++nuevas;
    }
    terminales_.clear();
    if (!nuevas) {
        actualiza_terminales();
        emit mensaje(QStringLiteral("ya estaban unidos"));
        return false;
    }
    emit cambiado(d, nuevas == 1 ? QStringLiteral("unir") : QStringLiteral("unir %1").arg(nuevas));
    return true;
}

bool Lienzo::gira(int grados)
{
    const QStringList ids = instancias_elegidas();
    if (ids.isEmpty()) return false;
    Documento d = doc_;
    for (const QString& id : ids) {
        Instancia* i = d.instancia(id);
        i->giro = ((i->giro + grados) % 360 + 360) % 360;
    }
    emit cambiado(d, QStringLiteral("girar"));
    return true;
}

bool Lienzo::borra()
{
    const QStringList ids = instancias_elegidas();
    QList<int> cs = conexiones_elegidas();
    if (ids.isEmpty() && cs.isEmpty()) return false;
    Documento d = doc_;
    std::sort(cs.begin(), cs.end(), std::greater<int>());
    for (int k : cs) d.conexiones.removeAt(k);
    for (const QString& id : ids) d.quita(id);
    escena_->clearSelection();
    emit cambiado(d, QStringLiteral("borrar"));
    return true;
}

bool Lienzo::alterna_trazado()
{
    const QList<int> cs = conexiones_elegidas();
    if (cs.isEmpty()) return false;
    Documento d = doc_;
    for (int k : cs) {
        Conexion& c = d.conexiones[k];
        if (c.tramos) {
            c.tramos = false;
            c.codos.clear();
        } else {
            QPointF da;
            const QPointF pa = punto_de(c.a, &da), pb = punto_de(c.b);
            c.tramos = true;
            c.horizontal = trazado::eje_de_curva(lineas_[k]->path(), pa);
            c.codos = {redondea(trazado::codo_medio(pa, pb, c.horizontal))};
        }
    }
    emit cambiado(d, QStringLiteral("curva / tramos"));
    return true;
}

bool Lienzo::alterna_visible()
{
    const QList<int> cs = conexiones_elegidas();
    if (cs.isEmpty()) return false;
    Documento d = doc_;
    bool alguna_visible = false;
    for (int k : cs) alguna_visible = alguna_visible || d.conexiones[k].visible;
    for (int k : cs) d.conexiones[k].visible = !alguna_visible;
    emit cambiado(d, alguna_visible ? QStringLiteral("ocultar") : QStringLiteral("mostrar"));
    return true;
}

bool Lienzo::marco_a_mano(bool si)
{
    Documento d = doc_;
    if (si) {
        const QRectF m = marco_efectivo(doc_, *cat_);
        d.marco = QRectF(redondea(m.x()), redondea(m.y()), redondea(m.width()), redondea(m.height()));
    } else {
        d.marco = QRectF();
    }
    if (d == doc_) return false;
    emit cambiado(d, si ? QStringLiteral("marco a mano") : QStringLiteral("marco automático"));
    return true;
}

void Lienzo::mostrar_ocultas(bool si)
{
    ocultas_ = si;
    actualiza_lineas();
}

// --- Mover -------------------------------------------------------------------

void Lienzo::pieza_movida(ItemPieza* p)
{
    if (reconstruyendo_) return;
    actualiza_lineas(p->id());
}

void Lienzo::suelta_piezas()
{
    Documento d = doc_;
    bool cambia = false;
    for (Instancia& i : d.instancias) {
        ItemPieza* p = piezas_.value(i.id);
        if (!p) continue;
        const QPointF q(redondea(p->pos().x()), redondea(p->pos().y()));
        if (q != i.pos) {
            i.pos = q;
            cambia = true;
        }
    }
    if (cambia) emit cambiado(d, QStringLiteral("mover"));
}

void Lienzo::suelta_marco()
{
    if (!marco_) return;
    Documento d = doc_;
    d.marco = marco_->marco();
    if (d.marco != doc_.marco) emit cambiado(d, QStringLiteral("marco"));
}

// --- El ratón y el teclado ---------------------------------------------------

ItemConexion* Lienzo::conexion_en(const QPointF& s) const
{
    for (QGraphicsItem* it : escena_->items(s))
        if (auto* l = dynamic_cast<ItemConexion*>(it)) return l;
    return nullptr;
}

void Lienzo::mousePressEvent(QMouseEvent* e)
{
    const QPointF s = mapToScene(e->position().toPoint());
    if (e->button() == Qt::LeftButton && (e->modifiers() & Qt::ControlModifier)) {
        for (QGraphicsItem* it : items(e->position().toPoint()))
            if (auto* t = dynamic_cast<ItemTerminal*>(it)) {
                elige_terminal(t->ref());
                e->accept();
                return;
            }
    }
    if (e->button() == Qt::MiddleButton) {
        setDragMode(ScrollHandDrag);
        QMouseEvent falso(e->type(), e->position(), e->globalPosition(), Qt::LeftButton, Qt::LeftButton, e->modifiers());
        QGraphicsView::mousePressEvent(&falso);
        return;
    }
    // Un tramo de una conexión elegida: se arrastra
    if (e->button() == Qt::LeftButton) {
        if (ItemConexion* l = conexion_en(s); l && l->isSelected() && l->conexion().tramos) {
            const int k = trazado::tramo_en(l->puntos, s, 1.0);
            if (k >= 1 && k - 1 < l->conexion().codos.size()) {
                tramo_con_ = l->k();
                tramo_k_ = k;
                e->accept();
                return;
            }
        }
    }
    QGraphicsView::mousePressEvent(e);
}

void Lienzo::mouseMoveEvent(QMouseEvent* e)
{
    if (tramo_con_ >= 0) {
        const QPointF s = mapToScene(e->position().toPoint());
        ItemConexion* l = lineas_.value(tramo_con_);
        Conexion& c = l->conexion();
        const bool h = (tramo_k_ % 2 == 0) == c.horizontal;    // ¿es horizontal el tramo?
        c.codos[tramo_k_ - 1] = redondea(h ? s.y() : s.x());
        actualiza_lineas(id_de(c.a));
        return;
    }
    QGraphicsView::mouseMoveEvent(e);
}

void Lienzo::mouseReleaseEvent(QMouseEvent* e)
{
    if (tramo_con_ >= 0) {
        Documento d = doc_;
        d.conexiones[tramo_con_] = lineas_[tramo_con_]->conexion();
        tramo_con_ = -1;
        if (d != doc_) emit cambiado(d, QStringLiteral("mover un tramo"));
        return;
    }
    QGraphicsView::mouseReleaseEvent(e);
    if (dragMode() == ScrollHandDrag) setDragMode(RubberBandDrag);
}

void Lienzo::mouseDoubleClickEvent(QMouseEvent* e)
{
    const QPointF s = mapToScene(e->position().toPoint());
    if (ItemConexion* l = conexion_en(s); l && l->conexion().tramos) {
        Documento d = doc_;
        QVector<double> cs = trazado::con_codo(l->puntos, s, l->conexion().horizontal);
        for (double& v : cs) v = redondea(v);
        d.conexiones[l->k()].codos = cs;
        emit cambiado(d, QStringLiteral("un codo más"));
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(e);
}

void Lienzo::wheelEvent(QWheelEvent* e)
{
    // Con Ctrl, el zoom; sin él, se desplaza
    if (e->modifiers() & Qt::ControlModifier) {
        pon_zoom(zoom() * std::pow(1.0015, e->angleDelta().y()));
        e->accept();
        return;
    }
    QGraphicsView::wheelEvent(e);
}

void Lienzo::keyPressEvent(QKeyEvent* e)
{
    const QStringList ids = instancias_elegidas();
    QPointF paso;
    const double p = e->modifiers() & Qt::ShiftModifier ? 1.0 : 0.1;
    switch (e->key()) {
    case Qt::Key_Left: paso = {-p, 0}; break;
    case Qt::Key_Right: paso = {p, 0}; break;
    case Qt::Key_Up: paso = {0, -p}; break;
    case Qt::Key_Down: paso = {0, p}; break;
    case Qt::Key_Escape: olvida_terminales(); return;
    default: QGraphicsView::keyPressEvent(e); return;
    }
    if (ids.isEmpty()) return;
    Documento d = doc_;
    for (const QString& id : ids) {
        Instancia* i = d.instancia(id);
        i->pos = QPointF(redondea(i->pos.x() + paso.x()), redondea(i->pos.y() + paso.y()));
    }
    emit cambiado(d, QStringLiteral("mover"));
}

void Lienzo::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasFormat(QLatin1String(MIME))) e->acceptProposedAction();
}

void Lienzo::dragMoveEvent(QDragMoveEvent* e)
{
    if (e->mimeData()->hasFormat(QLatin1String(MIME))) e->acceptProposedAction();
}

void Lienzo::dropEvent(QDropEvent* e)
{
    const QString clave = QString::fromUtf8(e->mimeData()->data(QLatin1String(MIME)));
    const Biblioteca* b = cat_->biblioteca();
    for (const Entrada& en : b->entradas())
        if (en.clave() == clave && !en.desaparecida) {
            e->acceptProposedAction();
            coloca(en, mapToScene(e->position().toPoint()));
            return;
        }
}

void Lienzo::drawBackground(QPainter* p, const QRectF& r)
{
    p->fillRect(r, QColor(250, 250, 247));
    if (zoom() < 2.5) return;
    // La rejilla de 2,54 mm (0,1 pulgadas)
    const double g = 2.54;
    QPen pen(QColor(215, 215, 210), 0);
    p->setPen(pen);
    QVector<QPointF> puntos;
    for (double x = std::floor(r.left() / g) * g; x < r.right(); x += g)
        for (double y = std::floor(r.top() / g) * g; y < r.bottom(); y += g) puntos << QPointF(x, y);
    p->drawPoints(puntos.data(), int(puntos.size()));
}

QString mime_entrada() { return QString::fromLatin1(MIME); }

} // namespace mcusim::build
