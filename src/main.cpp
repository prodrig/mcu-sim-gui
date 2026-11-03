// =============================================================================
// main.cpp — mcu-sim-gui
//
// Abre la ventana, que escucha a que un `mcu-sim --gui` se conecte (fase 3 del
// plan, `doc/plan_dos_procesos.md`) y, desde la fase 7, lo lanza ella misma
// con «Simulacion > Lanzar mcu-sim» (Ctrl+L):
//
//   mcu-sim-gui [--config FICHERO] [--puerto N] [--lanza] [--edit]
//   mcu-sim-gui --comprueba-3d
//
//   --config   la configuracion (JSON; la plantilla es `config.ejemplo.json`).
//              Por omision, `config.json` en el directorio actual si lo hay, y
//              si no, en el de configuracion del usuario. Si no existe, se
//              crea al lanzar por primera vez;
//   --puerto   donde escuchar, por encima de lo que diga la configuracion. Si
//              esta cogido, se escucha en otro, sin decir nada;
//   --lanza    lanza mcu-sim nada mas abrir, con la configuracion, sin dialogo;
//   --edit     deja EDITAR la ilustracion: colocar, girar y escalar las placas,
//              el lienzo, las lineas en tramos rectos, guardarlo en el XML y
//              abrir otro dibujo (plan §34 a §40). Sin el, la ilustracion solo
//              se mira: se ensenan y se esconden las conexiones, y se acerca o
//              se aleja (plan §45). Lo colocado, de la configuracion o del XML,
//              se ve igual.
//   --comprueba-3d  carga la escena de la vista 3D con un cubo, sin
//              ensenarla, dice si Qt Quick 3D y sus modulos de QML se han
//              podido cargar y de donde, y sale: 0 si si, 1 si no, 3 si esta
//              compilacion no lleva la vista 3D. Es la prueba de los paquetes
//              (ci/empaqueta-*.sh): lo que les falte solo se notaria al abrir
//              la ventana 3D. No la dibuja, porque eso depende de la maquina
//              -su OpenGL, Direct3D o Metal- y no del paquete.
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
#ifdef MCU_SIM_GUI_3D
#include "vista3d.h"
#endif

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
    QCommandLineOption editar(QStringLiteral("edit"),
                              QStringLiteral("Deja editar la ilustracion: colocar las placas, "
                                             "enrutar las lineas y abrir otro dibujo. Sin "
                                             "ella, solo se ensenan y se esconden las "
                                             "conexiones."));
    QCommandLineOption comprueba3d(QStringLiteral("comprueba-3d"),
                                   QStringLiteral("Comprueba que la vista 3D se puede cargar "
                                                  "(Qt Quick 3D y sus modulos de QML) y sale: "
                                                  "0 si, 1 no, 3 sin vista 3D."));
    args.addOption(config);
    args.addOption(puerto);
    args.addOption(lanza);
    args.addOption(editar);
    args.addOption(comprueba3d);
    args.process(app);

    if (args.isSet(comprueba3d)) {
#ifdef MCU_SIM_GUI_3D
        QString informe;
        const bool ok = mcusim::Vista3d::comprueba(informe);
        std::printf("%s\n%s\n", ok ? "vista 3D: bien" : "vista 3D: MAL", qPrintable(informe));
        std::fflush(stdout);
        return ok ? 0 : 1;
#else
        std::printf("vista 3D: esta compilacion no la lleva (MCU_SIM_GUI_3D)\n");
        std::fflush(stdout);
        return 3;
#endif
    }

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
    v.pon_editable(args.isSet(editar));
    v.show();
    if (args.isSet(lanza)) QTimer::singleShot(0, &v, [&v] { v.lanza(); });

    return app.exec();
}
