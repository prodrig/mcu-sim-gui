#include "ventana_principal.h"

#include <QLabel>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include "protocolo.h"

namespace mcusim {

VentanaPrincipal::VentanaPrincipal(QWidget* padre)
    : QMainWindow(padre)
{
    setWindowTitle(tr("mcu-sim-gui"));
    resize(900, 600);

    auto* centro = new QWidget(this);
    auto* caja   = new QVBoxLayout(centro);

    auto* texto = new QLabel(
        tr("<h2>mcu-sim-gui</h2>"
           "<p>Contraparte de visualizacion grafica de <b>mcu-sim</b>, en la "
           "opcion de <b>dos procesos</b>.</p>"
           "<p>Todavia no hay nada que ver: esto es la fase 0 del plan, la que "
           "comprueba que la cadena de herramientas existe. El plan esta en "
           "<code>doc/plan_dos_procesos.md</code> y el protocolo en "
           "<code>doc/protocolo.md</code>.</p>"),
        centro);
    texto->setWordWrap(true);
    texto->setAlignment(Qt::AlignTop);
    caja->addWidget(texto);

    setCentralWidget(centro);

    statusBar()->showMessage(
        tr("protocolo v%1, puerto por omision %2")
            .arg(proto::VERSION_PROTO)
            .arg(proto::PUERTO_OMISION));
}

} // namespace mcusim
