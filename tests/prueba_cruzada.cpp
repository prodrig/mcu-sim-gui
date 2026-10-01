// =============================================================================
// prueba_cruzada.cpp — la Sesion de esta ventana contra el mcu-sim DE VERDAD
//
// Las otras pruebas usan un modelo falso, y la de `mcu-sim` (`make gui-saludo`)
// una ventana falsa. Esta junta los dos de verdad: arranca el `mcu-sim` que se
// le diga con `--gui`, hace el saludo con la misma `Sesion` que usa la ventana,
// comprueba que la placa de la Discovery se construye entera y arranca.
//
// Necesita un `mcu-sim` compilado y su árbol de fuentes, porque las placas y
// los firmwares están allí. Se le dicen con dos variables de entorno:
//
//   MCU_SIM      el ejecutable          (…/mcu-sim/src/build/mcu-sim)
//   MCU_SIM_SRC  el directorio src/     (…/mcu-sim/src)
//
// Sin ellas se SALTA -código 77, que ctest cuenta como saltada y no como
// fallo-: en el CI de este repositorio no hay un mcu-sim, y no tiene por qué.
// =============================================================================
#include <QCoreApplication>
#include <QProcess>

#include "comun.h"
#include "sesion.h"

using namespace mcusim;
using namespace mcusim::proto;
using namespace prueba;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QString sim = qEnvironmentVariable("MCU_SIM");
    const QString src = qEnvironmentVariable("MCU_SIM_SRC");
    if (sim.isEmpty() || src.isEmpty()) {
        std::printf("  [SALTA] sin MCU_SIM y MCU_SIM_SRC no hay un mcu-sim contra el que "
                    "probar\n");
        return 77;
    }

    for (int vuelta = 0; vuelta < 2; ++vuelta) {
        const bool arranca = vuelta == 0;
        std::printf(arranca ? "G1 Saludo y arranque, con placas/discovery_min.xml\n"
                            : "G2 Saludo y T_PARA, sin simular\n");
        Sesion s;
        s.escucha(QHostAddress::LocalHost, 0);
        int listos = 0, fines = 0;
        quint32 motivo = 99; quint64 t = 1;
        QObject::connect(&s, &Sesion::listo, [&] { ++listos; });
        QObject::connect(&s, &Sesion::fin, [&](quint32 m, qint32, quint64 x) {
            ++fines; motivo = m; t = x;
        });
        QProcess p;
        p.setWorkingDirectory(src);
        p.start(sim, {"placas/discovery_min.xml", "verif/fw/blinky/blinky.bin", "50",
                      "--gui", QStringLiteral("127.0.0.1:%1").arg(s.puerto())});
        comprueba(espera([&] { return listos == 1; }, 20000),
                  "mcu-sim se conecta, saluda y dice T_LISTO");
        const PlacaGui& pl = s.placa();
        bool todas = !pl.piezas.isEmpty();
        for (const PiezaGui& pz : pl.piezas) todas = todas && pz.en_placa;
        comprueba(pl.piezas.size() == 11 && todas && pl.avisos.isEmpty() &&
                  pl.n_interesantes() == 8 && pl.n_mandos() == 2,
                  "la placa se construye entera: 11 piezas, todas casadas con su "
                  "componente, 8 indicadores y 2 mandos");
        comprueba(s.hola().value("modo") == "simula" && s.version() == 1,
                  "en modo simula y con la version 1");
        if (arranca) {
            comprueba(s.arranca(), "Arrancar");
            comprueba(espera([&] { return fines == 1; }, 60000) && motivo == M_VENTANA &&
                      t == 50110000ull,
                      "T_FIN al acabar la ventana, en 50 ms y 110 us");
        } else {
            comprueba(s.para(), "Parar antes de arrancar");
            comprueba(espera([&] { return fines == 1; }, 20000) && motivo == M_PARA && t == 0,
                      "T_FIN con motivo M_PARA y el tiempo en cero");
        }
        comprueba(p.waitForFinished(30000) && p.exitStatus() == QProcess::NormalExit &&
                  p.exitCode() == 0,
                  "y mcu-sim termina con codigo 0");
    }
    return resultado();
}
