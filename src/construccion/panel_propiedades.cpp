#include "panel_propiedades.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include "catalogo.h"
#include "exporta_placa.h"
#include "ficha.h"
#include "lienzo.h"

namespace mcusim::build {

PanelPropiedades::PanelPropiedades(Lienzo* l, QWidget* padre) : QWidget(padre), l_(l)
{
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(4, 4, 4, 4);
    titulo_ = new QLabel(this);
    titulo_->setWordWrap(true);
    titulo_->setTextFormat(Qt::RichText);
    v->addWidget(titulo_);
    auto* sc = new QScrollArea(this);
    sc->setWidgetResizable(true);
    sc->setFrameShape(QFrame::NoFrame);
    cuerpo_ = new QWidget(sc);
    form_ = new QFormLayout(cuerpo_);
    form_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    sc->setWidget(cuerpo_);
    v->addWidget(sc, 1);
    connect(l_, &Lienzo::seleccion_cambiada, this, &PanelPropiedades::actualiza);
    actualiza();
}

void PanelPropiedades::limpia()
{
    // Que lo que se va no mande nada al irse (un editingFinished al perder el foco)
    for (QWidget* w : cuerpo_->findChildren<QWidget*>()) w->blockSignals(true);
    while (form_->rowCount()) form_->removeRow(0);
}

void PanelPropiedades::actualiza()
{
    limpia();
    const QStringList ids = l_->instancias_elegidas();
    const int k = l_->conexion_elegida();
    if (ids.size() == 1) instancia(ids.front());
    else if (k >= 0) conexion(k);
    else if (ids.size() > 1) titulo_->setText(QStringLiteral("<b>%1 piezas</b>").arg(ids.size()));
    else placa();
    const QStringList& ts = l_->terminales_elegidos();
    if (!ts.isEmpty())
        form_->addRow(new QLabel(QStringLiteral("<i>Para unir (Ctrl+U o Ctrl+T):</i><br>%1")
                                     .arg(ts.join(QStringLiteral(" → ")).toHtmlEscaped()), cuerpo_));
}

void PanelPropiedades::instancia(const QString& id)
{
    const Documento& d = l_->documento();
    const Instancia* ip = d.instancia(id);
    if (!ip) return;
    const Instancia i = *ip;
    const Catalogo* c = l_->catalogo();
    titulo_->setText(QStringLiteral("<b>%1</b> — %2%3").arg(i.id.toHtmlEscaped(), i.tipo.toHtmlEscaped(),
                     i.perfil.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(i.perfil.toHtmlEscaped())));
    // Cambia la instancia con `f` y lo manda
    auto cambia = [this, id](const QString& que, auto f) {
        Documento n = l_->documento();
        Instancia* x = n.instancia(id);
        if (!x) return;
        f(*x);
        if (n != l_->documento()) l_->cambia(n, que);
    };

    auto* eid = new QLineEdit(i.id, cuerpo_);
    form_->addRow(QStringLiteral("Id"), eid);
    connect(eid, &QLineEdit::editingFinished, this, [this, eid, id] {
        const QString nuevo = eid->text().trimmed();
        if (nuevo == id) return;
        Documento n = l_->documento();
        const QString m = motivo_id_invalido(n, nuevo);
        if (!m.isEmpty()) {
            eid->setToolTip(m);
            eid->setStyleSheet(QStringLiteral("color: #c00"));
            emit l_->mensaje(QStringLiteral("%1: %2").arg(nuevo, m));
            return;
        }
        n.renombra(id, nuevo);
        l_->cambia(n, QStringLiteral("renombrar %1").arg(nuevo));
        l_->elige_instancia(nuevo);
    });

    auto* giro = new QComboBox(cuerpo_);
    for (int g : {0, 90, 180, 270}) giro->addItem(QStringLiteral("%1°").arg(g), g);
    giro->setCurrentIndex(i.giro / 90);
    form_->addRow(QStringLiteral("Giro"), giro);
    connect(giro, &QComboBox::activated, this, [cambia, giro](int) {
        cambia(QStringLiteral("girar"), [g = giro->currentData().toInt()](Instancia& x) { x.giro = g; });
    });

    auto* esc = new QDoubleSpinBox(cuerpo_);
    esc->setRange(0.25, 4.0);
    esc->setSingleStep(0.25);
    esc->setDecimals(2);
    esc->setValue(i.escala);
    form_->addRow(QStringLiteral("Escala"), esc);
    connect(esc, &QDoubleSpinBox::editingFinished, this, [cambia, esc] {
        cambia(QStringLiteral("escalar"), [v = esc->value()](Instancia& x) { x.escala = v; });
    });

    if (i.clase == Instancia::Mcu) {
        auto* det = new QCheckBox(QStringLiteral("Dibujo detallado, con el nombre de cada patilla"), cuerpo_);
        det->setChecked(i.detalle);
        form_->addRow(QString(), det);
        connect(det, &QCheckBox::toggled, this, [cambia](bool s) {
            cambia(QStringLiteral("dibujo del MCU"), [s](Instancia& x) { x.detalle = s; });
        });
        if (const FichaMcu* m = c->mcu(i.tipo))
            form_->addRow(new QLabel(QStringLiteral("<small>%1, %2 patillas</small>")
                                         .arg(m->encapsulado.toHtmlEscaped()).arg(m->patillas.size()), cuerpo_));
        return;
    }
    const FichaPieza* f = c->pieza(i.tipo);
    if (!f) return;
    if (!f->resumen.isEmpty()) {
        auto* r = new QLabel(QStringLiteral("<small>%1</small>").arg(f->resumen.toHtmlEscaped()), cuerpo_);
        r->setWordWrap(true);
        form_->addRow(r);
    }
    if (!f->series.isEmpty()) {
        auto* n = new QSpinBox(cuerpo_);
        n->setRange(1, 64);
        n->setValue(i.n_serie);
        QStringList ps;
        for (const Serie& s : f->series) ps << s.prefijo + QStringLiteral("…");
        n->setToolTip(QStringLiteral("Cuántos terminales de %1 se dibujan").arg(ps.join(QStringLiteral(", "))));
        form_->addRow(QStringLiteral("Terminales %1").arg(ps.join(QStringLiteral(", "))), n);
        connect(n, &QSpinBox::editingFinished, this, [cambia, n] {
            cambia(QStringLiteral("terminales"), [v = n->value()](Instancia& x) { x.n_serie = v; });
        });
    }
    for (const Atributo& a : f->atributos) {
        auto* e = new QLineEdit(i.atributos.value(a.nombre), cuerpo_);
        e->setPlaceholderText(a.omision);
        e->setToolTip(a.texto);
        form_->addRow(a.nombre, e);
        connect(e, &QLineEdit::editingFinished, this, [cambia, e, n = a.nombre] {
            cambia(QStringLiteral("%1=").arg(n), [n, v = e->text().trimmed()](Instancia& x) {
                if (v.isEmpty()) x.atributos.remove(n);
                else x.atributos.insert(n, v);
            });
        });
    }
}

void PanelPropiedades::conexion(int k)
{
    const Documento& d = l_->documento();
    const Conexion c = d.conexiones.value(k);
    titulo_->setText(QStringLiteral("<b>Conexión</b> %1 — %2").arg(c.a.toHtmlEscaped(), c.b.toHtmlEscaped()));
    auto cambia = [this, k](const QString& que, auto f) {
        Documento n = l_->documento();
        if (k >= n.conexiones.size()) return;
        f(n, n.conexiones[k]);
        if (n != l_->documento()) l_->cambia(n, que);
    };
    auto* t = new QComboBox(cuerpo_);
    t->addItems({QStringLiteral("Curva"), QStringLiteral("Tramos")});
    t->setCurrentIndex(c.tramos ? 1 : 0);
    form_->addRow(QStringLiteral("Trazado"), t);
    connect(t, &QComboBox::activated, this, [this, t, c](int) {
        if ((t->currentIndex() == 1) != c.tramos) l_->alterna_trazado();
    });
    auto* vis = new QCheckBox(QStringLiteral("Se ve en la placa"), cuerpo_);
    vis->setChecked(c.visible);
    vis->setToolTip(QStringLiteral("Una conexión oculta une igual: es lo que va por debajo de la placa"));
    form_->addRow(QString(), vis);
    connect(vis, &QCheckBox::toggled, this, [cambia](bool s) {
        cambia(s ? QStringLiteral("mostrar") : QStringLiteral("ocultar"), [s](Documento&, Conexion& x) { x.visible = s; });
    });
    // El bus es de la red entera
    const Redes& r = l_->redes();
    QStringList red;
    for (const Red& x : r.redes)
        if (x.terminales.contains(c.a)) red = x.terminales;
    bool es_bus = false;
    for (const QString& b : d.bus) es_bus = es_bus || red.contains(b);
    auto* bus = new QCheckBox(QStringLiteral("La red es un bus"), cuerpo_);
    bus->setChecked(es_bus);
    bus->setToolTip(QStringLiteral("bus=\"si\": varias salidas a la vez no son un cortocircuito (I2C, 1-Wire, RESET...)"));
    form_->addRow(QString(), bus);
    connect(bus, &QCheckBox::toggled, this, [cambia, red, a = c.a](bool s) {
        cambia(QStringLiteral("bus"), [s, red, a](Documento& n, Conexion&) {
            n.bus.erase(std::remove_if(n.bus.begin(), n.bus.end(), [&](const QString& b) { return red.contains(b); }),
                        n.bus.end());
            if (s) n.bus << a;
        });
    });
    QString nodo;
    for (const Red& x : r.redes)
        if (x.terminales.contains(c.a)) nodo = x.nodo;
    form_->addRow(QStringLiteral("Nodo"), new QLabel(nodo.toHtmlEscaped(), cuerpo_));
    form_->addRow(new QLabel(QStringLiteral("<small>%1</small>").arg(red.join(QStringLiteral(", ")).toHtmlEscaped()), cuerpo_));
}

void PanelPropiedades::placa()
{
    const Documento& d = l_->documento();
    titulo_->setText(QStringLiteral("<b>La placa</b>"));
    auto* n = new QLineEdit(d.nombre, cuerpo_);
    form_->addRow(QStringLiteral("Nombre"), n);
    connect(n, &QLineEdit::editingFinished, this, [this, n] {
        Documento x = l_->documento();
        x.nombre = n->text().trimmed();
        if (!x.nombre.isEmpty() && x != l_->documento()) l_->cambia(x, QStringLiteral("nombre"));
    });
    const bool a_mano = d.marco.isValid() && !d.marco.isEmpty();
    auto* m = new QCheckBox(QStringLiteral("Marco a mano"), cuerpo_);
    m->setChecked(a_mano);
    m->setToolTip(QStringLiteral("Sin marcar, el marco es lo que ocupan las piezas y 3 mm más"));
    form_->addRow(QString(), m);
    connect(m, &QCheckBox::toggled, this, [this](bool s) { l_->marco_a_mano(s); });
    const QRectF r = marco_efectivo(d, *l_->catalogo());
    form_->addRow(QStringLiteral("Marco"), new QLabel(QStringLiteral("%1 × %2 mm").arg(r.width(), 0, 'f', 1).arg(r.height(), 0, 'f', 1), cuerpo_));
    form_->addRow(QStringLiteral("Piezas"), new QLabel(QString::number(d.instancias.size()), cuerpo_));
    form_->addRow(QStringLiteral("Conexiones"), new QLabel(QString::number(d.conexiones.size()), cuerpo_));
    auto* ayuda = new QLabel(QStringLiteral(
        "<small>Arrastra piezas desde la biblioteca. Ctrl+clic en los terminales para elegirlos, y "
        "Ctrl+U (curva) o Ctrl+T (tramos) para unirlos: con más de dos, en estrella desde el primero. "
        "R gira, Supr borra, las flechas mueven (con Mayúsculas, 1 mm). En una conexión de tramos "
        "elegida, arrastra un tramo para moverlo y haz doble clic para añadir un codo. Ctrl+rueda: zoom.</small>"),
        cuerpo_);
    ayuda->setWordWrap(true);
    form_->addRow(ayuda);
}

} // namespace mcusim::build
