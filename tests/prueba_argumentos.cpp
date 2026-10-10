// =============================================================================
// prueba_argumentos.cpp — la lista de opciones, la configuración y el diálogo
//
// Fase 7 del plan, todo lo que no necesita un mcu-sim de verdad (eso es
// `prueba_lanzamiento`):
//
//   G1  leer lo que vuelca `mcu-sim --argumentos`, con la cabecera de SystemC
//       delante, y lo que no se puede leer;
//   G2  la línea de órdenes a partir de los valores: posicionales, banderas,
//       valores, repetibles, lo escrito a mano; y una opción vacía, que no es
//       lo mismo que su valor por omisión;
//   G3  la configuración en JSON: la plantilla versionada se lee, se guarda y
//       se vuelve a leer igual, los comentarios sobreviven, las rutas son
//       relativas al fichero;
//   G4  el diálogo construido con una lista: un campo por argumento según su
//       forma, ninguno para lo que no se ofrece, las del mismo grupo se
//       excluyen, y la línea que va a salir; y arriba, de dónde salen los
//       valores —o, si no había configuración, dónde se buscó y dónde se
//       guardará—;
//   G5  la ventana con el puerto cogido: escucha en otro sin decir nada, y una
//       ruta de ejecutable mala se dice;
//   G6  el firmware de cada MCU (plan §42): leer `mcu-sim --mcus`, la línea con
//       uno y con dos chips, y el diálogo: la fila de uno con su casilla «sin
//       firmware», una fila por chip con varios, y lo que se recuerda.
// =============================================================================
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTcpServer>
#include <QTemporaryDir>

#include "argumentos.h"
#include "comun.h"
#include "configuracion.h"
#include "dialogo_lanzamiento.h"
#include "ventana_principal.h"

using namespace mcusim;
using namespace prueba;

namespace {

// Lo que vuelca un mcu-sim de verdad, recortado, con la cabecera de SystemC
const char* const VOLCADO =
    "\n        SystemC 2.3.4-Accellera --- Apr 22 2024 14:54:19\n"
    "        Copyright (c) 1996-2022 by all Contributors,\n"
    "        ALL RIGHTS RESERVED\n"
    "<argumentos programa=\"mcu-sim\" version=\"desarrollo\">\n"
    "  <posicional nombre=\"placa\" tipo=\"fichero\" filtro=\"*.xml\" obligatorio=\"si\" ayuda=\"la placa\"/>\n"
    "  <posicional nombre=\"firmware\" tipo=\"fichero\" filtro=\"*.bin\" obligatorio=\"no\" ayuda=\"el firmware\"/>\n"
    "  <opcion nombre=\"--ms\" forma=\"valor\" tipo=\"numero\" omision=\"100\" unidad=\"ms\" ejemplo=\"2000\" ayuda=\"tiempo simulado\"/>\n"
    "  <opcion nombre=\"--mcu\" forma=\"valor\" tipo=\"eleccion\" ejemplo=\"STM32F407VG\" ayuda=\"el MCU\">\n"
    "    <valor>STM32F405RG</valor>\n"
    "    <valor>STM32F407VG</valor>\n"
    "    <valor>STM32F446RE</valor>\n"
    "  </opcion>\n"
    "  <opcion nombre=\"--valida\" forma=\"bandera\" ayuda=\"solo comprueba\"/>\n"
    "  <opcion nombre=\"--ondas\" forma=\"bandera\" ayuda=\"las ondas\"/>\n"
    "  <opcion nombre=\"--gdb\" forma=\"bandera\" grupo=\"gdb\" ayuda=\"GDB por SWD\"/>\n"
    "  <opcion nombre=\"--gdb-dap\" forma=\"bandera\" grupo=\"gdb\" ayuda=\"GDB por el DAP\"/>\n"
    "  <opcion nombre=\"--port\" forma=\"valor\" tipo=\"entero\" omision=\"3333\" ayuda=\"puerto\"/>\n"
    "  <opcion nombre=\"--serie\" forma=\"valor\" tipo=\"texto\" repetible=\"si\" ejemplo=\"VCP=rfc2217:4000\" ayuda=\"puentes &amp; cosas\"/>\n"
    "  <opcion nombre=\"--tiempo-real\" forma=\"valor_opcional\" tipo=\"numero\" omision=\"1\" con_gui=\"no\" ayuda=\"freno\"/>\n"
    "  <opcion nombre=\"--gui\" forma=\"valor_opcional\" tipo=\"texto\" omision=\"localhost:3344\" con_gui=\"no\" ayuda=\"la ventana\"/>\n"
    "  <opcion nombre=\"--help\" forma=\"accion\" ayuda=\"ayuda\"/>\n"
    "</argumentos>\n";

ArgumentosCli lista()
{
    ArgumentosCli a;
    QString e;
    lee_argumentos(VOLCADO, a, e);
    return a;
}

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    // -------------------------------------------------------------------------
    std::printf("G1 Leer la lista de opciones\n");
    {
        ArgumentosCli a;
        QString e;
        comprueba(lee_argumentos(VOLCADO, a, e) && a.programa == "mcu-sim" &&
                      a.version == "desarrollo",
                  "se lee, con la cabecera de SystemC delante");
        comprueba(a.posicionales.size() == 2 && a.posicionales[0].nombre == "placa" &&
                      a.posicionales[0].obligatorio && !a.posicionales[1].obligatorio &&
                      a.posicionales[0].filtro == "*.xml",
                  "dos posicionales: placa, obligatoria y con su filtro, y firmware");
        comprueba(a.opciones.size() == 11, "once opciones");
        const OpcionCli& mcu = a.opciones[1];
        comprueba(mcu.nombre == "--mcu" && mcu.tipo == "eleccion" &&
                      mcu.valores == QStringList{"STM32F405RG", "STM32F407VG", "STM32F446RE"} &&
                      mcu.omision.isEmpty() && mcu.ejemplo == "STM32F407VG",
                  "--mcu: una eleccion con sus tres valores, sin omision -sin <mcu> ni "
                  "--mcu la placa va sin MCU- y con un ejemplo");
        comprueba(a.opciones[7].ayuda == "puentes & cosas" && a.opciones[7].repetible,
                  "las entidades del XML se deshacen, y --serie es repetible");
        int ofrecidas = 0;
        for (const OpcionCli& o : a.opciones) ofrecidas += o.se_ofrece() ? 1 : 0;
        comprueba(ofrecidas == 8,
                  "se ofrecen ocho: ni --tiempo-real ni --gui, que con la ventana no tienen "
                  "sentido, ni --help, que es una accion");
        bool r = lee_argumentos("uso: sim placa.xml\n", a, e);
        comprueba(!r && e.contains("no contesta"),
                  "lo que no es una lista: se dice (\"" + e.toStdString() + "\")");
        r = lee_argumentos("<argumentos><posicional nombre=\"p\"/><opcion nombre=\"ms\"/>"
                           "</argumentos>", a, e);
        comprueba(!r && e.contains("no empieza por --"), "una opcion sin -- delante: se dice");
        r = lee_argumentos("<argumentos><posicional nombre=\"p\"></argumentos>", a, e);
        comprueba(!r && e.contains("XML"), "y XML roto, tambien (\"" + e.toStdString() + "\")");
    }

    // -------------------------------------------------------------------------
    std::printf("G2 La linea de ordenes\n");
    {
        const ArgumentosCli a = lista();
        QString e;
        ValoresCli v{{"placa", "placas/discovery_min.xml"}, {"firmware", "fw/blinky.bin"},
                     {"--ms", "2000"}, {"--ondas", "si"}, {"--valida", ""},
                     {"--mcu", "STM32F446RE"}, {"--serie", "VCP=tcp:4000; DBG=memoria"},
                     {"--tiempo-real", "0.5"}};
        const QStringList l = linea_de_ordenes(a, v, "--traza-gdb 'con espacio' \"y otra\"", &e);
        comprueba(e.isEmpty() &&
                      l == QStringList{"placas/discovery_min.xml", "fw/blinky.bin", "--ms=2000",
                                       "--mcu=STM32F446RE", "--ondas", "--serie=VCP=tcp:4000",
                                       "--serie=DBG=memoria", "--tiempo-real=0.5", "--traza-gdb",
                                       "con espacio", "y otra"},
                  "posicionales en su orden, opciones en el de la lista, una repetible por "
                  "valor, y lo de a mano troceado como en una consola: " +
                      l.join(' ').toStdString());
        comprueba(linea_de_ordenes(a, {{"placa", "p.xml"}}, QString(), &e) ==
                      QStringList{"p.xml"},
                  "una opcion vacia NO se escribe, aunque tenga valor por omision: con --gui, "
                  "un --ms ausente es 'sin fin', no 100 ms");
        comprueba(linea_de_ordenes(a, {{"firmware", "f.bin"}}, QString(), &e).isEmpty() &&
                      e.contains("placa"),
                  "sin la placa, que es obligatoria, no sale nada y se dice");
        comprueba(linea_de_ordenes(a, {{"placa", "p.xml"}, {"--help", "si"}}, "", &e) ==
                      QStringList{"p.xml"},
                  "las acciones no se escriben nunca");
        comprueba(linea_de_ordenes(ArgumentosCli(), {{"placa", "p.xml"}, {"firmware", "f.bin"},
                                                     {"--ms", "5"}},
                                   "--ms=7", &e) == QStringList{"p.xml", "f.bin", "--ms=7"},
                  "sin lista de opciones: los posicionales de siempre y lo escrito a mano");
        comprueba(como_texto("mcu-sim", {"a b", "c"}) == "mcu-sim 'a b' c",
                  "y para ensenarla, comillas donde hacen falta");
    }

    // -------------------------------------------------------------------------
    std::printf("G3 La configuracion\n");
    {
        QTemporaryDir tmp;
        Configuracion c;
        QString e;
        comprueba(Configuracion::lee(QStringLiteral(FUENTES "/config.ejemplo.json"), c, e),
                  "la plantilla versionada, config.ejemplo.json, se lee: " + e.toStdString());
        comprueba(c.ejecutable == "../mcu-sim/src/build/mcu-sim" &&
                      c.argumentos.value("placa") == "placas/discovery_min.xml" &&
                      c.argumentos.value("firmware") == "verif/fw/blinky/blinky.bin" &&
                      c.puerto == 3344 && c.ritmo == "real" && !c.argumentos.contains("_comentario"),
                  "con su ejecutable, su placa, su firmware, su puerto y su ritmo; los "
                  "comentarios no son argumentos");
        comprueba(c.ejecutable_absoluto() ==
                      QDir::cleanPath(QStringLiteral(FUENTES "/../mcu-sim/src/build/mcu-sim")),
                  "las rutas relativas, respecto al fichero");
        // Guardar sobre una copia, y volver a leer
        const QString ruta = tmp.filePath("config.json");
        QFile::copy(QStringLiteral(FUENTES "/config.ejemplo.json"), ruta);
        QFile::setPermissions(ruta, QFile::ReadOwner | QFile::WriteOwner);
        c.ruta = ruta;
        c.argumentos.insert("--ms", "2500");
        c.argumentos.insert("--ondas", "si");
        c.a_mano = "--traza-gdb";
        c.puerto = 4000;
        comprueba(!c.siempre_encima, "la plantilla dice siempre_encima = false");
        c.siempre_encima = true;
        comprueba(c.guarda(e), "se guarda: " + e.toStdString());
        Configuracion d;
        Configuracion::lee(ruta, d, e);
        comprueba(d.argumentos == c.argumentos && d.a_mano == c.a_mano && d.puerto == 4000 &&
                      d.ejecutable == c.ejecutable && d.ritmo == c.ritmo && d.siempre_encima,
                  "y se vuelve a leer igual, siempre_encima incluido");
        QFile f(ruta);
        comprueba(f.open(QIODevice::ReadOnly) && f.readAll().contains("_comentario"),
                  "los comentarios de la plantilla sobreviven a guardar");
        Configuracion n;
        comprueba(Configuracion::lee(tmp.filePath("no-existe.json"), n, e) && n.puerto == 3344 &&
                      n.ruta.endsWith("no-existe.json"),
                  "un fichero que no existe: la de por omision, con esa ruta para guardarla");
        QFile malo(tmp.filePath("malo.json"));
        const bool escrito = malo.open(QIODevice::WriteOnly) && malo.write("{ esto no es json") > 0;
        malo.close();
        const bool leido = Configuracion::lee(malo.fileName(), n, e);
        comprueba(escrito && !leido && e.contains("JSON"),
                  "y uno roto se dice: \"" + e.toStdString() + "\"");
    }

    // -------------------------------------------------------------------------
    std::printf("G4 El dialogo, construido con la lista\n");
    {
        const ArgumentosCli a = lista();
        Configuracion c;
        c.ejecutable = "mcu-sim";
        c.argumentos = {{"firmware", "fw.bin"}, {"--ondas", "si"}, {"--mcu", "STM32F405RG"}};
        DialogoLanzamiento d(c, "127.0.0.1:5555", &a);
        auto* placa = d.findChild<QLineEdit*>("arg:placa");
        auto* ms    = d.findChild<QLineEdit*>("arg:--ms");
        auto* mcu   = d.findChild<QComboBox*>("arg:--mcu");
        auto* ondas = d.findChild<QCheckBox*>("arg:--ondas");
        auto* gdb   = d.findChild<QCheckBox*>("arg:--gdb");
        auto* dap   = d.findChild<QCheckBox*>("arg:--gdb-dap");
        auto* serie = d.findChild<QLineEdit*>("arg:--serie");
        auto* orden = d.findChild<QLabel*>("orden");
        auto* lanzar = d.findChild<QPushButton*>("lanzar");
        comprueba(placa && ms && mcu && ondas && gdb && dap && serie && orden && lanzar,
                  "un campo por argumento: texto para placa, --ms y --serie, desplegable para "
                  "--mcu, casillas para las banderas");
        comprueba(!d.findChild<QWidget*>("arg:--gui") && !d.findChild<QWidget*>("arg:--tiempo-real") &&
                      !d.findChild<QWidget*>("arg:--help"),
                  "y ninguno para lo que no se ofrece");
        comprueba(mcu && mcu->count() == 4 && mcu->currentText() == "STM32F405RG" &&
                      mcu->itemText(0) == "(sin decir)" &&
                      ondas && ondas->isChecked() && ms && ms->text().isEmpty() &&
                      ms->placeholderText().contains("100"),
                  "con los valores de la configuracion; un campo vacio, con su omision de "
                  "muestra");
        comprueba(lanzar && !lanzar->isEnabled() && orden->text().contains("falta"),
                  "sin placa no se puede lanzar, y se dice");
        placa->setText("placas/discovery_min.xml");
        comprueba(lanzar->isEnabled() &&
                      orden->text().contains("mcu-sim placas/discovery_min.xml fw.bin "
                                             "--mcu=STM32F405RG --ondas --gui 127.0.0.1:5555"),
                  "con ella, si; y debajo la linea que va a salir: \"" +
                      orden->text().toStdString() + "\"");
        gdb->setChecked(true);
        dap->setChecked(true);
        comprueba(!gdb->isChecked() && dap->isChecked(),
                  "--gdb y --gdb-dap, del mismo grupo: marcar una desmarca la otra");
        ms->setText("2000");
        serie->setText("VCP=tcp:4000;X=memoria");
        const Configuracion r = d.configuracion();
        comprueba(r.argumentos.value("--ms") == "2000" && r.argumentos.value("--gdb-dap") == "si" &&
                      !r.argumentos.contains("--gdb") && r.argumentos.value("placa") ==
                      "placas/discovery_min.xml",
                  "y la configuracion sale con lo de los campos");
        comprueba(d.linea().contains("--serie=X=memoria") && d.linea().contains("--ms=2000"),
                  "y la linea, tambien");

        // De donde salen los valores
        auto origen = [&](const Configuracion& x) {
            DialogoLanzamiento o(x, "127.0.0.1:1", &a);
            auto* l = o.findChild<QLabel*>("origen");
            return l ? l->text() : QString();
        };
        QTemporaryDir tmp;
        Configuracion vacia;
        QString err;
        Configuracion::lee(tmp.filePath("aqui/config.json"), vacia, err);
        vacia.buscadas = {tmp.filePath("aqui/config.json"), tmp.filePath("usuario/config.json")};
        const QString t1 = origen(vacia);
        comprueba(t1.contains("No hay configuracion") && t1.contains("sale vacio") &&
                      t1.contains(QDir::toNativeSeparators(tmp.filePath("aqui/config.json"))) &&
                      t1.contains(QDir::toNativeSeparators(tmp.filePath("usuario/config.json"))),
                  "sin configuracion, el dialogo dice por que sale vacio, los dos sitios donde "
                  "se busco y donde se guardara");
        QFile f(tmp.filePath("leida.json"));
        const bool escrito = f.open(QIODevice::WriteOnly) && f.write("{}") > 0;
        f.close();
        Configuracion leida;
        Configuracion::lee(f.fileName(), leida, err);
        const QString t2 = origen(leida);
        comprueba(escrito && leida.existia && t2.contains("leida de") &&
                      t2.contains(QDir::toNativeSeparators(f.fileName())),
                  "con ella, de que fichero se leyo: \"" + t2.toStdString() + "\"");
        comprueba(origen(Configuracion()).contains("Sin fichero"),
                  "y sin fichero ninguno, que lo que se ponga no se guarda");

        Configuracion sin;
        sin.ejecutable = "/no/existe/mcu-sim";
        sin.argumentos = {{"placa", "p.xml"}};
        DialogoLanzamiento d2(sin, "127.0.0.1:1");
        auto* estado = d2.findChild<QLabel*>("estado");
        comprueba(estado && estado->text().contains("No se pueden leer") &&
                      d2.findChild<QLineEdit*>("arg:placa") &&
                      d2.findChild<QLineEdit*>("arg:firmware") &&
                      d2.findChild<QLineEdit*>("arg:a_mano"),
                  "sin un mcu-sim que conteste: lo dice, y deja placa, firmware y a mano (\"" +
                      d2.aviso().toStdString() + "\")");
    }

    // -------------------------------------------------------------------------
    std::printf("G5 La ventana, con el puerto cogido y una ruta mala\n");
    {
        QTcpServer okupa;
        okupa.listen(QHostAddress::LocalHost, 0);
        const quint16 cogido = okupa.serverPort();
        Configuracion c;
        c.puerto = cogido;
        c.ejecutable = "/no/existe/mcu-sim";
        c.argumentos = {{"placa", "p.xml"}};
        VentanaPrincipal v(c);
        v.show();
        auto* resumen = v.findChild<QLabel*>("resumen");
        comprueba(v.sesion().puerto() != 0 && v.sesion().puerto() != cogido &&
                      v.destino_gui() == QStringLiteral("127.0.0.1:%1").arg(v.sesion().puerto()) &&
                      resumen->text().contains("Esperando"),
                  "con el puerto " + std::to_string(cogido) + " cogido escucha en el " +
                      std::to_string(v.sesion().puerto()) + ", sin decir nada, y ese es el "
                      "que se le pasara al hijo");
        auto* consola = v.findChild<QPlainTextEdit*>("consola");
        comprueba(!v.lanza() && consola->toPlainText().contains("no hay ningun ejecutable") &&
                      v.statusBar()->currentMessage().contains("/no/existe/mcu-sim") &&
                      !v.lanzador().corriendo(),
                  "una ruta de ejecutable mala: no se lanza, y se dice en la consola y en la "
                  "barra de estado");
    }

    // -------------------------------------------------------------------------
    std::printf("G6 El firmware de cada MCU\n");
    {
        QVector<McuCli> m;
        QString e;
        const QByteArray salida =
            "\n        SystemC 2.3.4-Accellera\n"
            "<mcus placa=\"placas/x.xml\">\n"
            "  <mcu id=\"N/u0\" tipo=\"STM32F446RE\" firmware=\"verif/fw/a/a.bin\"/>\n"
            "  <mcu id=\"M/u0\" tipo=\"STM32F407VG\" firmware=\"\"/>\n"
            "</mcus>\n";
        comprueba(lee_mcus(salida, m, e) && m.size() == 2 && m[0].id == "N/u0" &&
                      m[0].tipo == "STM32F446RE" && m[0].firmware == "verif/fw/a/a.bin" &&
                      m[1].firmware.isEmpty(),
                  "mcu-sim --mcus se lee, con la cabecera de SystemC delante: " + e.toStdString());
        QVector<McuCli> m2;
        comprueba(!lee_mcus("uso: sim placa.xml", m2, e) && m2.isEmpty() && e.contains("--mcus"),
                  "y uno anterior, que no sabe, se dice: " + e.toStdString());

        const ArgumentosCli a = lista();
        const QVector<McuCli> uno{{"u0", "STM32F407VG", ""}};
        ValoresCli v{{"placa", "p.xml"}, {"firmware", "fw.bin"}};
        comprueba(linea_de_ordenes(a, v, {}, nullptr, uno) == QStringList({"p.xml", "fw.bin"}),
                  "con un MCU, el firmware es el posicional de siempre");
        v.insert("sin-firmware", "si");
        comprueba(linea_de_ordenes(a, v, {}, nullptr, uno) ==
                      QStringList({"p.xml", "--sin-firmware"}),
                  "y sin firmware, --sin-firmware en su lugar");
        ValoresCli v2{{"placa", "s.xml"}, {"firmware", "fw.bin"},
                      {"firmware:M/u0", "b.bin"}, {"sin-firmware:N/u0", "si"},
                      {"firmware:N/u0", "no_cuenta.bin"}, {"firmware:Z/u0", "otro_sistema.bin"}};
        comprueba(linea_de_ordenes(a, v2, {}, nullptr, m) ==
                      QStringList({"s.xml", "--sin-firmware=N/u0", "--firmware=M/u0=b.bin"}),
                  "con dos, el de cada uno con su id; el posicional no va, el fichero de uno "
                  "sin firmware tampoco, y el de un chip que no esta, menos");

        // El dialogo
        ArgumentosCli con = a;
        OpcionCli f;
        f.nombre = "--firmware";
        f.forma = "valor";
        f.tipo = "texto";
        f.repetible = true;
        OpcionCli sf = f;
        sf.nombre = "--sin-firmware";
        sf.forma = "valor_opcional";
        con.opciones << f << sf;
        Configuracion c;
        c.ejecutable = "mcu-sim";
        c.argumentos = {{"placa", "p.xml"}, {"firmware", "fw.bin"}};
        DialogoLanzamiento d(c, "127.0.0.1:5555", &con);
        comprueba(!d.findChild<QWidget*>("arg:--firmware") &&
                      !d.findChild<QWidget*>("arg:--sin-firmware") &&
                      d.findChild<QLineEdit*>("arg:firmware") &&
                      !d.findChild<QCheckBox*>("arg:sin-firmware"),
                  "sin saber sus MCUs, el firmware de siempre; --firmware y --sin-firmware "
                  "no salen como opciones sueltas");
        d.pon_mcus({{"u0", "STM32F407VG", "verif/fw/blinky/blinky.bin"}});
        auto* fw = d.findChild<QLineEdit*>("arg:firmware");
        auto* sin = d.findChild<QCheckBox*>("arg:sin-firmware");
        auto* orden = d.findChild<QLabel*>("orden");
        comprueba(fw && sin && fw->text() == "fw.bin" && !sin->isChecked() && fw->isEnabled() &&
                      fw->placeholderText().contains("verif/fw/blinky/blinky.bin"),
                  "con uno, su fila: el fichero de antes, de muestra el del XML, y la casilla "
                  "sin firmware al lado");
        if (!fw || !sin) return resultado();
        sin->setChecked(true);
        comprueba(!fw->isEnabled() && d.linea() == QStringList({"p.xml", "--sin-firmware"}) &&
                      orden->text().contains("--sin-firmware"),
                  "con la casilla, el fichero se apaga y sale --sin-firmware");
        sin->setChecked(false);
        fw->clear();
        comprueba(d.linea() == QStringList({"p.xml"}),
                  "vacio y sin casilla: el del XML, sin decir nada");
        fw->setText("fw.bin");
        d.pon_mcus(m);
        auto* fn = d.findChild<QLineEdit*>("arg:firmware:N/u0");
        auto* fm = d.findChild<QLineEdit*>("arg:firmware:M/u0");
        auto* sn = d.findChild<QCheckBox*>("arg:sin-firmware:N/u0");
        comprueba(fn && fm && sn && !d.findChild<QLineEdit*>("arg:firmware") &&
                      d.findChild<QLabel*>("firmwares") &&
                      fm->placeholderText().contains("sin firmware"),
                  "con dos, una fila por MCU en vez de la del posicional; la del que el XML "
                  "deja sin firmware lo dice");
        if (!fn || !fm || !sn) return resultado();
        fm->setText("b.bin");
        sn->setChecked(true);
        comprueba(d.linea() == QStringList({"p.xml", "--sin-firmware=N/u0", "--firmware=M/u0=b.bin"}),
                  "y la linea lleva el de cada uno: \"" + d.linea().join(' ').toStdString() + "\"");
        const Configuracion r = d.configuracion();
        comprueba(r.argumentos.value("firmware:M/u0") == "b.bin" &&
                      r.argumentos.value("sin-firmware:N/u0") == "si" &&
                      r.argumentos.value("firmware") == "fw.bin",
                  "la configuracion los guarda, y recuerda el de la placa de un solo chip");
        d.pon_mcus(uno);
        comprueba(d.findChild<QLineEdit*>("arg:firmware") &&
                      d.findChild<QLineEdit*>("arg:firmware")->text() == "fw.bin" &&
                      d.configuracion().argumentos.value("firmware:M/u0") == "b.bin",
                  "y al volver a una placa con un chip, sale el de antes; lo del sistema se "
                  "sigue recordando");
    }

    return resultado();
}
