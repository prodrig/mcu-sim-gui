// =============================================================================
// prueba_serie.cpp — La pestaña de un puente serie en memoria
//
//   S1  las tramas, sin widgets (`trama_serie.h`): ASCII y \xNN, \xNNMM... de
//       dos en dos cifras, mezclados; los escapes de C; los errores, con su
//       posición; los tres CRC contra valores conocidos -el ejemplo de la
//       especificación de Modbus y el CRC-16/MODBUS de «123456789»-; el orden
//       de lo escrito, el CRC, CR y LF, y que CR y LF no entran en el CRC;
//   S2  lo que llega, hecho texto: CR, LF y CR+LF son un salto, también
//       partido entre dos mensajes; lo que no se imprime, \xNN;
//   S3  la pestaña sola: los dos colores por omisión y sus iconos; cada
//       sentido de su color, también después de cambiarlos; la entrada, la
//       vista previa y Enviar, que solo vale en marcha y con una trama buena;
//   S4  la ventana entera contra un modelo falso que anuncia un puente: una
//       pestaña con su nombre, T_SERIE a ella, T_SERIE_ENVIA con lo escrito,
//       solo en marcha, y el nombre en azul si llega algo y no se mira;
//   S5  con un mcu-sim de verdad (MCU_SIM y MCU_SIM_SRC, como `cruzada`; sin
//       ellas se salta): la Nucleo con vcp_demo y `--serie VCP=memoria`. El
//       saludo del firmware llega a la pestaña, y lo que se escribe en ella
//       vuelve con el eco. Con PRUEBA_CAPTURA=fichero.png, la ventana entera.
// =============================================================================
#include <cstring>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QToolButton>

#include "comun.h"
#include "consola_serie.h"
#include "trama_serie.h"
#include "ventana_principal.h"

using namespace mcusim;
using namespace mcusim::proto;
using namespace prueba;

namespace {
QByteArray B(std::initializer_list<int> v)
{
    QByteArray b;
    for (int x : v) b.append(char(x));
    return b;
}
std::string hex(const QByteArray& b) { return b.toHex(' ').toStdString(); }

QByteArray trama_de(const QString& t, bool cr, bool lf, bool crc, trama::Crc tipo,
                    QString* e = nullptr)
{
    trama::Opciones o;
    o.cr = cr; o.lf = lf; o.crc = crc; o.tipo = tipo;
    QByteArray out;
    if (!trama::construye(t, o, out, e)) return QByteArray("ERROR");
    return out;
}

// El color con que está pintado el carácter `pos` del texto
QColor color_en(QPlainTextEdit* v, int pos)
{
    QTextCursor c(v->document());
    c.setPosition(pos + 1);
    return c.charFormat().foreground().color();
}

const char* CATALOGO_SERIE =
    "<catalogo>\n"
    "  <pieza idx=\"0\" id=\"LD2\" tipo=\"Led\">\n"
    "    <observable idx=\"0\" id_obs=\"0\" nombre=\"encendido\" unidad=\"\" min=\"0\" max=\"1\" interesante=\"si\"/>\n"
    "  </pieza>\n"
    "  <pieza idx=\"1\" id=\"VCP\" tipo=\"PuenteSerie\">\n"
    "    <serie destino=\"memoria\" formato=\"115200 8N1\"/>\n"
    "  </pieza>\n"
    "  <pieza idx=\"2\" id=\"RED\" tipo=\"PuenteSerie\">\n"
    "    <serie destino=\"tcp\" formato=\"9600 8N1\"/>\n"
    "  </pieza>\n"
    "</catalogo>\n";
const char* PLACA_SERIE =
    "<placa nombre=\"nucleo-f446re\">\n"
    "  <componente tipo=\"Led\" id=\"LD2\"><pin nombre=\"anodo\" nodo=\"PA5\"/></componente>\n"
    "  <componente tipo=\"PuenteSerie\" id=\"VCP\"><pin nombre=\"rx\" nodo=\"PA2\"/></componente>\n"
    "  <componente tipo=\"PuenteSerie\" id=\"RED\"><pin nombre=\"rx\" nodo=\"PA9\"/></componente>\n"
    "</placa>\n";
} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    using trama::Crc;

    // -------------------------------------------------------------------------
    std::printf("S1 Lo escrito, en bytes, y los CRC\n");
    {
        QByteArray b;
        QString e;
        comprueba(trama::desescapa("hola", b) && b == "hola", "ASCII tal cual");
        comprueba(trama::desescapa("\\x41", b) && b == "A", "\\x41 es una A");
        comprueba(trama::desescapa("\\x010203", b) && b == B({1, 2, 3}),
                  "\\x010203: tantos bytes como pares de cifras, tres");
        const bool mez = trama::desescapa("AT\\x0D\\x0aOK\\xFF-fin", b);
        comprueba(mez && b == QByteArray("AT\r\nOK\xFF") + "-fin",
                  "mezclados: ASCII, hex en mayusculas y en minusculas, y ASCII otra vez: " +
                      hex(b));
        comprueba(trama::desescapa("\\xABCDzz", b) && b == B({0xAB, 0xCD, 'z', 'z'}),
                  "el hexadecimal se acaba en lo primero que no es una cifra");
        comprueba(trama::desescapa("a\\r\\n\\t\\0\\\\b", b) &&
                      b == QByteArray("a\r\n\t\0\\b", 7),
                  "\\r \\n \\t \\0 y \\\\, los de C");
        {
            const bool ok_ = trama::desescapa("ab\\x123", b, &e);
            comprueba(!ok_ && e.contains("3 cifras") &&
                          e.contains("posicion 3"),
                      "un numero impar de cifras es un error, que dice donde: \"" +
                          e.toStdString() + "\"");
        }
        {
            const bool ok_ = trama::desescapa("\\xzz", b, &e);
            comprueba(!ok_ && e.contains("sin cifras"),
                      "\\x sin cifras, error: \"" + e.toStdString() + "\"");
        }
        {
            const bool ok_ = trama::desescapa("\\q", b, &e);
            comprueba(!ok_ && e.contains("no es un escape"),
                      "un escape que no existe, error: \"" + e.toStdString() + "\"");
        }
        {
            const bool ok_ = trama::desescapa("fin\\", b, &e);
            comprueba(!ok_ && e.contains("sin nada detras"),
                      "una barra al final, error");
        }
        {
            const bool ok_ = trama::desescapa(QString::fromUtf8("año"), b, &e);
            comprueba(!ok_ && e.contains("ASCII") &&
                          e.contains("posicion 2"),
                      "lo que no es ASCII, error: \"" + e.toStdString() + "\"");
        }

        // Los tres cálculos, contra lo conocido
        comprueba(trama::crc16_modbus("123456789") == 0x4B37,
                  "CRC-16/MODBUS de \"123456789\" = 0x4B37, el valor de comprobacion del "
                  "catalogo de CRC");
        // Modbus over Serial Line V1.02, §6.2.2: 02 07 -> CRC 0x1241, «41 12»
        comprueba(trama::crc16_modbus(B({0x02, 0x07})) == 0x1241,
                  "y el de 02 07, el del ejemplo de la especificacion: 0x1241");
        comprueba(trama_de("\\x0103000A0001", false, false, true, Crc::ModbusRtu) ==
                      B({0x01, 0x03, 0x00, 0x0A, 0x00, 0x01, 0xA4, 0x08}),
                  "Modbus RTU: 01 03 00 0A 00 01 lleva A4 08, el byte bajo primero");
        comprueba(trama::lrc_modbus(B({0x11, 0x03, 0x00, 0x6B, 0x00, 0x03})) == 0x7E,
                  "LRC de 11 03 00 6B 00 03 = 0x7E (el ejemplo de siempre)");
        comprueba(trama_de(":1103006B0003", true, true, true, Crc::ModbusAscii) ==
                      QByteArray(":1103006B00037E\r\n"),
                  "Modbus ASCII: el LRC de las parejas de cifras tras ':', como dos "
                  "caracteres, y luego CR y LF: \":1103006B00037E\\r\\n\"");
        comprueba(trama_de("1103006b0003", false, false, true, Crc::ModbusAscii) ==
                      QByteArray("1103006b00037E"),
                  "sin los dos puntos tambien, y con minusculas");
        QString ea;
        {
            const QByteArray r_ = trama_de(":110", false, false, true, Crc::ModbusAscii, &ea);
            comprueba(r_ == "ERROR" &&
                          ea.contains("par"),
                      "Modbus ASCII con un numero impar de cifras: error: \"" + ea.toStdString() + "\"");
        }
        {
            const QByteArray r_ = trama_de(":11GG", false, false, true, Crc::ModbusAscii, &ea);
            comprueba(r_ == "ERROR" &&
                          ea.contains("GG"),
                      "y con algo que no es una cifra: \"" + ea.toStdString() + "\"");
        }
        comprueba(trama::suma256("ABC") == quint8(0x41 + 0x42 + 0x43) &&
                      trama::suma256(B({0xFF, 0x02})) == 0x01,
                  "la suma modulo 256: 0xC6 de ABC, y 0xFF + 0x02 da la vuelta a 0x01");

        // El orden, y que CR y LF no entran
        comprueba(trama_de("AB", true, false, false, Crc::Suma256) == "AB\r" &&
                      trama_de("AB", false, true, false, Crc::Suma256) == "AB\n" &&
                      trama_de("AB", true, true, false, Crc::Suma256) == "AB\r\n",
                  "CR, LF y los dos, primero CR y luego LF");
        comprueba(trama_de("AB", true, true, true, Crc::Suma256) ==
                      QByteArray("AB") + char(0x83) + "\r\n",
                  "con suma: A + B = 0x83, y CR y LF detras, sin entrar en ella");
        const QByteArray rtu = trama_de("\\x0207", true, true, true, Crc::ModbusRtu);
        comprueba(rtu == B({0x02, 0x07, 0x41, 0x12, 0x0D, 0x0A}),
                  "con Modbus RTU: 02 07 41 12 0D 0A, el CRC antes que CR y LF: " + hex(rtu));
        comprueba(trama::crcs_conocidos().size() == 3 &&
                      trama::crcs_conocidos()[0].id == Crc::ModbusRtu &&
                      trama::crcs_conocidos()[1].id == Crc::ModbusAscii &&
                      trama::crcs_conocidos()[2].id == Crc::Suma256,
                  "la lista: Modbus RTU, Modbus ASCII y la suma modulo 256");
    }

    // -------------------------------------------------------------------------
    std::printf("S2 Lo que llega, hecho texto\n");
    {
        bool cr = false;
        comprueba(trama::legible("hola\r\nadios\n", cr) == "hola\nadios\n" && !cr,
                  "CR+LF y LF solo: un salto cada uno");
        cr = false;
        comprueba(trama::legible("uno\rdos", cr) == "uno\ndos", "un CR solo tambien");
        cr = false;
        const QString a = trama::legible("OK\r", cr);
        const QString z = trama::legible("\nsigue", cr);
        comprueba(a + z == "OK\nsigue",
                  "y un CR+LF partido entre dos mensajes es un salto, no dos");
        cr = false;
        comprueba(trama::legible(B({0x00, 'a', 0x7F, 0x80, 0xFF, '\t'}), cr) ==
                      "\\x00a\\x7F\\x80\\xFF\t",
                  "lo que no se imprime, \\xNN; el tabulador, tal cual");
    }

    // -------------------------------------------------------------------------
    std::printf("S3 La pestana sola\n");
    {
        ConsolaSerie c(7, "VCP", "115200 8N1");
        c.show();
        comprueba(c.color_enviado() == QColor(0x80, 0x80, 0x00) &&
                      c.color_recibido() == QColor(0x00, 0x00, 0xFF),
                  "por omision, lo enviado verde oliva (#808000) y lo recibido azul (#0000ff)");
        auto* btx = c.findChild<QToolButton*>("color_enviado");
        auto* brx = c.findChild<QToolButton*>("color_recibido");
        const QImage itx = btx ? btx->icon().pixmap(22, 22).toImage() : QImage();
        const QImage irx = brx ? brx->icon().pixmap(22, 22).toImage() : QImage();
        comprueba(!itx.isNull() && !irx.isNull() &&
                      QColor(itx.pixel(5, 5)) == QColor(0x80, 0x80, 0x00) &&
                      QColor(irx.pixel(5, 5)) == QColor(0x00, 0x00, 0xFF),
                  "dos iconos para elegirlos, cada uno de su color");
        auto* fmt = c.findChild<QLabel*>("formato");
        comprueba(fmt && fmt->text().contains("VCP") && fmt->text().contains("115200 8N1"),
                  "arriba, el nombre del puente y su formato");

        c.recibido("vcp_demo listo\r\n");
        c.enviado("AT\r\n");
        c.recibido("OK");
        c.enviado("x");
        auto* vista = c.findChild<QPlainTextEdit*>("trafico");
        comprueba(vista && c.texto() == "vcp_demo listo\nAT\nOK\nx",
                  "lo recibido y lo enviado, por lineas; y al cambiar de sentido a media "
                  "linea, lo nuevo empieza en otra: \"" + c.texto().toStdString() + "\"");
        comprueba(color_en(vista, 0) == c.color_recibido() &&
                      color_en(vista, 15) == c.color_enviado() &&
                      color_en(vista, 18) == c.color_recibido() &&
                      color_en(vista, 21) == c.color_enviado(),
                  "cada sentido de su color");
        c.pon_colores(QColor("#ff0000"), QColor("#00aa00"));
        comprueba(color_en(vista, 0) == QColor("#00aa00") &&
                      color_en(vista, 15) == QColor("#ff0000"),
                  "y al cambiarlos, tambien lo que ya estaba escrito");
        c.pon_colores(QColor("#123456"), QColor("#123456"));
        c.pon_colores(QColor("#808000"), QColor("#0000ff"));
        comprueba(color_en(vista, 0) == QColor("#0000ff") &&
                      color_en(vista, 15) == QColor("#808000"),
                  "aunque los dos hayan sido el mismo un momento: cada trozo sabe de quien es");
        comprueba(c.bytes_recibidos() == 18 && c.bytes_enviados() == 5,
                  "y cuenta los bytes de cada sentido");

        auto* ent = c.findChild<QLineEdit*>("entrada");
        auto* env = c.findChild<QPushButton*>("enviar");
        auto* cr = c.findChild<QCheckBox*>("cr");
        auto* lf = c.findChild<QCheckBox*>("lf");
        auto* crc = c.findChild<QCheckBox*>("crc");
        auto* tipo = c.findChild<QComboBox*>("tipo_crc");
        auto* previa = c.findChild<QLabel*>("previa");
        comprueba(ent && env && cr && lf && crc && tipo && previa &&
                      cr->text().contains("CR") && cr->text().contains("\\x0D") &&
                      lf->text().contains("LF") && lf->text().contains("\\x0A") &&
                      crc->text().contains("CRC") && tipo->count() == 3,
                  "la entrada, Enviar, las tres casillas y la lista de CRC");
        comprueba(!tipo->isEnabled(), "la lista, apagada mientras no se pida el CRC");
        std::vector<QByteArray> mandado;
        QObject::connect(&c, &ConsolaSerie::envia,
                         [&](quint16 p, const QByteArray& b) { if (p == 7) mandado.push_back(b); });
        ent->setText("\\x0207");
        cr->setChecked(true);
        lf->setChecked(true);
        crc->setChecked(true);
        comprueba(tipo->isEnabled() && previa->text().contains("6 bytes:") &&
                      previa->text().contains("02 07 41 12 0D 0A"),
                  "debajo, los bytes que saldrian: \"" + previa->text().toStdString() + "\"");
        comprueba(!env->isEnabled(), "pero con la simulacion parada, Enviar no vale");
        c.activa(true);
        comprueba(env->isEnabled(), "en marcha, si");
        env->click();
        comprueba(mandado.size() == 1 && mandado[0] == B({0x02, 0x07, 0x41, 0x12, 0x0D, 0x0A}),
                  "Enviar pide mandar la trama entera");
        comprueba(ent->selectedText() == ent->text(),
                  "y lo escrito se queda seleccionado, para repetirlo o escribir encima");
        tipo->setCurrentIndex(2);
        crc->setChecked(false);
        ent->setText("hola\\x");
        comprueba(!env->isEnabled() && previa->text().contains("sin cifras"),
                  "una entrada que no vale apaga Enviar y dice por que");
        QMetaObject::invokeMethod(ent, "returnPressed");
        comprueba(mandado.size() == 1, "e Intro tampoco manda nada");
        ent->setText("hola");
        QMetaObject::invokeMethod(ent, "returnPressed");
        comprueba(mandado.size() == 2 && mandado[1] == "hola\r\n",
                  "Intro con una buena manda, como Enviar");
        c.activa(false);
        comprueba(!env->isEnabled(), "y al terminar, se apaga otra vez");
    }

    // -------------------------------------------------------------------------
    std::printf("S4 La ventana, con un puente en memoria\n");
    {
        VentanaPrincipal v(0);
        v.show();
        ModeloFalso m;
        m.conecta(v.sesion().puerto());
        m.manda(T_HOLA, "protocolo_max=1\nplaca=placas/nucleo.xml\nmcu=STM32F446RE\n"
                        "firmware=vcp_demo.bin\n");
        comprueba(m.espera_leidos(1) && m.leido[0].tipo == T_VERSION, "el saludo");
        m.version(1);
        m.manda(T_PLACA, PLACA_SERIE);
        m.manda(T_CATALOGO, CATALOGO_SERIE);
        m.manda(T_LISTO);
        auto* abajo = v.findChild<QTabWidget*>("abajo");
        comprueba(espera([&] { return v.series().size() == 1; }) && abajo &&
                      abajo->count() == 3 && abajo->tabText(2) == "VCP" &&
                      v.series()[0]->pieza() == 1,
                  "una pestana para el puente en memoria, con su nombre, junto a Avisos y "
                  "mcu-sim; el que va por red no tiene");
        ConsolaSerie* c = v.series().isEmpty() ? nullptr : v.series()[0];
        if (!c) return resultado();
        auto* env = c->findChild<QPushButton*>("enviar");
        auto* ent = c->findChild<QLineEdit*>("entrada");
        ent->setText("hola");
        comprueba(!env->isEnabled(), "antes de arrancar no se puede mandar");
        m.espera_leidos(2);                       // T_SUSCRIBE
        const std::size_t antes = m.leido.size();
        v.findChild<QPushButton*>("arrancar")->click();
        comprueba(m.espera_leidos(antes + 1) && m.leido.back().tipo == T_ARRANCA &&
                      espera([&] { return env->isEnabled(); }),
                  "en marcha, si");
        abajo->setCurrentIndex(0);
        m.manda(T_SERIE, bytes(CabSerie{1000, 1, 0, 16}) + "vcp_demo listo\r\n");
        comprueba(espera([&] { return c->bytes_recibidos() == 16; }) &&
                      c->texto().startsWith("vcp_demo listo"),
                  "T_SERIE a la pieza 1 llega a su pestana");
        comprueba(abajo->tabBar()->tabTextColor(2) == QColor(0x1f, 0x6f, 0xd0),
                  "y como no se esta mirando, su nombre sale en azul");
        abajo->setCurrentIndex(2);
        comprueba(!abajo->tabBar()->tabTextColor(2).isValid(), "al mirarla, vuelve a su color");
        m.manda(T_SERIE, bytes(CabSerie{2000, 0, 0, 3}) + "LD2");
        m.manda(T_SERIE, bytes(CabSerie{2000, 1, 0, 9}) + "corto");
        comprueba(espera([&] { return v.statusBar()->currentMessage().contains("no mide"); }) &&
                      c->bytes_recibidos() == 16,
                  "a otra pieza no va a ninguna pestana, y uno que no mide lo que dice se "
                  "tira y se dice");
        c->findChild<QCheckBox*>("cr")->setChecked(true);
        const std::size_t n0 = m.leido.size();
        env->click();
        CabSerie cs{};
        comprueba(m.espera_leidos(n0 + 1) && m.leido.back().tipo == T_SERIE_ENVIA &&
                      m.leido.back().cuerpo.size() == int(sizeof cs + 5) &&
                      (std::memcpy(&cs, m.leido.back().cuerpo.constData(), sizeof cs), true) &&
                      cs.pieza == 1 && cs.n == 5 &&
                      m.leido.back().cuerpo.mid(int(sizeof cs)) == "hola\r",
                  "Enviar manda T_SERIE_ENVIA a la pieza 1 con 'hola\\r'");
        comprueba(c->bytes_enviados() == 5 && c->texto().endsWith("hola\n"),
                  "y lo pinta como enviado");
        m.manda(T_FIN, bytes(Fin{M_PARA, 0, 3000000ull}));
        comprueba(espera([&] { return !env->isEnabled(); }),
                  "con T_FIN no se puede mandar mas, y la pestana se queda para leerla");
    }

    // -------------------------------------------------------------------------
    std::printf("S5 Con el mcu-sim de verdad\n");
    const QString sim = qEnvironmentVariable("MCU_SIM");
    const QString src = qEnvironmentVariable("MCU_SIM_SRC");
    if (sim.isEmpty() || src.isEmpty()) {
        std::printf("  [SALTA] sin MCU_SIM y MCU_SIM_SRC\n");
        return resultado();
    }
    {
        VentanaPrincipal v(0);
        v.resize(1100, 760);
        v.show();
        QProcess p;
        p.setWorkingDirectory(src);
        p.start(sim, {"placas/nucleo_f446re_vcp.xml", "verif/fw/vcp_demo/vcp_demo.bin",
                      "--serie", "VCP=memoria", "--gui", v.destino_gui()});
        comprueba(espera([&] { return v.series().size() == 1; }, 20000) &&
                      espera([&] { return v.findChild<QPushButton*>("arrancar")->isEnabled(); }),
                  "mcu-sim se conecta y la ventana abre la pestana VCP");
        ConsolaSerie* c = v.series().isEmpty() ? nullptr : v.series()[0];
        if (c) {
            auto* abajo = v.findChild<QTabWidget*>("abajo");
            abajo->setCurrentWidget(c);
            v.findChild<QComboBox*>("ritmo")->setCurrentIndex(0);       // tiempo real
            v.findChild<QPushButton*>("arrancar")->click();
            const bool saludo = espera([&] { return c->texto().contains("vcp_demo listo"); }, 20000);
            comprueba(saludo,
                      "el saludo de vcp_demo llega a la pestana: \"" +
                          c->texto().left(40).toStdString() + "\"");
            auto* ent = c->findChild<QLineEdit*>("entrada");
            c->findChild<QCheckBox*>("cr")->setChecked(true);
            c->findChild<QCheckBox*>("lf")->setChecked(true);
            ent->setText("hola \\x41\\x42!");
            const quint64 r0 = c->bytes_recibidos();
            QMetaObject::invokeMethod(ent, "returnPressed");
            comprueba(espera([&] { return c->bytes_recibidos() >= r0 + 10; }, 20000) &&
                          c->texto().endsWith("hola AB!\nhola AB!\n"),
                      "'hola \\x41\\x42!' con CR y LF sale, y vuelve con el eco de vcp_demo: "
                      "una vez de cada color");
            ent->setText("\\x01\\x7F\\xFF");
            c->findChild<QCheckBox*>("crc")->setChecked(true);
            const quint64 r1 = c->bytes_recibidos();
            c->findChild<QPushButton*>("enviar")->click();
            const bool vuelve = espera([&] { return c->bytes_recibidos() >= r1 + 7; }, 20000);
            comprueba(vuelve && c->texto().endsWith("\\x01\\x7F\\xFF@p\n\\x01\\x7F\\xFF@p\n"),
                      "y tres bytes que no son texto, con su CRC de Modbus RTU (40 70, \"@p\"), igual: \"" +
                          c->texto().right(40).toStdString() + "\"");
            const QString cap = qEnvironmentVariable("PRUEBA_CAPTURA");
            if (!cap.isEmpty()) v.grab().save(cap);
        }
        auto* parar = v.findChild<QPushButton*>("parar");
        comprueba(parar->isEnabled(), "Parar, habilitado");
        parar->click();
        const bool acabo = espera([&] { return p.state() == QProcess::NotRunning; }, 20000);
        if (!acabo || p.exitCode() != 0)
            std::printf("%s\n%s\n", p.readAllStandardOutput().right(800).constData(),
                        p.readAllStandardError().right(800).constData());
        comprueba(acabo && p.exitCode() == 0, "Parar: mcu-sim termina con 0");
        if (!acabo) p.kill();
    }
    return resultado();
}
