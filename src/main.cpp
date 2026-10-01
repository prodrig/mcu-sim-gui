// =============================================================================
// main.cpp — mcu-sim-gui
//
// Abre la ventana, que escucha en un puerto a que un `mcu-sim --gui` se
// conecte (fase 3 del plan, `doc/plan_dos_procesos.md`). El puerto es el 3344
// de `protocolo.h` salvo que se diga otro:
//
//   mcu-sim-gui [--puerto N]
//
// Y en la otra consola:
//
//   mcu-sim placa.xml firmware.bin --gui            (o --gui localhost:N)
// =============================================================================
#include <cstdio>

#include <QApplication>
#include <QCommandLineParser>

#include "ventana_principal.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    QApplication::setApplicationName("mcu-sim-gui");
    QApplication::setApplicationVersion("0.1.0");
    QApplication::setOrganizationName("mcu-sim");

    QCommandLineParser args;
    args.setApplicationDescription(
        QStringLiteral("La contraparte grafica de mcu-sim. Escucha a que un "
                       "'mcu-sim placa.xml --gui' se conecte."));
    args.addHelpOption();
    args.addVersionOption();
    QCommandLineOption puerto(QStringLiteral("puerto"),
                              QStringLiteral("Puerto en el que escuchar (por omision %1).")
                                  .arg(mcusim::proto::PUERTO_OMISION),
                              QStringLiteral("N"),
                              QString::number(mcusim::proto::PUERTO_OMISION));
    args.addOption(puerto);
    args.process(app);

    bool ok = false;
    const uint p = args.value(puerto).toUInt(&ok);
    if (!ok || p == 0 || p > 65535) {
        std::fprintf(stderr, "--puerto: '%s' no es un puerto\n",
                     qPrintable(args.value(puerto)));
        return 1;
    }

    mcusim::VentanaPrincipal v{quint16(p)};
    v.show();

    return app.exec();
}
