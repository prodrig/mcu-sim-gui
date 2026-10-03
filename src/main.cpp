// =============================================================================
// main.cpp — mcu-sim-gui
//
// Abre la ventana, que escucha a que un `mcu-sim --gui` se conecte (fase 3 del
// plan, `doc/plan_dos_procesos.md`) y, desde la fase 7, lo lanza ella misma
// con «Simulacion > Lanzar mcu-sim» (Ctrl+L):
//
//   mcu-sim-gui [--config FICHERO] [--puerto N] [--lanza]
//
//   --config   la configuracion (JSON; la plantilla es `config.ejemplo.json`).
//              Por omision, `config.json` en el directorio actual si lo hay, y
//              si no, en el de configuracion del usuario. Si no existe, se
//              crea al lanzar por primera vez;
//   --puerto   donde escuchar, por encima de lo que diga la configuracion. Si
//              esta cogido, se escucha en otro, sin decir nada;
//   --lanza    lanza mcu-sim nada mas abrir, con la configuracion, sin dialogo.
//
// Y lanzarlo desde otra consola sigue valiendo:
//
//   mcu-sim placa.xml firmware.bin --gui            (o --gui localhost:N)
// =============================================================================
#include <cstdio>

#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

#include "configuracion.h"
#include "ventana_principal.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    QApplication::setApplicationName("mcu-sim-gui");
    QApplication::setApplicationVersion(QStringLiteral(MCU_SIM_GUI_VERSION));
    QApplication::setOrganizationName("mcu-sim");

    QCommandLineParser args;
    args.setApplicationDescription(
        QStringLiteral("La contraparte grafica de mcu-sim: lo lanza, lo ve y lo toca."));
    args.addHelpOption();
    args.addVersionOption();
    QCommandLineOption config(QStringLiteral("config"),
                              QStringLiteral("Fichero de configuracion (por omision %1).")
                                  .arg(mcusim::Configuracion::ruta_por_omision()),
                              QStringLiteral("FICHERO"));
    QCommandLineOption puerto(QStringLiteral("puerto"),
                              QStringLiteral("Puerto en el que escuchar, por encima de la "
                                             "configuracion (por omision %1).")
                                  .arg(mcusim::proto::PUERTO_OMISION),
                              QStringLiteral("N"));
    QCommandLineOption lanza(QStringLiteral("lanza"),
                             QStringLiteral("Lanza mcu-sim al abrir, con la configuracion."));
    args.addOption(config);
    args.addOption(puerto);
    args.addOption(lanza);
    args.process(app);

    mcusim::Configuracion c;
    QString error;
    const QString ruta = args.isSet(config) ? args.value(config)
                                            : mcusim::Configuracion::ruta_por_omision();
    if (!mcusim::Configuracion::lee(ruta, c, error)) {
        std::fprintf(stderr, "%s\n", qPrintable(error));
        return 1;
    }
    // Sin --config se ha buscado en dos sitios: si no estaba en ninguno, el
    // diálogo lo dice, con los dos
    if (!args.isSet(config)) c.buscadas = mcusim::Configuracion::rutas_candidatas();
    if (args.isSet(puerto)) {
        bool ok = false;
        const uint p = args.value(puerto).toUInt(&ok);
        if (!ok || p > 65535) {
            std::fprintf(stderr, "--puerto: '%s' no es un puerto\n",
                         qPrintable(args.value(puerto)));
            return 1;
        }
        c.puerto = quint16(p);
    }

    mcusim::VentanaPrincipal v(c);
    v.show();
    if (args.isSet(lanza)) QTimer::singleShot(0, &v, [&v] { v.lanza(); });

    return app.exec();
}
