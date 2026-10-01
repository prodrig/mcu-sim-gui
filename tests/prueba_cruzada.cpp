// =============================================================================
// prueba_cruzada.cpp — la Sesion de esta ventana contra el mcu-sim DE VERDAD
//
// Las otras pruebas usan un modelo falso, y la de `mcu-sim` (`make gui-saludo`)
// una ventana falsa. Esta junta los dos de verdad: arranca el `mcu-sim` que se
// le diga con `--gui`, hace el saludo con la misma `Sesion` que usa la ventana,
// comprueba que la placa de la Discovery se construye entera y arranca. Y desde
// la fase 4, que el LED parpadea en las muestras que llegan: seis flancos
// separados 100 ms, que es lo que programa el blinky.
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
        QVector<quint64> ts;
        QVector<float>   led;
        quint64 t_inst = 0;
        QObject::connect(&s, &Sesion::instantanea, [&](quint64 t, quint32) { t_inst = t; });
        QObject::connect(&s, &Sesion::muestra, [&](quint16, float v) {
            ts.push_back(t_inst);
            led.push_back(v);
        });
        quint32 motivo = 99; quint64 t = 1;
        QObject::connect(&s, &Sesion::listo, [&] { ++listos; });
        QObject::connect(&s, &Sesion::fin, [&](quint32 m, qint32, quint64 x) {
            ++fines; motivo = m; t = x;
        });
        QProcess p;
        p.setWorkingDirectory(src);
        p.start(sim, {"placas/discovery_min.xml", "verif/fw/blinky/blinky.bin", "700",
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
            // encendido de LD4, la tercera pieza: su primer observable
            quint16 id = 0;
            for (const PiezaGui& pz : pl.piezas)
                if (pz.id == "LD4" && !pz.observables.isEmpty())
                    id = quint16(pz.observables[0].id_obs);
            comprueba(s.suscribe(1000000, {id}), "suscrito a encendido de LD4, cada 1 ms");
            comprueba(s.arranca(), "Arrancar");
            comprueba(espera([&] { return fines == 1; }, 60000) && motivo == M_VENTANA &&
                      t == 700110000ull,
                      "T_FIN al acabar la ventana, en 700 ms y 110 us");
            QVector<quint64> flancos;
            for (int k = 1; k < led.size(); ++k)
                if (led[k] != led[k - 1]) flancos.push_back(ts[k]);
            bool cada_100 = flancos.size() == 6;
            for (int k = 1; k < flancos.size(); ++k)
                cada_100 = cada_100 && flancos[k] - flancos[k - 1] == 100000000ull;
            comprueba(ts.size() == 700 && cada_100,
                      "700 muestras, y el LED parpadea en ellas: seis flancos separados "
                      "100 ms, como programa el blinky");
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
