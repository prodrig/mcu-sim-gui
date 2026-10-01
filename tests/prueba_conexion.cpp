// =============================================================================
// prueba_conexion.cpp — el extremo de la GUI del transporte, sin ventana
//
// Fase 2 del plan. La `Conexion` de verdad escuchando, y un cliente que hace de
// modelo sobre un `QTcpSocket` crudo, escribiendo el marco con el mismo
// `proto_io.h`. Solo QtCore y QtNetwork: no abre ninguna ventana, así que corre
// en un servidor sin pantalla y en el CI.
//
// Es la contraparte de `make gui-proto` en mcu-sim, que prueba el otro extremo
// con sockets de Berkeley y Winsock. Aquí se prueba lo que solo tiene este
// lado: que el bucle de eventos de Qt entrega los mensajes enteros aunque
// lleguen troceados, que los desconocidos se saltan, que un segundo modelo se
// rechaza y que algo que no es el protocolo cierra la conexión diciendo por qué.
//
//   cmake --build build && ctest --test-dir build --output-on-failure
//
// Código de salida 0 si todo va bien.
// =============================================================================
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTcpSocket>

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "conexion.h"

using namespace mcusim;
using namespace mcusim::proto;

namespace {

unsigned g_ok = 0, g_mal = 0;
bool comprueba(bool c, const std::string& que) {
    (c ? g_ok : g_mal)++;
    std::printf("  [%s] %s\n", c ? "OK  " : "FALLO", que.c_str());
    return c;
}

// Deja correr el bucle de eventos hasta que se cumpla `c` o pase el plazo.
bool espera(const std::function<bool()>& c, int ms = 3000) {
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

// El «modelo»: un QTcpSocket crudo que escribe con el Emisor de proto_io.h.
struct Modelo {
    QTcpSocket s;
    Emisor     e;
    Lector     l{Origen::Pantalla};
    std::vector<Recibido> leido;

    bool conecta(quint16 p) {
        s.connectToHost(QHostAddress::LocalHost, p);
        return s.waitForConnected(3000);
    }
    void escribe(const std::string& d) { s.write(d.data(), qint64(d.size())); s.flush(); }
    void manda(quint16 tipo, const std::string& cuerpo) {
        std::string d;
        e.mensaje(d, tipo, cuerpo);
        escribe(d);
    }
    void lee() {
        const QByteArray d = s.readAll();
        l.mete(d.constData(), std::size_t(d.size()));
        Mensaje m;
        while (l.saca(m) == Lector::LEC_MENSAJE)
            leido.push_back({m.tipo, QByteArray(reinterpret_cast<const char*>(m.cuerpo),
                                                int(m.longitud))});
    }
};

std::string placa_grande() {
    std::string s = "<placa>\n";
    for (int i = 0; s.size() < 300000; ++i)
        s += "  <componente tipo=\"Led\" id=\"LD" + std::to_string(i) + "\"/>\n";
    return s + "</placa>\n";
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    Conexion cx;
    std::vector<Recibido> rec;
    int conexiones = 0, desconexiones = 0;
    QString motivo = "sin desconexion";
    QObject::connect(&cx, &Conexion::conectado, [&] { ++conexiones; });
    QObject::connect(&cx, &Conexion::mensaje,
                     [&](quint16 t, const QByteArray& c) { rec.push_back({t, c}); });
    QObject::connect(&cx, &Conexion::desconectado, [&](const QString& m) {
        ++desconexiones;
        motivo = m;
    });

    std::printf("G1 Escuchar y aceptar\n");
    comprueba(cx.escucha(QHostAddress::LocalHost, 0) && cx.puerto() != 0,
              "escucha en 127.0.0.1 con el puerto 0, y se sabe cual le dieron");

    Modelo m;
    comprueba(m.conecta(cx.puerto()) && espera([&] { return conexiones == 1; }) &&
              cx.conectada(),
              "un modelo se conecta y la Conexion lo dice");

    std::printf("G2 Del modelo a la pantalla\n");
    {
        const std::string placa = placa_grande();
        const std::string aviso = bytes(CabAviso{N_ERROR, 3, 99}) + "abc" + "texto";
        m.manda(T_HOLA, "protocolo_max=1\n");
        m.manda(T_PLACA, placa);
        m.manda(T_LISTO, "");
        m.manda(T_AVISO, aviso);
        m.manda(T_FIN, bytes(Fin{M_VENTANA, 0, 10}));
        comprueba(espera([&] { return rec.size() == 5; }),
                  "cinco mensajes, uno de ellos de 300 kB, llegan como cinco senales");
        comprueba(rec.size() == 5 && rec[0].tipo == T_HOLA && rec[1].tipo == T_PLACA &&
                  rec[1].cuerpo == QByteArray::fromStdString(placa) &&
                  rec[2].tipo == T_LISTO && rec[2].cuerpo.isEmpty() &&
                  rec[3].cuerpo == QByteArray::fromStdString(aviso) && rec[4].tipo == T_FIN,
                  "en orden, con sus cuerpos intactos y el vacio vacio");
        rec.clear();
    }
    {
        // Partido: media cabecera, y el resto despues de que Qt la haya leido
        std::string d;
        m.e.mensaje(d, T_CATALOGO, std::string(1000, 'c'));
        m.escribe(d.substr(0, 9));
        espera([] { return false; }, 100);
        const bool nada_aun = rec.empty() && cx.conectada();
        m.escribe(d.substr(9));
        comprueba(nada_aun && espera([&] { return rec.size() == 1; }) &&
                  rec[0].cuerpo == QByteArray(1000, 'c'),
                  "un mensaje partido a mitad de la cabecera espera y luego llega entero");
        rec.clear();
    }
    {
        std::string d;
        m.e.vacio(d, T_PONG);
        m.e.mensaje(d, 0x0055, "algo que esta version no conoce");
        m.e.mensaje(d, T_ORDEN_HECHA, bytes(OrdenHecha{5, 1, 0, 1.f, RES_OK, 0}));
        m.escribe(d);
        comprueba(espera([&] { return rec.size() == 2; }) && rec[0].tipo == T_PONG &&
                  rec[1].tipo == T_ORDEN_HECHA && cx.desconocidos() == 1,
                  "tres mensajes de un golpe, uno desconocido en medio: se salta y se cuenta");
        rec.clear();
    }

    std::printf("G3 De la pantalla al modelo\n");
    {
        const std::string sus = bytes(CabSuscribe{1000000u, 0u, 1u, 0u}) + bytes(uint16_t(7));
        comprueba(cx.envia(T_VERSION, "protocolo=1\n") &&
                  cx.envia(T_SUSCRIBE, QByteArray::fromStdString(sus)) &&
                  cx.envia_pod(T_ARRANCA, Arranca{RIT_LIBRE, 1.f, 0}) &&
                  cx.envia(T_PAUSA) &&
                  cx.envia_pod(T_ORDENES, Orden{1000, 3, 0, 1.f}),
                  "la Conexion manda cinco mensajes");
        comprueba(espera([&] { m.lee(); return m.leido.size() == 5; }) &&
                  m.leido[0].tipo == T_VERSION &&
                  m.leido[1].cuerpo == QByteArray::fromStdString(sus) &&
                  m.leido[2].tipo == T_ARRANCA && m.leido[3].cuerpo.isEmpty() &&
                  m.leido[4].tipo == T_ORDENES && m.l.saltos_de_secuencia() == 0,
                  "y el modelo los lee con el mismo proto_io.h, en orden y sin huecos");
    }

    std::printf("G4 Un segundo modelo, y los cierres\n");
    {
        Modelo otro;
        otro.conecta(cx.puerto());
        comprueba(espera([&] { return cx.rechazadas() == 1; }) &&
                  espera([&] { return otro.s.state() == QAbstractSocket::UnconnectedState; }) &&
                  cx.conectada() && conexiones == 1,
                  "un segundo modelo se rechaza en el acto, y el primero sigue");
    }
    m.s.disconnectFromHost();
    comprueba(espera([&] { return desconexiones == 1; }) && motivo.isEmpty() &&
              !cx.conectada(),
              "el modelo cierra: desconectado() sin motivo, que es un cierre normal");

    {
        Modelo nav;
        comprueba(nav.conecta(cx.puerto()) && espera([&] { return conexiones == 2; }),
                  "despues se puede volver a conectar");
        nav.escribe("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
        comprueba(espera([&] { return desconexiones == 2; }) && motivo.contains("magia") &&
                  espera([&] { return nav.s.state() == QAbstractSocket::UnconnectedState; }),
                  "un navegador en el puerto: se cierra, y el motivo dice 'magia'");
    }
    {
        Modelo raro;
        raro.conecta(cx.puerto());
        espera([&] { return conexiones == 3; });
        raro.escribe(bytes(Cabecera{MAGIA, 1, T_PLACA, 0xFFFFFFFFu, 0}));
        comprueba(espera([&] { return desconexiones == 3; }) && motivo.contains("techo"),
                  "una longitud de 4 GB: se cierra con la cabecera sola");
    }
    {
        Modelo dos_gui;
        dos_gui.conecta(cx.puerto());
        espera([&] { return conexiones == 4; });
        dos_gui.manda(T_VERSION, "protocolo=1\n");
        comprueba(espera([&] { return desconexiones == 4; }) && motivo.contains("pantallas"),
                  "un tipo de pantalla llegando a la pantalla: se cierra, y se dice");
    }
    {
        Modelo nuevo;
        nuevo.conecta(cx.puerto());
        espera([&] { return conexiones == 5; });
        nuevo.manda(T_LISTO, "");
        comprueba(espera([&] { return rec.size() == 1; }) && rec[0].tipo == T_LISTO,
                  "y cada conexion empieza de cero: lector limpio, secuencia desde 0");
        cx.cierra();
        comprueba(espera([&] { return nuevo.s.state() == QAbstractSocket::UnconnectedState; }) &&
                  !cx.conectada() && !cx.envia(T_PING),
                  "cierra() desde aqui: el modelo lo ve, y ya no se puede mandar");
    }

    std::printf("RESULTADO %u ok, %u fallos\n", g_ok, g_mal);
    return g_mal ? 1 : 0;
}
