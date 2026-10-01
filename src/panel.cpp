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

const char* const AUN_NO =
    "Todavia no hace nada: las ordenes llegan al modelo en la fase 5 del plan.";

QWidget* control_de(const PiezaGui& p, const MandoGui& m, QWidget* padre)
{
    QWidget* w = nullptr;
    if (m.tipo == QLatin1String("interruptor")) {
        w = new QCheckBox(m.nombre, padre);
    } else if (m.tipo == QLatin1String("continuo")) {
        auto* s = new QSlider(Qt::Horizontal, padre);
        s->setRange(0, 1000);               // de min a max, en milésimas
        w = s;
    } else {                                // "boton", y lo que no se conozca
        w = new QPushButton(m.nombre, padre);
    }
    w->setObjectName(QStringLiteral("mando:%1:%2").arg(p.idx).arg(m.idx));
    w->setEnabled(false);
    w->setToolTip(QString::fromUtf8(AUN_NO));
    return w;
}

QGroupBox* recuadro_de(const PiezaGui& p, QWidget* padre)
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
        QString nombre = o.nombre;
        if (!o.unidad.isEmpty()) nombre += QStringLiteral(" (%1)").arg(o.unidad);
        f->addRow(QStringLiteral("<b>%1</b>").arg(nombre), l);
    }
    for (const MandoGui& m : p.mandos) f->addRow(control_de(p, m, g));
    if (ocultos > 0) {
        auto* l = new QLabel(QObject::tr("+%n observable(s) sin pintar", nullptr, ocultos), g);
        l->setObjectName(QStringLiteral("ocultos:%1").arg(p.idx));
        l->setEnabled(false);
        f->addRow(l);
    }
    return g;
}

} // namespace

QWidget* construye_panel(const PlacaGui& placa, QWidget* padre)
{
    auto* w = new QWidget(padre);
    w->setObjectName(QStringLiteral("panel"));
    auto* rejilla = new QGridLayout(w);
    const int columnas = 3;
    for (int i = 0; i < placa.piezas.size(); ++i)
        rejilla->addWidget(recuadro_de(placa.piezas[i], w), i / columnas, i % columnas,
                           Qt::AlignTop);
    rejilla->setRowStretch(int((placa.piezas.size() + columnas - 1) / columnas), 1);
    return w;
}

} // namespace mcusim
