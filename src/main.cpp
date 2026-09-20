// =============================================================================
// main.cpp — mcu-sim-gui
//
// De momento abre una ventana y dice lo que va a ser. El plan por fases está en
// `doc/plan_dos_procesos.md`; esto es la fase 0: que la cadena de herramientas
// exista y compile en las tres plataformas antes de escribir nada encima.
// =============================================================================
#include <QApplication>

#include "ventana_principal.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    QApplication::setApplicationName("mcu-sim-gui");
    QApplication::setApplicationVersion("0.1.0");
    QApplication::setOrganizationName("mcu-sim");

    mcusim::VentanaPrincipal v;
    v.show();

    return app.exec();
}
