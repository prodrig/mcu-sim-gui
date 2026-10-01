#include "panel.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QWidget>

namespace mcusim {

namespace {

// El control de un mando, y lo que ordena. Nace desactivado.
QWidget* control_de(Panel* panel, const PiezaGui& p, const MandoGui& m, QWidget* padre)
{
    const quint16 pz = quint16(p.idx), md = quint16(m.idx);
    const float lo = float(m.min), hi = float(m.max);
    QWidget* w = nullptr;
    QString ayuda;
    if (m.tipo == QLatin1String("interruptor")) {
        auto* c = new QCheckBox(m.nombre, padre);
        QObject::connect(c, &QCheckBox::toggled, panel,
                         [=](bool si) { emit panel->orden(pz, md, si ? hi : lo); });
        ayuda = QObject::tr("Marcado ordena %1; desmarcado, %2.").arg(hi).arg(lo);
        w = c;
    } else if (m.tipo == QLatin1String("continuo")) {
        auto* s = new QSlider(Qt::Horizontal, padre);
        s->setRange(0, 1000);               // de min a max, en milésimas
        QObject::connect(s, &QSlider::valueChanged, panel, [=](int v) {
            emit panel->orden(pz, md, lo + (hi - lo) * float(v) / 1000.f);
        });
        ayuda = QObject::tr("%1: de %2 a %3.").arg(m.nombre).arg(lo).arg(hi);
        w = s;
    } else {                                // "boton", y lo que no se conozca
        auto* b = new QPushButton(m.nombre, padre);
        QObject::connect(b, &QPushButton::pressed, panel,
                         [=] { emit panel->orden(pz, md, hi); });
        QObject::connect(b, &QPushButton::released, panel,
                         [=] { emit panel->orden(pz, md, lo); });
        ayuda = QObject::tr("Mientras esta hundido ordena %1; al soltarlo, %2.")
                    .arg(hi).arg(lo);
        w = b;
    }
    w->setObjectName(QStringLiteral("mando:%1:%2").arg(p.idx).arg(m.idx));
    w->setEnabled(false);
    w->setToolTip(ayuda);
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
    for (const MandoGui& m : p.mandos) {
        QWidget* c = control_de(panel, p, m, g);
        controles.push_back(c);
        f->addRow(c);
    }
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
