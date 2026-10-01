#include "ventana_principal.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
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
    relojes_ = new QLabel(cuerpo);
    relojes_->setObjectName(QStringLiteral("relojes"));
    fila->addWidget(resumen_, 1);
    fila->addWidget(relojes_);
    fila->addWidget(arrancar_);
    fila->addWidget(parar_);
    caja->addLayout(fila);

    centro_ = new QScrollArea(cuerpo);
    centro_->setWidgetResizable(true);
    caja->addWidget(centro_, 1);

    avisos_ = new QListWidget(cuerpo);
    avisos_->setObjectName(QStringLiteral("avisos"));
    avisos_->setMaximumHeight(120);
    caja->addWidget(avisos_);
    setCentralWidget(cuerpo);

    connect(arrancar_, &QPushButton::clicked, this, [this] {
        if (ses_.arranca()) {
            arrancar_->setEnabled(false);
            // Parar en marcha es la fase 6: hasta entonces, no se ofrece.
            parar_->setEnabled(false);
            parar_->setToolTip(tr("Parar con la simulacion en marcha llega en la fase 6 del plan."));
            statusBar()->showMessage(tr("simulando"));
        }
    });
    connect(parar_, &QPushButton::clicked, this, [this] {
        if (ses_.para()) parar_->setEnabled(false);
    });

    connect(&ses_, &Sesion::conectado, this, [this] {
        avisos_->clear();
        relojes_->clear();
        statusBar()->showMessage(tr("mcu-sim conectado; saludando"));
    });
    connect(&ses_, &Sesion::placa_lista, this, &VentanaPrincipal::pon_placa);
    connect(&ses_, &Sesion::listo, this, [this] {
        // Antes de arrancar: asi la secuencia se repite al picosegundo
        if (panel_ && !panel_->pintados().isEmpty())
            ses_.suscribe(PERIODO_NS, panel_->pintados());
        arrancar_->setEnabled(true);
        parar_->setEnabled(true);
        // Lo que se toque desde ya se aplica en t = 0 (doc/protocolo.md §5)
        if (panel_) panel_->activa_mandos(true);
        statusBar()->showMessage(tr("el modelo esta construido y esperando: pulsa Arrancar"));
    });
    connect(&ses_, &Sesion::fin, this, &VentanaPrincipal::termina);
    connect(&ses_, &Sesion::orden_hecha, this, &VentanaPrincipal::pon_eco);
    connect(&ses_, &Sesion::muestra, this, [this](quint16 id, float v) {
        if (panel_) panel_->pon_valor(id, v);
    });
    connect(&ses_, &Sesion::aviso, this, &VentanaPrincipal::pon_aviso);
    connect(&ses_, &Sesion::estado_modelo, this,
            [this](quint32, quint64 t, double p, quint64 d) { pon_relojes(t, p, d); });
    connect(&ses_, &Sesion::desconectado, this, [this](const QString& m) {
        arrancar_->setEnabled(false);
        parar_->setEnabled(false);
        apaga_mandos();
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
    panel_ = construye_panel(p);
    connect(panel_, &Panel::orden, this, [this](quint16 pieza, quint16 mando, float v) {
        if (!ses_.ordena(pieza, mando, v))
            statusBar()->showMessage(tr("no se puede ordenar: el modelo no esta esperando "
                                        "ni corriendo"));
    });
    centro_->setWidget(panel_);
}

void VentanaPrincipal::pon_aviso(quint32 nivel, quint64 t_sim_ns, const QString& origen,
                                 const QString& texto)
{
    static const char* const nombre[] = {"info", "aviso", "error", "fatal"};
    const QString n = QString::fromLatin1(nivel < 4 ? nombre[nivel] : "?");
    auto* it = new QListWidgetItem(QStringLiteral("[%1] t = %2 ms · %3: %4")
                                       .arg(n)
                                       .arg(double(t_sim_ns) / 1e6, 0, 'f', 3)
                                       .arg(origen, texto),
                                   avisos_);
    if (nivel >= proto::N_ERROR) it->setForeground(Qt::red);
    else if (nivel == proto::N_AVISO) it->setForeground(QColor(0xB0, 0x6A, 0x00));
    avisos_->scrollToBottom();
}

void VentanaPrincipal::pon_relojes(quint64 t_sim_ns, double t_pared_s, quint64 deltas)
{
    QString t = tr("t = %1 ms · pared %2 s · %3 deltas")
                    .arg(double(t_sim_ns) / 1e6, 0, 'f', 3)
                    .arg(t_pared_s, 0, 'f', 2)
                    .arg(deltas);
    if (ses_.perdidas() > 0)
        t += tr(" · <b>va por detras: %1 instantaneas perdidas</b>").arg(ses_.perdidas());
    relojes_->setText(t);
}

void VentanaPrincipal::pon_eco(quint64 t_sim_ns, quint16 pieza, quint16 mando, float valor,
                               quint32 resultado)
{
    // RES_OK no se dice: se ve en los indicadores. RES_RANGO tampoco: el
    // modelo ya manda su propio T_AVISO, con lo que pidio y lo que aplico.
    if (resultado == proto::RES_OK || resultado == proto::RES_RANGO) return;
    QString quien = tr("pieza %1, mando %2").arg(pieza).arg(mando);
    const PlacaGui& p = ses_.placa();
    if (pieza < p.piezas.size()) {
        const PiezaGui& pz = p.piezas[pieza];
        quien = pz.id;
        if (mando < pz.mandos.size()) quien += QStringLiteral(".") + pz.mandos[mando].nombre;
        else                          quien += tr(", mando %1").arg(mando);
    }
    const QString por =
        resultado == proto::RES_PIEZA ? tr("no existe esa pieza")
      : resultado == proto::RES_MANDO ? tr("esa pieza no tiene ese mando")
      : resultado == proto::RES_TARDE ? tr("llego tarde: se aplico al recibirla")
                                      : tr("resultado %1").arg(resultado);
    pon_aviso(resultado == proto::RES_TARDE ? proto::N_INFO : proto::N_AVISO, t_sim_ns,
              tr("ventana"), tr("orden a %1 (%2): %3").arg(quien).arg(valor).arg(por));
}

void VentanaPrincipal::termina(quint32 motivo, qint32 codigo, quint64 t_sim_ns)
{
    arrancar_->setEnabled(false);
    parar_->setEnabled(false);
    apaga_mandos();
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
