// =============================================================================
// comun.h — lo que comparten las pruebas: el contador, la espera con el bucle
// de eventos en marcha, y un «modelo» falso sobre un QTcpSocket crudo que
// escribe y lee con el mismo proto_io.h.
// =============================================================================
#ifndef MCU_SIM_GUI_PRUEBAS_COMUN_H
#define MCU_SIM_GUI_PRUEBAS_COMUN_H

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QTcpSocket>

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "proto_io.h"

namespace prueba {

inline unsigned g_ok = 0, g_mal = 0;

inline bool comprueba(bool c, const std::string& que) {
    (c ? g_ok : g_mal)++;
    std::printf("  [%s] %s\n", c ? "OK  " : "FALLO", que.c_str());
    std::fflush(stdout);
    return c;
}

inline int resultado() {
    std::printf("RESULTADO %u ok, %u fallos\n", g_ok, g_mal);
    return g_mal ? 1 : 0;
}

// Deja correr el bucle de eventos hasta que se cumpla `c` o pase el plazo.
inline bool espera(const std::function<bool()>& c, int ms = 5000) {
    QElapsedTimer t;
    t.start();
    while (!c() && t.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    return c();
}

template <class T> std::string bytes(const T& t) {
    return std::string(reinterpret_cast<const char*>(&t), sizeof t);
}

struct Recibido { quint16 tipo; QByteArray cuerpo; };

// El «modelo»: lo que haría mcu-sim, a mano.
struct ModeloFalso {
    QTcpSocket                     s;
    mcusim::proto::Emisor          e;
    mcusim::proto::Lector          l{mcusim::proto::Origen::Pantalla};
    std::vector<Recibido>          leido;

    bool conecta(quint16 p) {
        s.connectToHost(QHostAddress::LocalHost, p);
        return s.waitForConnected(3000);
    }
    void manda(quint16 tipo, const std::string& cuerpo = std::string()) {
        std::string d;
        e.mensaje(d, tipo, cuerpo);
        s.write(d.data(), qint64(d.size()));
        s.flush();
    }
    void lee() {
        const QByteArray d = s.readAll();
        l.mete(d.constData(), std::size_t(d.size()));
        mcusim::proto::Mensaje m;
        while (l.saca(m) == mcusim::proto::Lector::LEC_MENSAJE)
            leido.push_back({m.tipo, QByteArray(reinterpret_cast<const char*>(m.cuerpo),
                                                int(m.longitud))});
    }
    // Espera a tener `n` mensajes leídos en total.
    bool espera_leidos(std::size_t n, int ms = 5000) {
        return espera([&] { lee(); return leido.size() >= n; }, ms);
    }
    void version(uint16_t v) { e.fija_version(v); l.fija_version(v); }
};

// Una placa parecida a la de la Discovery, en los dos XML del saludo. El
// catálogo, con el formato de doc/protocolo.md §3.
inline const char* PLACA_XML =
    "<placa nombre=\"discovery\">\n"
    "  <componente tipo=\"Crystal\" id=\"X3\" vdd=\"3.3\" conectada=\"no\">\n"
    "    <pin nombre=\"osc_in\" nodo=\"PC14\"/>\n"
    "  </componente>\n"
    "  <componente tipo=\"Led\" id=\"LD4\" a_vss=\"si\" vf=\"2.0\" r=\"680\">\n"
    "    <pin nombre=\"anodo\" nodo=\"PD12\"/>\n"
    "  </componente>\n"
    "  <componente tipo=\"Button\" id=\"B1\" v_cerrado=\"3.3\">\n"
    "    <pin nombre=\"pin\" nodo=\"PA0\"/>\n"
    "  </componente>\n"
    "  <componente tipo=\"Rpull\" id=\"R35\" v=\"0\" r=\"100000\">\n"
    "    <pin nombre=\"a\" nodo=\"PA0\"/>\n"
    "  </componente>\n"
    "</placa>\n";

inline const char* CATALOGO_XML =
    "<catalogo>\n"
    "  <pieza idx=\"0\" id=\"X3\" tipo=\"Crystal\">\n"
    "    <observable idx=\"0\" id_obs=\"0\" nombre=\"presente\" unidad=\"\" min=\"0\" max=\"1\" interesante=\"si\"/>\n"
    "  </pieza>\n"
    "  <pieza idx=\"1\" id=\"LD4\" tipo=\"Led\">\n"
    "    <observable idx=\"0\" id_obs=\"1\" nombre=\"encendido\" unidad=\"\" min=\"0\" max=\"1\" interesante=\"si\"/>\n"
    "    <observable idx=\"1\" id_obs=\"2\" nombre=\"corriente\" unidad=\"mA\" min=\"0\" max=\"25\" interesante=\"no\"/>\n"
    "  </pieza>\n"
    "  <pieza idx=\"2\" id=\"B1\" tipo=\"Button\">\n"
    "    <observable idx=\"0\" id_obs=\"3\" nombre=\"pulsado\" unidad=\"\" min=\"0\" max=\"1\" interesante=\"si\"/>\n"
    "    <mando idx=\"0\" nombre=\"pulsar\" tipo=\"boton\" min=\"0\" max=\"1\" valor=\"0\"/>\n"
    "    <mando idx=\"1\" nombre=\"rebote_ms\" tipo=\"continuo\" min=\"0\" max=\"20\" valor=\"2\"/>\n"
    "    <mando idx=\"2\" nombre=\"rebotes\" tipo=\"discreto\" min=\"1\" max=\"9\" valor=\"5\"/>\n"
    "  </pieza>\n"
    "  <pieza idx=\"3\" id=\"R35\" tipo=\"Rpull\"/>\n"
    "</catalogo>\n";

// UN SISTEMA (versión 2 del protocolo): una Nucleo y un shield, con un
// conector de cada lado enchufado. Es lo que manda `mcu-sim` para
// placas/nucleo_y_shield.xml, recortado: aplanado, con los nombres
// cualificados y los conectores ya resueltos.
inline const char* SISTEMA_XML =
    "<sistema nombre=\"nucleo-y-shield\">\n"
    "  <placa id=\"N\" nombre=\"nucleo-f446re\" fichero=\"nucleo_f446re.xml\"/>\n"
    "  <placa id=\"S\" nombre=\"shield-leds\" fichero=\"shield_leds.xml\"/>\n"
    "  <mcu tipo=\"STM32F446RE\" id=\"N/u0\"/>\n"
    "  <componente tipo=\"Led\" id=\"N/LD2\" a_vss=\"si\">\n"
    "    <pin nombre=\"anodo\" nodo=\"N/u0.PA5\"/>\n"
    "  </componente>\n"
    "  <componente tipo=\"Conector\" id=\"N/CN5\" columnas=\"10\" filas=\"1\">\n"
    "    <pin nombre=\"1\" nodo=\"N/CN5.1\"/>\n"
    "    <pin nombre=\"2\" nodo=\"N/CN5.2\"/>\n"
    "    <pin nombre=\"3\" nodo=\"N/CN5.3\"/>\n"
    "    <pin nombre=\"4\" nodo=\"N/CN5.4\"/>\n"
    "    <pin nombre=\"5\" nodo=\"N/CN5.5\"/>\n"
    "    <pin nombre=\"6\" nodo=\"N/u0.PA5\"/>\n"
    "    <pin nombre=\"7\" nodo=\"N/CN5.7\"/>\n"
    "    <pin nombre=\"8\" nodo=\"N/CN5.8\"/>\n"
    "    <pin nombre=\"9\" nodo=\"N/CN5.9\"/>\n"
    "    <pin nombre=\"10\" nodo=\"N/CN5.10\"/>\n"
    "  </componente>\n"
    "  <componente tipo=\"Conector\" id=\"S/J5\" columnas=\"10\" filas=\"1\">\n"
    "    <pin nombre=\"1\" nodo=\"N/CN5.1\"/>\n"
    "    <pin nombre=\"2\" nodo=\"N/CN5.2\"/>\n"
    "    <pin nombre=\"3\" nodo=\"N/CN5.3\"/>\n"
    "    <pin nombre=\"4\" nodo=\"N/CN5.4\"/>\n"
    "    <pin nombre=\"5\" nodo=\"N/CN5.5\"/>\n"
    "    <pin nombre=\"6\" nodo=\"N/u0.PA5\"/>\n"
    "    <pin nombre=\"7\" nodo=\"N/CN5.7\"/>\n"
    "    <pin nombre=\"8\" nodo=\"N/CN5.8\"/>\n"
    "    <pin nombre=\"9\" nodo=\"N/CN5.9\"/>\n"
    "    <pin nombre=\"10\" nodo=\"N/CN5.10\"/>\n"
    "  </componente>\n"
    "  <componente tipo=\"Led\" id=\"S/LD_D13\" a_vss=\"si\">\n"
    "    <pin nombre=\"anodo\" nodo=\"N/u0.PA5\"/>\n"
    "  </componente>\n"
    "  <acopla a=\"N/CN5\" b=\"S/J5\"/>\n"
    "  <hilo a=\"N/CN5.7\" b=\"S/J5.8\"/>\n"
    "</sistema>\n";

// UNA PILA (PC/104), como la manda el mcu-sim de ahora: cada placa descrita
// dentro de su <placa id> -piezas, chips, conectores con su forma- y un
// acople de TRES conectores, con las placas que une. Y un hilo de la CPU a
// la última placa. Las piezas se omiten: aquí solo importa la estructura.
inline const char* PILA_XML =
    "<sistema nombre=\"pila-pc104\">\n"
    "  <placa id=\"CPU\" nombre=\"pc104-cpu\" fichero=\"pc104_cpu.xml\" piezas=\"2\">\n"
    "    <mcu ref=\"CPU/u0\" tipo=\"STM32F407VG\"/>\n"
    "    <conector ref=\"CPU/J1\" filas=\"2\" columnas=\"32\" numeracion=\"zigzag\" acople=\"0\"/>\n"
    "  </placa>\n"
    "  <placa id=\"L1\" nombre=\"pc104-leds\" fichero=\"pc104_leds.xml\" piezas=\"3\">\n"
    "    <conector ref=\"L1/J1\" filas=\"2\" columnas=\"32\" numeracion=\"zigzag\" acople=\"0\"/>\n"
    "  </placa>\n"
    "  <placa id=\"L2\" nombre=\"pc104-leds\" fichero=\"pc104_leds.xml\" piezas=\"3\">\n"
    "    <conector ref=\"L2/J1\" filas=\"2\" columnas=\"32\" numeracion=\"zigzag\" acople=\"0\"/>\n"
    "    <conector ref=\"L2/J9\" filas=\"1\" columnas=\"8\" numeracion=\"filas\"/>\n"
    "  </placa>\n"
    "  <mcu tipo=\"STM32F407VG\" id=\"CPU/u0\"/>\n"
    "  <acopla n=\"0\" conectores=\"CPU/J1 L1/J1 L2/J1\" placas=\"CPU L1 L2\"/>\n"
    "  <hilo a=\"CPU/u0.PA2\" b=\"L2/J9.1\" placas=\"CPU L2\"/>\n"
    "</sistema>\n";

inline const char* CATALOGO_SISTEMA_XML =
    "<catalogo>\n"
    "  <pieza idx=\"0\" id=\"N/LD2\" tipo=\"Led\">\n"
    "    <observable idx=\"0\" id_obs=\"0\" nombre=\"encendido\" unidad=\"\" min=\"0\" max=\"1\" interesante=\"si\"/>\n"
    "  </pieza>\n"
    "  <pieza idx=\"1\" id=\"N/CN5\" tipo=\"Conector\"/>\n"
    "  <pieza idx=\"2\" id=\"S/J5\" tipo=\"Conector\"/>\n"
    "  <pieza idx=\"3\" id=\"S/LD_D13\" tipo=\"Led\">\n"
    "    <observable idx=\"0\" id_obs=\"1\" nombre=\"encendido\" unidad=\"\" min=\"0\" max=\"1\" interesante=\"si\"/>\n"
    "  </pieza>\n"
    "</catalogo>\n";

// El saludo entero desde el lado del modelo, hasta T_LISTO incluido.
inline bool saluda_hasta_listo(ModeloFalso& m) {
    using namespace mcusim::proto;
    m.manda(T_HOLA, "protocolo_max=1\nplaca=placas/discovery.xml\nmcu=STM32F407VG\n"
                    "firmware=blinky.bin\n");
    if (!m.espera_leidos(1) || m.leido[0].tipo != T_VERSION) return false;
    m.version(1);
    m.manda(T_PLACA, PLACA_XML);
    m.manda(T_CATALOGO, CATALOGO_XML);
    m.manda(T_LISTO);
    return true;
}

} // namespace prueba

#endif // MCU_SIM_GUI_PRUEBAS_COMUN_H
