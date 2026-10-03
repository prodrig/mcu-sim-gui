#include "panel.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QWidget>

#include <memory>

namespace mcusim {

namespace {

// Un mando de tipo boton tiene DOS controles, el dedo y el «switch», y lo que
// el modelo recibe es uno O el otro. Este es su estado, compartido por los dos.
struct EstadoBoton {
    bool dedo = false;        // el boton momentaneo, hundido
    bool fijo = false;        // el «switch», marcado
    bool enviado = false;     // lo ultimo que se ordeno: no se repite
};

// El control de un mando, y lo que ordena. Nace desactivado. Los widgets que
// se encienden y se apagan van a `controles`; lo que se devuelve es lo que se
// coloca en el recuadro.
QWidget* control_de(Panel* panel, const PiezaGui& p, const MandoGui& m, QWidget* padre,
                    QVector<QWidget*>& controles)
{
    const quint16 pz = quint16(p.idx), md = quint16(m.idx);
    const float lo = float(m.min), hi = float(m.max);
    QWidget* w = nullptr;
    QString ayuda;
    if (m.tipo == QLatin1String("interruptor")) {
        auto* c = new QCheckBox(m.nombre, padre);
        c->setChecked(m.valor >= (m.min + m.max) / 2);   // donde esta el modelo
        QObject::connect(c, &QCheckBox::toggled, panel,
                         [=](bool si) { emit panel->orden(pz, md, si ? hi : lo); });
        ayuda = QObject::tr("Marcado ordena %1; desmarcado, %2.").arg(hi).arg(lo);
        w = c;
    } else if (m.tipo == QLatin1String("continuo")) {
        // El deslizador, con su NOMBRE y su VALOR al lado: sin el número no se
        // sabe qué se está pidiendo. Nace donde está el modelo (el `valor` del
        // catálogo), y colocarlo ahí no ordena nada: no es la mano.
        auto* fila = new QWidget(padre);
        auto* h = new QHBoxLayout(fila);
        h->setContentsMargins(0, 0, 0, 0);
        auto* s = new QSlider(Qt::Horizontal, fila);
        s->setObjectName(QStringLiteral("mando:%1:%2").arg(p.idx).arg(m.idx));
        s->setRange(0, 1000);               // de min a max, en milésimas
        auto* num = new QLabel(fila);
        num->setObjectName(QStringLiteral("valor:%1:%2").arg(p.idx).arg(m.idx));
        num->setMinimumWidth(num->fontMetrics().horizontalAdvance(QStringLiteral("00000.0")));
        auto escribe = [=](int v) {
            num->setText(QString::number(double(lo + (hi - lo) * float(v) / 1000.f), 'g', 4));
        };
        const double f = hi > lo ? (m.valor - m.min) / (m.max - m.min) : 0.0;
        {
            const QSignalBlocker quieto(s);
            s->setValue(qBound(0, int(f * 1000.0 + 0.5), 1000));
        }
        escribe(s->value());
        QObject::connect(s, &QSlider::valueChanged, panel, [=](int v) {
            escribe(v);
            emit panel->orden(pz, md, lo + (hi - lo) * float(v) / 1000.f);
        });
        s->setEnabled(false);
        s->setToolTip(QObject::tr("%1: de %2 a %3.").arg(m.nombre).arg(lo).arg(hi));
        controles.push_back(s);
        h->addWidget(new QLabel(m.nombre, fila));
        h->addWidget(s, 1);
        h->addWidget(num);
        return fila;
    } else {                                // "boton", y lo que no se conozca
        // DOS CONTROLES PARA UN MANDO. El de siempre, que es un dedo: la
        // pulsacion dura lo que dura el raton abajo. Y el «switch», que la deja
        // puesta hasta la siguiente vez que se toque: es lo que hace falta para
        // depurar paso a paso con el boton pulsado desde OTRA ventana -la del
        // IDE-, sin tener que sujetar el raton aqui.
        //
        // El mando esta hundido si lo esta CUALQUIERA de los dos, y solo se
        // ordena cuando eso cambia: con el «switch» puesto, el dedo no ordena
        // nada -ni al bajar ni, sobre todo, al subir, que soltaria lo que el
        // «switch» sujeta-; y quitar el «switch» con el dedo abajo tampoco.
        auto estado = std::make_shared<EstadoBoton>();
        auto ordena = [=] {
            const bool ahora = estado->dedo || estado->fijo;
            if (ahora == estado->enviado) return;
            estado->enviado = ahora;
            emit panel->orden(pz, md, ahora ? hi : lo);
        };
        auto* fila = new QWidget(padre);
        auto* h = new QHBoxLayout(fila);
        h->setContentsMargins(0, 0, 0, 0);
        auto* b = new QPushButton(m.nombre, fila);
        b->setObjectName(QStringLiteral("mando:%1:%2").arg(p.idx).arg(m.idx));
        QObject::connect(b, &QPushButton::pressed, panel,
                         [=] { estado->dedo = true;  ordena(); });
        QObject::connect(b, &QPushButton::released, panel,
                         [=] { estado->dedo = false; ordena(); });
        b->setToolTip(QObject::tr("Mientras esta hundido ordena %1; al soltarlo, %2. "
                                  "Con el «switch» puesto no ordena nada.")
                          .arg(hi).arg(lo));
        auto* sw = new QPushButton(QStringLiteral("switch"), fila);
        sw->setObjectName(QStringLiteral("fija:%1:%2").arg(p.idx).arg(m.idx));
        sw->setCheckable(true);
        QObject::connect(sw, &QPushButton::toggled, panel,
                         [=](bool si) { estado->fijo = si; ordena(); });
        sw->setToolTip(QObject::tr("Cada pulsacion cambia %1 entre hundido (%2) y suelto "
                                   "(%3), y lo deja asi: para seguir con el hundido "
                                   "mientras se trabaja en otra ventana.")
                           .arg(m.nombre).arg(hi).arg(lo));
        h->addWidget(b, 1);
        h->addWidget(sw);
        for (QPushButton* x : {b, sw}) {
            x->setEnabled(false);
            controles.push_back(x);
        }
        return fila;
    }
    w->setObjectName(QStringLiteral("mando:%1:%2").arg(p.idx).arg(m.idx));
    w->setEnabled(false);
    w->setToolTip(ayuda);
    controles.push_back(w);
    return w;
}

QGroupBox* recuadro_de(Panel* panel, const PiezaGui& p, QWidget* padre,
                       QHash<quint16, QLabel*>& etiquetas, QVector<QWidget*>& controles)
{
    QString titulo = QStringLiteral("%1 · %2").arg(p.id, p.tipo);
    if (!p.conectada) titulo += QStringLiteral(" (desoldada)");
    auto* g = new QGroupBox(titulo, padre);
    g->setObjectName(QStringLiteral("pieza:%1").arg(p.idx));
    auto* f = new QFormLayout(g);

    for (const PatillaGui& t : p.patillas) {
        auto* l = new QLabel(t.nodo, g);
        l->setObjectName(QStringLiteral("patilla:%1:%2").arg(p.idx).arg(t.nombre));
        f->addRow(t.nombre + QStringLiteral(" →"), l);
    }
    int ocultos = 0;
    for (const ObservableGui& o : p.observables) {
        if (!o.interesante) { ++ocultos; continue; }
        auto* l = new QLabel(QStringLiteral("—"), g);
        l->setObjectName(QStringLiteral("obs:%1").arg(o.id_obs));
        etiquetas.insert(quint16(o.id_obs), l);
        QString nombre = o.nombre;
        if (!o.unidad.isEmpty()) nombre += QStringLiteral(" (%1)").arg(o.unidad);
        f->addRow(QStringLiteral("<b>%1</b>").arg(nombre), l);
    }
    for (const MandoGui& m : p.mandos)
        f->addRow(control_de(panel, p, m, g, controles));
    if (ocultos > 0) {
        auto* l = new QLabel(QObject::tr("+%n observable(s) sin pintar", nullptr, ocultos), g);
        l->setObjectName(QStringLiteral("ocultos:%1").arg(p.idx));
        l->setEnabled(false);
        f->addRow(l);
    }
    return g;
}

} // namespace

Panel::Panel(const PlacaGui& placa, QWidget* padre) : QWidget(padre)
{
    setObjectName(QStringLiteral("panel"));
    auto* rejilla = new QGridLayout(this);
    const int columnas = 3;
    QHash<quint16, QLabel*> etiquetas;
    for (int i = 0; i < placa.piezas.size(); ++i)
        rejilla->addWidget(recuadro_de(this, placa.piezas[i], this, etiquetas, controles_),
                           i / columnas,
                           i % columnas, Qt::AlignTop);
    rejilla->setRowStretch(int((placa.piezas.size() + columnas - 1) / columnas), 1);
    for (const PiezaGui& p : placa.piezas)
        for (const ObservableGui& o : p.observables)
            if (QLabel* l = etiquetas.value(quint16(o.id_obs), nullptr)) {
                ind_.insert(quint16(o.id_obs), {l, o});
                pintados_.push_back(quint16(o.id_obs));
            }
}

QString Panel::texto_de(const ObservableGui& o, float valor)
{
    if (o.min == 0 && o.max == 1 && o.unidad.isEmpty())
        return valor >= 0.5f ? QStringLiteral("●") : QStringLiteral("○");
    QString t = QString::number(double(valor), 'g', 4);
    if (!o.unidad.isEmpty()) t += QLatin1Char(' ') + o.unidad;
    return t;
}

void Panel::pon_valor(quint16 id_obs, float valor)
{
    const auto it = ind_.constFind(id_obs);
    if (it == ind_.constEnd()) return;
    const QString t = texto_de(it->obs, valor);
    if (it->etiqueta->text() != t) it->etiqueta->setText(t);
}

void Panel::activa_mandos(bool si)
{
    activos_ = si;
    for (QWidget* w : controles_) w->setEnabled(si);
}

Panel* construye_panel(const PlacaGui& placa, QWidget* padre)
{
    return new Panel(placa, padre);
}

} // namespace mcusim
