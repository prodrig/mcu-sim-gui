#include "ventana_principal.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include "panel.h"

namespace mcusim {

VentanaPrincipal::VentanaPrincipal(quint16 puerto, QWidget* padre)
    : QMainWindow(padre), puerto_(puerto)
{
    setWindowTitle(tr("mcu-sim-gui"));
    resize(1000, 700);

    auto* cuerpo = new QWidget(this);
    auto* caja   = new QVBoxLayout(cuerpo);

    auto* fila = new QHBoxLayout;
    resumen_ = new QLabel(cuerpo);
    resumen_->setObjectName(QStringLiteral("resumen"));
    resumen_->setTextFormat(Qt::RichText);
    resumen_->setWordWrap(true);
    arrancar_ = new QPushButton(tr("Arrancar"), cuerpo);
    arrancar_->setObjectName(QStringLiteral("arrancar"));
    parar_ = new QPushButton(tr("Parar"), cuerpo);
    parar_->setObjectName(QStringLiteral("parar"));
    fila->addWidget(resumen_, 1);
    fila->addWidget(arrancar_);
    fila->addWidget(parar_);
    caja->addLayout(fila);

    centro_ = new QScrollArea(cuerpo);
    centro_->setWidgetResizable(true);
    caja->addWidget(centro_, 1);
    setCentralWidget(cuerpo);

    connect(arrancar_, &QPushButton::clicked, this, [this] {
        if (ses_.arranca()) {
            arrancar_->setEnabled(false);
            statusBar()->showMessage(tr("simulando"));
        }
    });
    connect(parar_, &QPushButton::clicked, this, [this] {
        if (ses_.para()) parar_->setEnabled(false);
    });

    connect(&ses_, &Sesion::conectado, this, [this] {
        statusBar()->showMessage(tr("mcu-sim conectado; saludando"));
    });
    connect(&ses_, &Sesion::placa_lista, this, &VentanaPrincipal::pon_placa);
    connect(&ses_, &Sesion::listo, this, [this] {
        arrancar_->setEnabled(true);
        parar_->setEnabled(true);
        statusBar()->showMessage(tr("el modelo esta construido y esperando: pulsa Arrancar"));
    });
    connect(&ses_, &Sesion::fin, this, &VentanaPrincipal::termina);
    connect(&ses_, &Sesion::desconectado, this, [this](const QString& m) {
        arrancar_->setEnabled(false);
        parar_->setEnabled(false);
        if (!m.isEmpty())
            statusBar()->showMessage(tr("conexion cerrada: %1").arg(m));
    });
    connect(&ses_, &Sesion::problema, this, [this](const QString& t) {
        statusBar()->showMessage(t);
    });

    espera_modelo();
    if (!ses_.escucha(QHostAddress::LocalHost, puerto_))
        resumen_->setText(tr("<b>No se puede escuchar en el puerto %1</b>: %2")
                              .arg(puerto_).arg(ses_.error()));
}

void VentanaPrincipal::espera_modelo()
{
    arrancar_->setEnabled(false);
    parar_->setEnabled(false);
    resumen_->setText(
        tr("<b>Esperando a mcu-sim</b> en localhost:%1. Lanzalo desde una consola "
           "con <code>mcu-sim placa.xml firmware.bin --gui</code>.").arg(puerto_));
    auto* vacio = new QLabel(tr("Aqui aparecera la placa: una pieza por recuadro, con lo "
                                "que deja ver y lo que se le puede hacer."), centro_);
    vacio->setAlignment(Qt::AlignCenter);
    vacio->setEnabled(false);
    centro_->setWidget(vacio);
}

void VentanaPrincipal::pon_placa()
{
    const PlacaGui& p = ses_.placa();
    const auto& h = ses_.hola();
    QString fw = h.value(QStringLiteral("firmware"));
    if (fw.isEmpty()) fw = tr("sin firmware");
    // Una placa que no declara su MCU lo recibe implicito de `sim`, y entonces
    // no esta en el XML: lo dice T_HOLA.
    const QString mcus = p.mcus.isEmpty() ? h.value(QStringLiteral("mcu"))
                                          : p.mcus.join(QStringLiteral(", "));
    QString texto = tr("<b>%1</b> &nbsp; %2 &nbsp; %3 &nbsp; <i>%4 piezas, %5 indicadores, "
                       "%6 mandos</i>")
                        .arg(p.nombre.toHtmlEscaped(), mcus.toHtmlEscaped(),
                             fw.toHtmlEscaped())
                        .arg(p.piezas.size()).arg(p.n_interesantes()).arg(p.n_mandos());
    if (h.value(QStringLiteral("modo")) == QLatin1String("valida"))
        texto += tr(" &nbsp; <b>(solo validacion: no se simulara)</b>");
    resumen_->setText(texto);
    centro_->setWidget(construye_panel(p));
}

void VentanaPrincipal::termina(quint32 motivo, qint32 codigo, quint64 t_sim_ns)
{
    arrancar_->setEnabled(false);
    parar_->setEnabled(false);
    const QString por =
        motivo == proto::M_VENTANA ? tr("se agoto la ventana de simulacion")
      : motivo == proto::M_PARA    ? tr("se pidio parar")
      : motivo == proto::M_ERROR   ? tr("el modelo se rindio")
                                   : tr("el modelo se detuvo");
    statusBar()->showMessage(tr("terminado: %1, en t = %2 ms (codigo %3)")
                                 .arg(por)
                                 .arg(double(t_sim_ns) / 1e6, 0, 'f', 3)
                                 .arg(codigo));
}

} // namespace mcusim
