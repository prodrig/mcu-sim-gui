// =============================================================================
// prueba_lanzamiento.cpp — la ventana lanza al mcu-sim DE VERDAD
//
// Fase 7 del plan. El plan dice que aquí hace falta una persona: abrir la
// ventana, elegir la placa y el blinky, arrancar y VER el LED parpadear a su
// velocidad. Eso sigue siendo a mano —esto no tiene ojos—. Lo que sí se hace
// aquí, con la `VentanaPrincipal` entera en la plataforma `offscreen`, es
// todo lo que una persona miraría para decidirlo, y la lista de fallos que el
// plan manda probar a mano:
//
//   L1  `mcu-sim --argumentos` se lee, y el diálogo sale de él (con CAPTURAS,
//       también una captura del diálogo);
//   L2  lanzar con la configuración: el hijo se conecta solo, su salida llega a
//       la consola, y a tiempo real el LED de las instantáneas cambia cada
//       100 ms simulados Y cada ~100 ms de pared. Con CAPTURAS=directorio,
//       deja dos capturas de la ventana: el LED encendido y apagado;
//   L3  cerrar la ventana con el modelo corriendo: T_PARA, y el hijo termina
//       con 0 dando su resumen;
//   L4  matar mcu-sim desde fuera: la ventana lo dice y se puede volver a
//       lanzar;
//   L5  un puerto ocupado a propósito: escucha en otro, y el hijo se conecta
//       a ese;
//   L6  una placa que no existe: el hijo termina sin conectarse, y la ventana
//       lo dice y enseña su salida de error. Una ruta de ejecutable mala está en
//       `prueba_argumentos` G5.
//
// Como `cruzada`, necesita MCU_SIM (el ejecutable) y MCU_SIM_SRC (su src/).
// Sin ellas, código 77: saltada.
// =============================================================================
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTcpServer>

#include "argumentos.h"
#include "comun.h"
#include "configuracion.h"
#include "dialogo_lanzamiento.h"
#include "lanzador.h"
#include "ventana_principal.h"

#ifdef Q_OS_UNIX
#include <csignal>
#endif

using namespace mcusim;
using namespace mcusim::proto;
using namespace prueba;

namespace {

QString g_sim, g_src;

Configuracion configuracion(quint16 puerto = 0)
{
    Configuracion c;
    c.ejecutable = g_sim;
    c.directorio = g_src;
    c.puerto = puerto;
    c.argumentos = {{"placa", "placas/discovery_min.xml"},
                    {"firmware", "verif/fw/blinky/blinky.bin"}};
    return c;
}

QString consola(VentanaPrincipal& v)
{
    return v.findChild<QPlainTextEdit*>("consola")->toPlainText();
}

// Matar desde fuera, como lo haría alguien con `kill -9` o el administrador de
// tareas: sin que el proceso pueda decir nada.
void mata_desde_fuera(qint64 pid)
{
#ifdef Q_OS_UNIX
    ::kill(pid_t(pid), SIGKILL);
#else
    QProcess::execute(QStringLiteral("taskkill"),
                      {QStringLiteral("/F"), QStringLiteral("/PID"), QString::number(pid)});
#endif
}

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    g_sim = qEnvironmentVariable("MCU_SIM");
    g_src = qEnvironmentVariable("MCU_SIM_SRC");
    if (g_sim.isEmpty() || g_src.isEmpty()) {
        std::printf("  [SALTA] sin MCU_SIM y MCU_SIM_SRC no hay un mcu-sim que lanzar\n");
        return 77;
    }
    const QString capturas = qEnvironmentVariable("CAPTURAS");

    // -------------------------------------------------------------------------
    std::printf("L1 La lista de opciones, del mcu-sim de verdad\n");
    {
        QString e;
        ArgumentosCli a;
        const QByteArray s = Lanzador::argumentos_de(g_sim, g_src, e);
        comprueba(!s.isEmpty() && lee_argumentos(s, a, e) && a.programa == "mcu-sim",
                  "mcu-sim --argumentos se lee: " + e.toStdString());
        bool ms = false, mcu = false;
        for (const OpcionCli& o : a.opciones) {
            ms  = ms  || (o.nombre == "--ms" && o.se_ofrece());
            mcu = mcu || (o.nombre == "--mcu" && o.valores.contains("STM32F407VG"));
        }
        comprueba(ms && mcu, "con --ms, y --mcu con los MCUs del catalogo");
        DialogoLanzamiento d(configuracion(), "127.0.0.1:1");
        comprueba(d.findChild<QComboBox*>("arg:--mcu") && d.findChild<QWidget*>("arg:--ondas") &&
                      d.aviso().isEmpty(),
                  "y el dialogo, abierto sin lista, se la pide a mcu-sim y sale de ella");
        if (!capturas.isEmpty()) {
            d.resize(640, 640);
            d.show();
            QCoreApplication::processEvents();
            d.grab().save(QDir(capturas).filePath("dialogo.png"));
        }
    }

    // -------------------------------------------------------------------------
    std::printf("L2 Lanzar, y ver parpadear el LED\n");
    {
        auto* v = new VentanaPrincipal(configuracion());
        v->resize(1100, 720);
        v->show();
        auto* arrancar = v->findChild<QPushButton*>("arrancar");
        auto* ritmo    = v->findChild<QComboBox*>("ritmo");
        comprueba(v->lanza() && espera([&] { return arrancar->isEnabled(); }, 20000),
                  "lanza() con la configuracion: el hijo se conecta solo, saluda y espera");
        comprueba(consola(*v).contains("$ ") && consola(*v).contains("--gui 127.0.0.1:") &&
                      consola(*v).contains("gui: conectado"),
                  "la consola ensena la orden, con su --gui, y lo que mcu-sim dice");
        // El LED: encendido de LD4
        quint16 id = 0xFFFF;
        for (const PiezaGui& pz : v->sesion().placa().piezas)
            if (pz.id == "LD4") id = quint16(pz.observables[0].id_obs);
        QVector<QPair<quint64, qint64>> flancos;   // instante simulado, ms de pared
        float antes = -1;
        quint64 t_inst = 0;
        QElapsedTimer pared;
        QObject::connect(&v->sesion(), &Sesion::instantanea,
                         [&](quint64 t, quint32) { t_inst = t; });
        QObject::connect(&v->sesion(), &Sesion::muestra, [&](quint16 i, float x) {
            if (i != id) return;
            if (antes >= 0 && x != antes) flancos.push_back({t_inst, pared.elapsed()});
            antes = x;
        });
        ritmo->setCurrentIndex(0);              // tiempo real
        pared.start();
        arrancar->click();
        auto* led = v->findChild<QLabel*>(QStringLiteral("obs:%1").arg(id));
        bool capturado_on = false, capturado_off = false;
        espera([&] {
            if (!capturas.isEmpty() && led && flancos.size() >= 1) {
                if (!capturado_on && led->text() == QString::fromUtf8("●")) {
                    v->grab().save(QDir(capturas).filePath("led_encendido.png"));
                    capturado_on = true;
                } else if (!capturado_off && capturado_on && led->text() == QString::fromUtf8("○")) {
                    v->grab().save(QDir(capturas).filePath("led_apagado.png"));
                    capturado_off = true;
                }
            }
            return flancos.size() >= 5;
        }, 5000);
        // El blinky enciende en 3 ms y conmuta cada 100: el primer flanco cae
        // antes de la primera muestra (16,7 ms), así que se ven cinco. Y se ven
        // con la resolución del muestreo, 60 por segundo simulado.
        const qint64 periodo_ms = 17;
        bool sim_100 = flancos.size() >= 5, pared_100 = sim_100;
        QString dsim, dpared;
        for (int k = 1; k < flancos.size() && k < 5; ++k) {
            const qint64 ds = qint64(flancos[k].first - flancos[k - 1].first) / 1000000;
            const qint64 dp = flancos[k].second - flancos[k - 1].second;
            sim_100 = sim_100 && qAbs(ds - 100) <= periodo_ms;
            pared_100 = pared_100 && qAbs(dp - 100) <= periodo_ms + 40;
            dsim += QStringLiteral("%1 ").arg(ds);
            dpared += QStringLiteral("%1 ").arg(dp);
        }
        comprueba(sim_100, "a tiempo real el LED cambia cada 100 ms simulados, como programa "
                           "el blinky, con la resolucion del muestreo: " +
                               dsim.trimmed().toStdString() + " ms");
        comprueba(pared_100, "y cada ~100 ms de pared: " + dpared.trimmed().toStdString() + " ms");
        comprueba(led && v->findChild<QLabel*>("relojes")->text().contains("pared"),
                  "con el reloj simulado y el de pared en pantalla: \"" +
                      v->findChild<QLabel*>("relojes")->text().toStdString() + "\"");
        if (!capturas.isEmpty())
            comprueba(capturado_on && capturado_off,
                      "y dos capturas en " + capturas.toStdString() + ": encendido y apagado");

        // ---------------------------------------------------------------------
        std::printf("L3 Cerrar la ventana con el modelo corriendo\n");
        const qint64 pid = v->lanzador().pid();
        int codigo = -1;
        bool estrellado = true;
        QObject::connect(&v->lanzador(), &Lanzador::termino, [&](int c, bool e) {
            codigo = c;
            estrellado = e;
        });
        QElapsedTimer t;
        t.start();
        v->close();
        comprueba(pid > 0 && !v->lanzador().corriendo() && codigo == 0 && !estrellado &&
                      t.elapsed() < 3000,
                  "cerrar le pide parar (T_PARA) y mcu-sim termina con 0, en " +
                      std::to_string(t.elapsed()) + " ms");
        comprueba(consola(*v).contains("la ventana pidio parar") && consola(*v).contains("LED LD4"),
                  "dando su resumen de siempre, que llega a la consola");
        delete v;
    }

    // -------------------------------------------------------------------------
    std::printf("L4 Matar mcu-sim desde fuera\n");
    {
        VentanaPrincipal v(configuracion());
        v.show();
        auto* arrancar = v.findChild<QPushButton*>("arrancar");
        v.lanza();
        espera([&] { return arrancar->isEnabled(); }, 20000);
        v.findChild<QComboBox*>("ritmo")->setCurrentIndex(2);   // libre
        arrancar->click();
        espera([&] { return v.findChild<QPushButton*>("parar")->isEnabled(); });
        mata_desde_fuera(v.lanzador().pid());
        comprueba(espera([&] { return !v.lanzador().corriendo(); }, 10000) &&
                      espera([&] { return v.sesion().estado() == Sesion::Estado::Terminada; }),
                  "la ventana ve que se ha ido: el proceso y la conexion");
        comprueba(v.statusBar()->currentMessage().contains("estrellado") &&
                      consola(v).contains("estrellado") &&
                      !v.findChild<QPushButton*>("parar")->isEnabled(),
                  "lo dice -\"" + v.statusBar()->currentMessage().toStdString() +
                      "\"- y apaga el control");
        comprueba(v.lanza() && espera([&] { return arrancar->isEnabled(); }, 20000),
                  "y se puede volver a lanzar, en la misma ventana");
        v.close();
    }

    // -------------------------------------------------------------------------
    std::printf("L5 Un puerto ocupado a proposito\n");
    {
        QTcpServer okupa;
        okupa.listen(QHostAddress::LocalHost, 0);
        VentanaPrincipal v(configuracion(okupa.serverPort()));
        v.show();
        auto* arrancar = v.findChild<QPushButton*>("arrancar");
        comprueba(v.sesion().puerto() != okupa.serverPort() && v.lanza() &&
                      espera([&] { return arrancar->isEnabled(); }, 20000) &&
                      consola(v).contains(QStringLiteral("--gui 127.0.0.1:%1").arg(v.sesion().puerto())),
                  "con el de la configuracion cogido, escucha en otro, y el hijo se conecta "
                  "a ese: se le paso en --gui");
        comprueba(!okupa.hasPendingConnections(), "y al cogido no ha llamado nadie");
        v.close();
    }

    // -------------------------------------------------------------------------
    std::printf("L6 Una placa que no existe\n");
    {
        Configuracion c = configuracion();
        c.argumentos["placa"] = "placas/no-existe.xml";
        VentanaPrincipal v(c);
        v.show();
        int codigo = -1;
        QObject::connect(&v.lanzador(), &Lanzador::termino, [&](int k, bool) { codigo = k; });
        const bool termino = v.lanza() && espera([&] { return codigo >= 0; }, 20000);
        comprueba(termino && codigo != 0, "mcu-sim termina con codigo " + std::to_string(codigo));
        comprueba(v.findChild<QLabel*>("resumen")->text().contains("sin llegar a conectarse") &&
                      consola(v).contains("no-existe.xml"),
                  "y la ventana dice que no llego a conectarse, con su salida de error en la "
                  "consola");
    }

    return resultado();
}
