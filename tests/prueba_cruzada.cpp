// =============================================================================
// prueba_cruzada.cpp — la Sesion de esta ventana contra el mcu-sim DE VERDAD
//
// Las otras pruebas usan un modelo falso, y la de `mcu-sim` (`make gui-saludo`)
// una ventana falsa. Esta junta los dos de verdad: arranca el `mcu-sim` que se
// le diga con `--gui`, hace el saludo con la misma `Sesion` que usa la ventana,
// comprueba que la placa de la Discovery se construye entera y arranca. Y desde
// la fase 4, que el LED parpadea en las muestras que llegan: seis flancos
// separados 100 ms, que es lo que programa el blinky. Y desde la fase 5, que
// unas órdenes al pulsador B1 mandadas antes de arrancar vuelven con su eco en
// el instante exacto, y que el blinky —que no mira PA0— parpadea igual. Y desde
// la fase 6, G3: a demanda, un paso exacto, y T_PARA en marcha. Y G4, una
// placa SIN MCU (`placas/fuente_y_masa.xml`): la Fuente F1 limitando a sus
// 20 mA con el pulsador CORTO pulsado, y su `sobrecorriente`, una alarma.
// Y G5, un SISTEMA (`placas/nucleo_y_shield.xml`, versión 2 del protocolo):
// la Nucleo y un shield sin MCU, y el blinky encendiendo a la vez el LD2 de
// una placa y el LED del shield, que está en la otra. Y G6, una PILA
// (`placas/pila_pc104.xml`): un acople de tres conectores, lo que T_PLACA
// cuenta de cada placa para poder dibujarla, y el mismo pin en las tres. Y
// desde las ilustraciones, en G5, el dibujo de la Nucleo en T_ILUSTRACION. Y
// G7, desde la rama msp432: la LaunchPad MSP-EXP432P401R, un chip de TI con
// sus pines llamados `P1.0` y su LED parpadeando en las muestras. Y G8,
// desde la rama stm32f303: la STM32F3DISCOVERY con GPIO_IOToggle de ST, su
// dibujo, y la rosa encendiendose en orden, un LED cada 101 ms. Y G9, el
// sistema de los dos motores (`placas/nucleo_f446re_encoder_lcd_servo_2xmcc.xml`):
// nueve placas, el L298N, dos MotorCC con su encoder compartiendo dibujo, y el
// mando que los pone en marcha.
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
#include <QHash>
#include <QProcess>

#include <algorithm>
#include <cmath>

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
        struct Eco { quint64 t; quint16 p; float v; quint32 r; };
        QVector<Eco> ecos;
        QObject::connect(&s, &Sesion::orden_hecha,
                         [&](quint64 t, quint16 p, quint16, float v, quint32 r) {
                             ecos.push_back({t, p, v, r});
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
                  pl.n_interesantes() == 8 && pl.n_mandos() == 6,
                  "la placa se construye entera: 11 piezas, todas casadas con su "
                  "componente, 8 indicadores y 6 mandos -pulsar, rebote_ms y rebotes de "
                  "B1 y B2-");
        {
            double reb_b1 = -1, reb_b2 = -1;
            for (const PiezaGui& pz : pl.piezas)
                for (const MandoGui& m : pz.mandos)
                    if (m.nombre == "rebote_ms") (pz.id == "B1" ? reb_b1 : reb_b2) = m.valor;
            comprueba(reb_b1 == 2 && reb_b2 == 0,
                      "y el catalogo dice donde esta cada mando: B1 rebota 2 ms y B2 no");
        }
        comprueba(s.hola().value("modo") == "simula" && s.version() == 2,
                  "en modo simula y con la version 2");
        if (arranca) {
            // encendido de LD4, la tercera pieza: su primer observable
            quint16 id = 0;
            for (const PiezaGui& pz : pl.piezas)
                if (pz.id == "LD4" && !pz.observables.isEmpty())
                    id = quint16(pz.observables[0].id_obs);
            comprueba(s.suscribe(1000000, {id}), "suscrito a encendido de LD4, cada 1 ms");
            // Fase 5: B1 pulsado de 200 a 250 ms, y una orden a una pieza que no hay
            quint16 b1 = 0, pulsar = 0;
            for (const PiezaGui& pz : pl.piezas)
                if (pz.id == "B1" && !pz.mandos.isEmpty()) {
                    b1 = quint16(pz.idx);
                    pulsar = quint16(pz.mandos[0].idx);
                }
            comprueba(s.ordena(QVector<Orden>{{200000000ull, b1, pulsar, 1.f},
                                              {50000000ull, b1, pulsar, 0.f},
                                              {0, 99, 0, 1.f}}),
                      "tres ordenes antes de arrancar: pulsa B1 en 200 ms, lo suelta en "
                      "250, y una a la pieza 99");
            comprueba(s.arranca(RIT_LIBRE), "Arrancar, con ritmo libre");
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
            comprueba(ecos.size() == 3 && ecos[0].t == 200000000ull && ecos[0].p == b1 &&
                      ecos[0].v == 1.f && ecos[0].r == RES_OK &&
                      ecos[1].t == 250000000ull && ecos[1].v == 0.f && ecos[1].r == RES_OK &&
                      ecos[2].t == 250000000ull && ecos[2].p == 99 && ecos[2].r == RES_PIEZA,
                      "y tres ecos: B1 en exactamente 200 y 250 ms, con RES_OK, y la de la "
                      "pieza 99 en 250 ms, con RES_PIEZA");
        } else {
            comprueba(s.para(), "Parar antes de arrancar");
            comprueba(espera([&] { return fines == 1; }, 20000) && motivo == M_PARA && t == 0,
                      "T_FIN con motivo M_PARA y el tiempo en cero");
        }
        // waitForFinished() devuelve false si el proceso YA ha terminado, y
        // puede haber terminado mientras `espera` atendia los eventos: si no,
        // esto falla de vez en cuando sin que mcu-sim haya hecho nada mal.
        const bool acabo = p.state() == QProcess::NotRunning || p.waitForFinished(30000);
        comprueba(acabo && p.exitStatus() == QProcess::NormalExit &&
                  p.exitCode() == 0,
                  "y mcu-sim termina con codigo 0");
    }
    // -------------------------------------------------------------------------
    std::printf("G3 A demanda: un paso exacto, y parar en marcha\n");
    {
        Sesion s;
        s.escucha(QHostAddress::LocalHost, 0);
        int listos = 0, fines = 0;
        QVector<QPair<quint32, quint64>> ests;
        quint32 motivo = 99; quint64 t = 1;
        QObject::connect(&s, &Sesion::listo, [&] { ++listos; });
        QObject::connect(&s, &Sesion::estado_modelo, [&](quint32 f, quint64 x, double, quint64) {
            ests.push_back({f, x});
        });
        QObject::connect(&s, &Sesion::fin, [&](quint32 m, qint32, quint64 x) {
            ++fines; motivo = m; t = x;
        });
        QProcess p;
        p.setWorkingDirectory(src);
        // Sin ventana de tiempo: hasta que se diga parar
        p.start(sim, {"placas/discovery_min.xml", "verif/fw/blinky/blinky.bin",
                      "--gui", QStringLiteral("127.0.0.1:%1").arg(s.puerto())});
        comprueba(espera([&] { return listos == 1; }, 20000) && s.arranca(RIT_DEMANDA),
                  "saludo, y arranca a demanda, sin ventana de tiempo");
        comprueba(espera([&] { return s.pausada(); }, 10000) && ests.last().second == 0,
                  "el modelo dice que esta en pausa, en t = 0");
        comprueba(s.paso(100000000ull) &&
                      espera([&] { return s.pausada() && ests.last().second > 0; }, 20000) &&
                      ests.last().second == 100000000ull,
                  "un paso de 100 ms: otra vez en pausa en exactamente 100 ms");
        comprueba(s.para() && espera([&] { return fines == 1; }, 20000) &&
                      motivo == M_PARA && t == 100000000ull,
                  "Parar: T_FIN con motivo M_PARA, en los mismos 100 ms");
        const bool acabo = p.state() == QProcess::NotRunning || p.waitForFinished(30000);
        comprueba(acabo && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0 &&
                      p.readAllStandardOutput().contains("la ventana pidio parar"),
                  "y mcu-sim termina con codigo 0, diciendo que se le pidio parar");
    }
    // -------------------------------------------------------------------------
    std::printf("G4 Una placa sin MCU: la Fuente y su sobrecorriente\n");
    {
        Sesion s;
        s.escucha(QHostAddress::LocalHost, 0);
        int listos = 0, fines = 0;
        quint64 t_inst = 0;
        QHash<quint64, QHash<quint16, float>> m;      // instante -> id_obs -> valor
        QStringList avisos;
        QObject::connect(&s, &Sesion::listo, [&] { ++listos; });
        QObject::connect(&s, &Sesion::fin, [&](quint32, qint32, quint64) { ++fines; });
        QObject::connect(&s, &Sesion::instantanea, [&](quint64 t, quint32) { t_inst = t; });
        QObject::connect(&s, &Sesion::muestra, [&](quint16 id, float v) { m[t_inst][id] = v; });
        QObject::connect(&s, &Sesion::aviso,
                         [&](quint32, quint64, const QString&, const QString& x) { avisos << x; });
        QProcess p;
        p.setWorkingDirectory(src);
        p.start(sim, {"placas/fuente_y_masa.xml", "--ms=30",
                      "--gui", QStringLiteral("127.0.0.1:%1").arg(s.puerto())});
        comprueba(espera([&] { return listos == 1; }, 20000) &&
                      s.hola().value("mcu").isEmpty() && s.placa().mcus.isEmpty(),
                  "saluda sin MCU: mcu= vacio en T_HOLA y ningun <mcu> en la placa");
        int cor = -1, sob = -1, corto = -1;
        bool alarma_bien = false;
        for (const PiezaGui& pz : s.placa().piezas) {
            if (pz.id == "CORTO" && !pz.mandos.isEmpty()) corto = pz.idx;
            if (pz.id != "F1") continue;
            for (const ObservableGui& o : pz.observables) {
                if (o.nombre == "corriente") { cor = o.id_obs; alarma_bien = !o.alarma; }
                if (o.nombre == "sobrecorriente") {
                    sob = o.id_obs;
                    alarma_bien = alarma_bien && o.alarma;
                }
            }
        }
        comprueba(cor >= 0 && sob >= 0 && corto >= 0 && alarma_bien,
                  "F1 trae `corriente` y `sobrecorriente`, y solo la segunda es una alarma");
        s.suscribe(1000000, {quint16(cor), quint16(sob)});
        s.ordena(QVector<Orden>{{10000000ull, quint16(corto), 0, 1.f},
                                {10000000ull, quint16(corto), 0, 0.f}});
        comprueba(s.arranca(RIT_LIBRE) && espera([&] { return fines == 1; }, 30000),
                  "CORTO pulsado de 10 a 20 ms, y 30 ms simulados");
        const auto a = m.value(5000000ull), b = m.value(15000000ull), c = m.value(25000000ull);
        comprueba(a.value(quint16(sob), -1) == 0.f && a.value(quint16(cor)) > 8.f &&
                      a.value(quint16(cor)) < 9.f,
                  "a los 5 ms, unos 8,5 mA y sin sobrecorriente");
        comprueba(b.value(quint16(sob), -1) == 1.f &&
                      std::fabs(b.value(quint16(cor)) - 20.f) < 0.02f,
                  "a los 15 ms, con el corto, sus 20 mA justos y la sobrecorriente a 1");
        comprueba(c.value(quint16(sob), -1) == 0.f && c.value(quint16(cor)) > 8.f,
                  "y a los 25 ms, suelto, todo como al principio");
        comprueba(avisos.filter("F1: sobrecorriente").size() == 1,
                  "con un T_AVISO, uno solo, al entrar en limitacion");
        const bool acabo = p.state() == QProcess::NotRunning || p.waitForFinished(30000);
        comprueba(acabo && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0,
                  "y mcu-sim termina con codigo 0");
    }
    // -------------------------------------------------------------------------
    std::printf("G5 Un sistema: la Nucleo y un shield enchufado\n");
    {
        Sesion s;
        s.escucha(QHostAddress::LocalHost, 0);
        int listos = 0, fines = 0;
        quint64 t_inst = 0;
        QHash<quint64, QHash<quint16, float>> m;
        QObject::connect(&s, &Sesion::listo, [&] { ++listos; });
        QObject::connect(&s, &Sesion::fin, [&](quint32, qint32, quint64) { ++fines; });
        QObject::connect(&s, &Sesion::instantanea, [&](quint64 t, quint32) { t_inst = t; });
        QObject::connect(&s, &Sesion::muestra, [&](quint16 id, float v) { m[t_inst][id] = v; });
        QProcess p;
        p.setWorkingDirectory(src);
        p.start(sim, {"placas/nucleo_y_shield.xml", "--ms=700",
                      "--gui", QStringLiteral("127.0.0.1:%1").arg(s.puerto())});
        comprueba(espera([&] { return listos == 1; }, 20000) && s.version() == 2 &&
                      s.placa().es_sistema() && s.placa().placas.size() == 2 &&
                      s.placa().placas[0].id == "N" && s.placa().placas[1].id == "S" &&
                      s.placa().acoples.size() == 4,
                  "saluda en la version 2 con un <sistema>: placas N y S, y cuatro acoples");
        // Desde las ilustraciones: la Nucleo trae su dibujo, el shield no
        comprueba(s.ilustraciones().size() == 1 &&
                      s.ilustraciones()[0].placas == QStringList({"N"}) &&
                      s.ilustraciones()[0].fichero == "nucleo_f446re.svg" &&
                      s.ilustraciones()[0].svg.contains("id=\"LD2\""),
                  "y un T_ILUSTRACION: el dibujo de la Nucleo, para la placa N, con su LD2; "
                  "el shield no tiene");
        int ld2 = -1, d13 = -1;
        bool casan = !s.placa().piezas.isEmpty();
        for (const PiezaGui& pz : s.placa().piezas) {
            casan = casan && pz.en_placa && !pz.placa.isEmpty();
            if (pz.id == "N/LD2") ld2 = pz.observables.value(0).id_obs;
            if (pz.id == "S/LD_D13") d13 = pz.observables.value(0).id_obs;
        }
        comprueba(casan && ld2 >= 0 && d13 >= 0 && s.placa().avisos.isEmpty(),
                  "todas las piezas casan con el catalogo y saben de que placa son");
        s.suscribe(1000000, {quint16(ld2), quint16(d13)});
        comprueba(s.arranca(RIT_LIBRE) && espera([&] { return fines == 1; }, 60000),
                  "700 ms simulados");
        QVector<quint64> ts = m.keys().toVector();
        std::sort(ts.begin(), ts.end());
        int flancos = 0, distintos = 0;
        for (int k = 0; k < ts.size(); ++k) {
            const float a = m[ts[k]].value(quint16(ld2), -1), b = m[ts[k]].value(quint16(d13), -1);
            if (a != b) ++distintos;
            if (k && a != m[ts[k - 1]].value(quint16(ld2), -1)) ++flancos;
        }
        comprueba(ts.size() == 700 && flancos >= 6 && distintos == 0,
                  "y el blinky enciende y apaga los dos LEDs A LA VEZ, en dos placas: " +
                      std::to_string(flancos) + " flancos, ni una muestra distinta");
        const bool acabo = p.state() == QProcess::NotRunning || p.waitForFinished(30000);
        comprueba(acabo && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0,
                  "y mcu-sim termina con codigo 0");
    }
    // -------------------------------------------------------------------------
    std::printf("G6 Una pila PC/104: tres placas en un acople\n");
    {
        Sesion s;
        s.escucha(QHostAddress::LocalHost, 0);
        int listos = 0, fines = 0;
        quint64 t_inst = 0;
        QHash<quint64, QHash<quint16, float>> m;
        QObject::connect(&s, &Sesion::listo, [&] { ++listos; });
        QObject::connect(&s, &Sesion::fin, [&](quint32, qint32, quint64) { ++fines; });
        QObject::connect(&s, &Sesion::instantanea, [&](quint64 t, quint32) { t_inst = t; });
        QObject::connect(&s, &Sesion::muestra, [&](quint16 id, float v) { m[t_inst][id] = v; });
        QProcess p;
        p.setWorkingDirectory(src);
        p.start(sim, {"placas/pila_pc104.xml", "--ms=500",
                      "--gui", QStringLiteral("127.0.0.1:%1").arg(s.puerto())});
        comprueba(espera([&] { return listos == 1; }, 20000) && s.placa().es_sistema(),
                  "saluda con el sistema de la pila");
        const PlacaGui& pl = s.placa();
        const SubPlacaGui* cpu = pl.subplaca("CPU");
        const SubPlacaGui* l2 = pl.subplaca("L2");
        comprueba(pl.placas.size() == 3 && cpu && l2 && cpu->n_piezas == 2 &&
                      l2->n_piezas == 3 &&
                      cpu->mcus == QStringList({"CPU/u0 (STM32F407VG)"}) &&
                      l2->mcus.isEmpty() && l2->fichero == "pc104_leds.xml",
                  "cada placa, descrita: cuantas piezas, sus chips y su fichero");
        comprueba(cpu && cpu->conectores.size() == 1 && cpu->conectores[0].ref == "CPU/J1" &&
                      cpu->conectores[0].filas == 2 && cpu->conectores[0].columnas == 32 &&
                      cpu->conectores[0].zigzag && cpu->conectores[0].acople == 0,
                  "y sus conectores, con su forma -2x32 en zigzag- y su acople");
        const QVector<EnlaceGui> en = pl.enlaces();
        comprueba(pl.acoples.size() == 1 &&
                      pl.acoples[0].conectores == QStringList({"CPU/J1", "L1/J1", "L2/J1"}) &&
                      pl.acoples[0].placas == QStringList({"CPU", "L1", "L2"}) &&
                      en.size() == 2 && en[0].placa_a == "CPU" && en[1].placa_b == "L2",
                  "un acople de tres, y el grafo de placas: CPU - L1 - L2");
        int a = -1, b = -1;
        for (const PiezaGui& pz : pl.piezas) {
            if (pz.id == "L1/LD1") a = pz.observables.value(0).id_obs;
            if (pz.id == "L2/LD1") b = pz.observables.value(0).id_obs;
        }
        s.suscribe(1000000, {quint16(a), quint16(b)});
        comprueba(a >= 0 && b >= 0 && s.arranca(RIT_LIBRE) &&
                      espera([&] { return fines == 1; }, 60000),
                  "500 ms simulados, mirando el LD1 de los dos modulos");
        QVector<quint64> ts = m.keys().toVector();
        std::sort(ts.begin(), ts.end());
        int flancos = 0, distintos = 0;
        for (int k = 0; k < ts.size(); ++k) {
            const float x = m[ts[k]].value(quint16(a), -1), y = m[ts[k]].value(quint16(b), -1);
            if (x != y) ++distintos;
            if (k && x != m[ts[k - 1]].value(quint16(a), -1)) ++flancos;
        }
        comprueba(ts.size() == 500 && flancos >= 4 && distintos == 0,
                  "el blinky de la CPU los enciende y apaga a la vez, en dos placas de la "
                  "pila: " + std::to_string(flancos) + " flancos, ni una muestra distinta");
        const bool acabo = p.state() == QProcess::NotRunning || p.waitForFinished(30000);
        comprueba(acabo && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0,
                  "y mcu-sim termina con codigo 0");
    }
    // -------------------------------------------------------------------------
    // G7, desde la rama msp432 de los dos repositorios: un chip que NO es de ST.
    // La LaunchPad MSP-EXP432P401R con su firmware de verificacion, cargado
    // como ELF (el `.out` de CCS). Lo que se comprueba es lo que esta ventana
    // podria dar por hecho y no debe: que los pines se llaman `PA5`. Aqui se
    // llaman `P1.0`, `P2.1` y `PJ.3`, con un punto que NO es el del prefijo
    // del MCU, y la ventana los tiene que tratar como una cadena mas.
    // -------------------------------------------------------------------------
    std::printf("G7 Un MSP432P401R: la LaunchPad roja, con los nombres de TI\n");
    {
        Sesion s;
        s.escucha(QHostAddress::LocalHost, 0);
        int listos = 0, fines = 0;
        quint64 t_inst = 0;
        QVector<quint64> ts;
        QVector<float>   led;
        QObject::connect(&s, &Sesion::listo, [&] { ++listos; });
        QObject::connect(&s, &Sesion::fin, [&](quint32, qint32, quint64) { ++fines; });
        QObject::connect(&s, &Sesion::instantanea, [&](quint64 t, quint32) { t_inst = t; });
        QObject::connect(&s, &Sesion::muestra, [&](quint16, float v) {
            ts.push_back(t_inst);
            led.push_back(v);
        });
        QProcess p;
        p.setWorkingDirectory(src);
        p.start(sim, {"placas/msp_exp432p401r.xml",
                      "verif/fw/msp432p4/p4_demo/p4_demo.out", "--ms=60",
                      "--gui", QStringLiteral("127.0.0.1:%1").arg(s.puerto())});
        comprueba(espera([&] { return listos == 1; }, 20000),
                  "mcu-sim monta la LaunchPad, saluda y dice T_LISTO");
        const PlacaGui& pl = s.placa();
        bool todas = !pl.piezas.isEmpty();
        QHash<QString, QString> nodo;            // "LED1" -> "P1.0"
        for (const PiezaGui& pz : pl.piezas) {
            todas = todas && pz.en_placa;
            if (!pz.patillas.isEmpty()) nodo[pz.id] = pz.patillas[0].nodo;
        }
        auto tipo_de = [](const PlacaGui& pl, const QString& id) {
            for (const PiezaGui& pz : pl.piezas)
                if (pz.id == id) return pz.tipo;
            return QString();
        };
        // El LED RGB, una pieza: sus tres patillas y sus canales de color
        QHash<QString, QString> rgb;             // "r" -> "P2.0"
        QStringList colores;
        for (const PiezaGui& pz : pl.piezas)
            if (pz.id == "LED2") {
                for (const PatillaGui& pt : pz.patillas) rgb[pt.nombre] = pt.nodo;
                for (const ObservableGui& o : pz.observables)
                    if (!o.color.isEmpty()) colores << o.nombre + "=" + o.color;
            }
        comprueba(pl.piezas.size() == 23 && todas && pl.avisos.isEmpty() &&
                      tipo_de(pl, "LED1") == "Led" && tipo_de(pl, "LED2") == "RGBLed" &&
                      tipo_de(pl, "S3") == "Button" && tipo_de(pl, "Y2") == "Crystal" &&
                      tipo_de(pl, "XDS") == "Xds110",
                  "veintitres piezas -los LED, el LED RGB, los pulsadores, los cristales, la "
                  "sonda XDS110, los puentes y los conectores-, todas casadas con su "
                  "componente y sin avisos");
        // La sonda XDS110: la corriente de su LDO, la alarma de sobrecorriente
        // y si hay un OpenOCD conectado
        QStringList xds;
        for (const PiezaGui& pz : pl.piezas)
            if (pz.id == "XDS")
                for (const ObservableGui& o : pz.observables)
                    xds << o.nombre + (o.alarma ? "!" : "") + (o.unidad.isEmpty() ? "" : "/" + o.unidad);
        comprueba(xds == QStringList({"corriente/mA", "sobrecorriente!", "depurando"}),
                  "y la sonda XDS110 ensena la corriente de su LDO, la sobrecorriente como "
                  "alarma y si se esta depurando: " + xds.join(' ').toStdString());
        comprueba(nodo.value("LED1") == "P1.0" && rgb.value("r") == "P2.0" &&
                      rgb.value("g") == "P2.1" && rgb.value("b") == "P2.2" &&
                      nodo.value("Y2") == "PJ.3" && nodo.value("S3") == "RSTN",
                  "y sus patillas llegan con los nombres del datasheet de TI: P1.0, "
                  "P2.0/P2.1/P2.2, PJ.3 y RSTN, no PB0 ni NRST");
        comprueba(colores.size() == 6 && colores.contains("rojo=#ff0000") &&
                      colores.contains("verde=#00ff00") && colores.contains("azul=#0000ff") &&
                      colores.contains("corriente_b=#0000ff"),
                  "y el LED RGB declara sus canales de color, que la ventana mezcla: " +
                      colores.join(' ').toStdString());
        quint16 id = 0;
        for (const PiezaGui& pz : pl.piezas)
            if (pz.id == "LED1" && !pz.observables.isEmpty())
                id = quint16(pz.observables[0].id_obs);
        comprueba(s.suscribe(1000000, {id}), "suscrito al LED1, cada 1 ms");
        comprueba(s.arranca(RIT_LIBRE) && espera([&] { return fines == 1; }, 60000),
                  "60 ms simulados");
        int flancos = 0;
        for (int k = 1; k < led.size(); ++k)
            if (led[k] != led[k - 1]) ++flancos;
        comprueba(ts.size() == 60 && flancos >= 20,
                  "y el LED1 parpadea en las muestras: el firmware para el perro y "
                  "conmuta P1.0 cada 1,1 ms a 3 MHz (" + std::to_string(flancos) +
                      " flancos en 60 muestras)");
        const bool acabo = p.state() == QProcess::NotRunning || p.waitForFinished(30000);
        comprueba(acabo && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0,
                  "y mcu-sim termina con codigo 0");
    }
    // -------------------------------------------------------------------------
    // G8, desde la rama stm32f303 de los dos repositorios: la STM32F3DISCOVERY
    // (S1 de doc/analisis_stm32f303.md), con GPIO_IOToggle de STM32CubeF3 sin
    // modificar. El reloj, 72 MHz por el HSE desde el MCO del ST-LINK; la
    // rosa se enciende de PE15 (LD6) a PE8 (LD4), un LED cada 101 ms
    // (HAL_Delay(100)). Sin los sensores, que llegan en S4.
    // -------------------------------------------------------------------------
    std::printf("G8 Un STM32F303VC: la STM32F3DISCOVERY y su rosa\n");
    {
        Sesion s;
        s.escucha(QHostAddress::LocalHost, 0);
        int listos = 0, fines = 0;
        QHash<quint16, QVector<float>> m;
        QObject::connect(&s, &Sesion::listo, [&] { ++listos; });
        QObject::connect(&s, &Sesion::fin, [&](quint32, qint32, quint64) { ++fines; });
        QObject::connect(&s, &Sesion::muestra, [&](quint16 id, float v) {
            m[id].push_back(v);
        });
        QProcess p;
        p.setWorkingDirectory(src);
        p.start(sim, {"placas/stm32f3discovery.xml",
                      "verif/fw/stm32f3/gpio_iotoggle/GPIO_IOToggle.bin", "--ms=900",
                      "--gui", QStringLiteral("127.0.0.1:%1").arg(s.puerto())});
        comprueba(espera([&] { return listos == 1; }, 20000),
                  "mcu-sim monta la Discovery, saluda y dice T_LISTO");
        const PlacaGui& pl = s.placa();
        bool todas = !pl.piezas.isEmpty();
        QHash<QString, QString> nodo, tipo;
        QHash<QString, quint16> obs;
        for (const PiezaGui& pz : pl.piezas) {
            todas = todas && pz.en_placa;
            tipo[pz.id] = pz.tipo;
            if (!pz.patillas.isEmpty()) nodo[pz.id] = pz.patillas[0].nodo;
            if (!pz.observables.isEmpty()) obs[pz.id] = quint16(pz.observables[0].id_obs);
        }
        // S6 del F303: y R16, la de D+ del USB de usuario (sin dibujo)
        comprueba(pl.piezas.size() == 26 && todas && pl.avisos.isEmpty() &&
                      tipo["LD3"] == "Led" && tipo["B1"] == "Button" && tipo["X3"] == "Crystal" &&
                      tipo["MCO"] == "ExtClock" && tipo["JP3"] == "Jumper" &&
                      tipo["EXP1"] == "Conector" && tipo["VCP"] == "PuenteSerie" &&
                      tipo["R16"] == "Resistencia",
                  "veintiseis piezas -la rosa, LD1, B1 y B2, el MCO y el VCP del ST-LINK, X3, JP3, "
                  "las tiras P1 y P2, los sensores con su postura y R16, la de D+ del USB-, todas "
                  "casadas y sin avisos");
        // S4 del F303: los sensores de la revision E -la del fichero- y la
        // Postura que los mueve, con sus tres mandos
        {
            const PiezaGui* mov = nullptr;
            for (const PiezaGui& pz : pl.piezas) if (pz.id == "MOV") mov = &pz;
            bool mandos = mov && mov->mandos.size() == 3;
            if (mandos) {
                const char* n[3] = {"alabeo", "cabeceo", "rumbo"};
                for (int k = 0; k < 3; ++k) mandos = mandos && mov->mandos[k].nombre == n[k];
            }
            comprueba(tipo["U5"] == "I3g4250d" && tipo["U3"] == "Lsm303agr" &&
                          tipo["MOV"] == "Postura" && tipo["R27"] == "Resistencia" && mandos,
                      "los sensores de la revision E (I3G4250D y LSM303AGR) y la Postura, con "
                      "alabeo, cabeceo y rumbo");
        }
        comprueba(nodo["LD3"] == "PE9" && nodo["LD6"] == "PE15" && nodo["LD4"] == "PE8" &&
                      nodo["B1"] == "PA0" && nodo["MCO"] == "PF0",
                  "y sus patillas: LD3 en PE9, LD6 en PE15, LD4 en PE8, B1 en PA0 y el MCO en PF0");
        comprueba(pl.ilustracion.contains("stm32f3discovery") ||
                      (s.ilustraciones().size() == 1 &&
                       s.ilustraciones()[0].fichero == "stm32f3discovery.svg" &&
                       s.ilustraciones()[0].svg.contains("id=\"EXP2.50\"")),
                  "con su dibujo, stm32f3discovery.svg, que lleva cada pin de P1 y P2");
        const QString orden[8] = {"LD6", "LD8", "LD10", "LD9", "LD7", "LD5", "LD3", "LD4"};
        QVector<quint16> ids;
        for (const QString& l : orden) ids << obs.value(l);
        comprueba(!ids.contains(0) && s.suscribe(1000000, ids), "suscrito a los ocho LED, cada 1 ms");
        comprueba(s.arranca(RIT_LIBRE) && espera([&] { return fines == 1; }, 120000),
                  "900 ms simulados");
        // Cuando se enciende cada uno: la primera muestra distinta de la
        // primera. Una muestra por milisegundo, asi que el indice es el tiempo
        // en ms (las instantaneas marcan lotes, no muestras)
        int t_on[8] = {};
        bool todos = true;
        for (int k = 0; k < 8; ++k) {
            const auto& v = m.value(ids[k]);
            for (int j = 1; j < v.size() && !t_on[k]; ++j)
                if (v[j] != v[0]) t_on[k] = j;
            todos = todos && t_on[k];
        }
        bool en_orden = todos;
        std::string dts;
        for (int k = 1; k < 8 && todos; ++k) {
            const int dt = t_on[k] - t_on[k - 1];
            dts += (k > 1 ? " " : "") + std::to_string(dt);
            en_orden = en_orden && std::abs(dt - 101) <= 1;
        }
        comprueba(en_orden,
                  "y la rosa se enciende en el orden del ejemplo, de LD6 (PE15) a LD4 (PE8), "
                  "uno cada 101 ms (" + dts + " ms)");
        const bool acabo = p.state() == QProcess::NotRunning || p.waitForFinished(30000);
        comprueba(acabo && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0,
                  "y mcu-sim termina con codigo 0");
    }

    // -------------------------------------------------------------------------
    // G9, del pulido de la 0.4.0: el sistema de la Nucleo con el servo, el
    // KY-040 de 24 PPR, la pantalla y el FTDI, mas un L298N, dos motorreductores
    // con su encoder y su bateria. Lo que la ventana tiene que saber de las
    // piezas nuevas -MotorCC, L298 y Regulador- llega en el catalogo como el de
    // cualquier otra; los dos motores comparten dibujo; y diez clics del mando,
    // mandados antes de arrancar, los ponen a toda marcha.
    // -------------------------------------------------------------------------
    std::printf("G9 Dos motores con su encoder tras un L298N, en la Nucleo con ocho modulos\n");
    {
        Sesion s;
        s.escucha(QHostAddress::LocalHost, 0);
        int listos = 0, fines = 0;
        QHash<quint16, QVector<float>> m;
        QObject::connect(&s, &Sesion::listo, [&] { ++listos; });
        QObject::connect(&s, &Sesion::fin, [&](quint32, qint32, quint64) { ++fines; });
        QObject::connect(&s, &Sesion::muestra, [&](quint16 id, float v) {
            m[id].push_back(v);
        });
        QProcess p;
        p.setWorkingDirectory(src);
        p.start(sim, {"placas/nucleo_f446re_encoder_lcd_servo_2xmcc.xml", "--ms=1300",
                      "--serie", "F/U1=memoria", "--serie", "N/VCP=memoria",
                      "--gui", QStringLiteral("127.0.0.1:%1").arg(s.puerto())});
        comprueba(espera([&] { return listos == 1; }, 20000),
                  "mcu-sim monta el sistema de los motores, saluda y dice T_LISTO");
        const PlacaGui& pl = s.placa();
        QHash<QString, const PiezaGui*> pz;
        bool todas = !pl.piezas.isEmpty();
        for (const PiezaGui& x : pl.piezas) {
            pz[x.id] = &x;
            todas = todas && x.en_placa;
        }
        auto tipo = [&](const char* id) { return pz.contains(id) ? pz[id]->tipo : QString(); };
        comprueba(pl.placas.size() == 9 && pl.piezas.size() == 44 && todas &&
                      pl.avisos.isEmpty() && tipo("MA/MOTOR") == "MotorCC" &&
                      tipo("MB/MOTOR") == "MotorCC" && tipo("L/U1") == "L298" &&
                      tipo("L/U2") == "Regulador" && tipo("K/ENC") == "Encoder" &&
                      tipo("F/U1") == "PuenteSerie",
                  "nueve placas y cuarenta y cuatro piezas, todas casadas y sin avisos: los dos "
                  "MotorCC, el L298 y su Regulador, el encoder de mando y el FTDI");
        const PiezaGui* ma = pz.value("MA/MOTOR");
        bool obs = ma && ma->observables.size() == 7 && ma->mandos.size() == 2;
        if (obs) {
            const ObservableGui& a = ma->observables[0];
            obs = a.nombre == "angulo" && a.unidad == QString::fromUtf8("\xc2\xb0") &&
                  a.interesante && a.min == 0 && a.max == 360 &&
                  ma->observables[1].nombre == "rpm" && ma->mandos[0].nombre == "bloquear" &&
                  ma->mandos[1].nombre == "carga" && ma->mandos[1].tipo == "continuo";
        }
        comprueba(obs, "el motor sugiere su angulo, de 0 a 360 grados -el disco gira con el-, "
                       "y se deja bloquear y cargar");
        bool motor = false, l298 = false;
        for (const Sesion::Ilustracion& il : s.ilustraciones()) {
            if (il.fichero == "motor_tt_encoder.svg")
                motor = il.placas.contains("MA") && il.placas.contains("MB") &&
                        il.svg.contains("id=\"MOTOR\"");
            if (il.fichero == "l298n.svg")
                l298 = il.placas == QStringList{"L"} && !il.svg.contains("id=\"JA@5V-ENA\"") &&
                       il.svg.contains("id=\"JP5@REG-5V\"");
        }
        comprueba(motor && l298,
                  "los dos motores, con UN dibujo, el de su disco; el L298N, sin los puentes "
                  "de ENA y ENB, que llevan el PWM");
        quint16 rpm_a = 0, rpm_b = 0, enc = 0, girar = 0;
        if (ma) rpm_a = quint16(ma->observables[1].id_obs);
        if (pz.contains("MB/MOTOR") && pz["MB/MOTOR"]->observables.size() > 1)
            rpm_b = quint16(pz["MB/MOTOR"]->observables[1].id_obs);
        if (pz.contains("K/ENC") && !pz["K/ENC"]->mandos.isEmpty()) {
            enc = quint16(pz["K/ENC"]->idx);
            girar = quint16(pz["K/ENC"]->mandos[0].idx);
        }
        comprueba(rpm_a && rpm_b && s.suscribe(1000000, {rpm_a, rpm_b}) &&
                      s.ordena(QVector<Orden>{{400000000ull, enc, girar, 10.f}}),
                  "suscrito a las rpm de los dos, y diez clics del mando a los 400 ms");
        comprueba(s.arranca(RIT_LIBRE) && espera([&] { return fines == 1; }, 120000),
                  "1300 ms simulados");
        const auto& va = m.value(rpm_a);
        const auto& vb = m.value(rpm_b);
        const float fa = va.isEmpty() ? 0.f : va.back(), fb = vb.isEmpty() ? 0.f : vb.back();
        comprueba(va.size() == 1300 && va[390] == 0.f && fa > 237.f && fa < 240.5f && fb == fa,
                  "parados hasta el mando; despues, el PWM al 100 % y los dos a " +
                      std::to_string(fa) + " rpm");
        const bool acabo = p.state() == QProcess::NotRunning || p.waitForFinished(30000);
        comprueba(acabo && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0,
                  "y mcu-sim termina con codigo 0");
    }
    return resultado();
}
