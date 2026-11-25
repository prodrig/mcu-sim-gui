// =============================================================================
// main_build.cpp — mcu-sim-build: dibujar placas para mcu-sim
// (doc/analisis_build.md de mcu-sim)
//
//   mcu-sim-build [placa.xml]
//
// Abre la placa si se le da (una hecha con mcu-sim-build, o cualquiera, que
// se reconstruye en rejilla); si no, una nueva.
// =============================================================================
#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>

#include "ventana_build.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("mcu-sim-build"));
    QApplication::setOrganizationName(QStringLiteral("mcu-sim"));
    QApplication::setApplicationVersion(QStringLiteral(MCU_SIM_GUI_VERSION));
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Dibuja placas para mcu-sim"));
    p.addHelpOption();
    p.addVersionOption();
    p.addPositionalArgument(QStringLiteral("placa"), QStringLiteral("La placa que se abre (XML)"));
    p.process(app);
    mcusim::build::VentanaBuild v;
    v.show();
    if (!p.positionalArguments().isEmpty()) {
        QString e;
        if (!v.abre(p.positionalArguments().front(), &e))
            QMessageBox::warning(&v, QStringLiteral("mcu-sim-build"), e);
    }
    return app.exec();
}
