// =============================================================================
// prueba_sesion.cpp — el saludo del lado de la ventana, y los dos XML
//
// Fase 3 del plan. Sin ventana: QtCore y QtNetwork. Tres partes:
//
//   G1  `lee_catalogo` y `junta_placa`: los dos XML del saludo, juntos por el
//       id de cada pieza, y lo que pasa cuando no casan;
//   G2  la `Sesion` contra un modelo falso: T_HOLA -> T_VERSION, la placa, el
//       catálogo, T_LISTO, arranca() -> T_ARRANCA, y T_FIN;
//   G3  un modelo que no habla ninguna versión que esto conozca, y uno que
//       manda un catálogo que no se puede leer.
// =============================================================================
#include <QCoreApplication>

#include <cstring>

#include "comun.h"
#include "placa.h"
#include "sesion.h"

using namespace mcusim;
using namespace mcusim::proto;
using namespace prueba;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // -------------------------------------------------------------------------
    std::printf("G1 Los dos XML del saludo, juntos\n");
    {
        QVector<PiezaGui> cat;
        QString e;
        comprueba(lee_catalogo(CATALOGO_XML, cat, e) && cat.size() == 4,
                  "el catalogo de doc/protocolo.md se lee: cuatro piezas");
        comprueba(cat.size() == 4 && cat[1].id == "LD4" && cat[1].tipo == "Led" &&
                  cat[1].observables.size() == 2 && cat[1].observables[1].unidad == "mA" &&
                  cat[1].observables[1].max == 25 && !cat[1].observables[1].interesante &&
                  cat[2].mandos.size() == 1 && cat[2].mandos[0].tipo == "boton" &&
                  cat[3].observables.isEmpty() && cat[3].mandos.isEmpty(),
                  "con sus observables, sus unidades, sus escalas y sus mandos");

        PlacaGui p;
        comprueba(junta_placa(PLACA_XML, cat, p, e) && p.nombre == "discovery" &&
                  p.mcus.isEmpty(),
                  "la placa se junta con el catalogo, con su nombre; sin <mcu>, sin MCU, "
                  "como la de la Discovery (lo pone sim, y lo dice T_HOLA)");
        PlacaGui p2;
        comprueba(junta_placa("<placa nombre=\"dos\"><mcu tipo=\"STM32F407VG\" id=\"u0\"/>"
                              "<mcu tipo=\"STM32F446RE\" id=\"u1\"/></placa>", cat, p2, e) &&
                  p2.mcus == QStringList({"u0 (STM32F407VG)", "u1 (STM32F446RE)"}),
                  "y una que declara dos MCUs los trae con su id y su tipo");
        comprueba(p.piezas.size() == 4 && p.piezas[1].en_placa &&
                  p.piezas[1].patillas.size() == 1 &&
                  p.piezas[1].patillas[0].nombre == "anodo" &&
                  p.piezas[1].patillas[0].nodo == "PD12",
                  "cada pieza del catalogo recibe sus patillas de la placa, por su id");
        comprueba(!p.piezas[0].conectada && p.piezas[1].conectada,
                  "y sabe si esta desoldada");
        comprueba(p.n_observables() == 4 && p.n_interesantes() == 3 && p.n_mandos() == 1 &&
                  p.avisos.isEmpty(),
                  "cuatro observables, tres que pintar, un mando, y nada que no case");

        PlacaGui q;
        const QByteArray ajena =
            "<placa nombre=\"x\"><componente tipo=\"Servo\" id=\"S1\"/></placa>";
        comprueba(junta_placa(ajena, cat, q, e) && q.avisos.size() == 1 &&
                  q.avisos[0].contains("S1") && !q.piezas[0].en_placa,
                  "un componente que el catalogo no trae se avisa, no se inventa");

        QVector<PiezaGui> malo;
        comprueba(!lee_catalogo("<catalogo><pieza idx=\"1\" id=\"a\" tipo=\"b\"/></catalogo>",
                                malo, e) && e.contains("idx"),
                  "un idx que no es su posicion se rechaza: es el `pieza` de una Orden");
        comprueba(!lee_catalogo("esto no es xml <", malo, e) && !e.isEmpty(),
                  "y algo que no es XML, tambien");
        comprueba(!lee_catalogo("<placa/>", malo, e) && e.contains("catalogo"),
                  "y un XML que no es un catalogo");
    }

    // -------------------------------------------------------------------------
    std::printf("G2 La Sesion contra un modelo falso\n");
    {
        comprueba(Sesion::elige_version(1) == 1 && Sesion::elige_version(5) == 1 &&
                  Sesion::elige_version(0) == 0 && Sesion::elige_version(-3) == 0,
                  "la version elegida es la mas alta que conocen los dos, o 0");

        Sesion s;
        int placas = 0, listos = 0, fines = 0;
        quint32 motivo = 99; quint64 t_fin = 0;
        QObject::connect(&s, &Sesion::placa_lista, [&] { ++placas; });
        QObject::connect(&s, &Sesion::listo, [&] { ++listos; });
        QObject::connect(&s, &Sesion::fin, [&](quint32 m, qint32, quint64 t) {
            ++fines; motivo = m; t_fin = t;
        });
        comprueba(s.escucha(QHostAddress::LocalHost, 0) && s.puerto() != 0 &&
                  s.estado() == Sesion::Estado::Escuchando,
                  "escucha, y esta en Escuchando");

        ModeloFalso m;
        m.conecta(s.puerto());
        m.manda(T_HOLA, "protocolo_max=3\nmcu_sim=prueba\nplaca=placas/discovery.xml\n");
        comprueba(m.espera_leidos(1) && m.leido[0].tipo == T_VERSION &&
                  m.leido[0].cuerpo.startsWith("protocolo=1\n"),
                  "a un modelo que ofrece hasta la 3, contesta T_VERSION protocolo=1");
        comprueba(s.version() == 1 && s.hola().value("placa") == "placas/discovery.xml",
                  "y se queda con la version y con lo que dijo T_HOLA");
        m.version(1);
        m.manda(T_PLACA, PLACA_XML);
        m.manda(T_CATALOGO, CATALOGO_XML);
        comprueba(espera([&] { return placas == 1; }) && s.placa().piezas.size() == 4 &&
                  s.placa().piezas[2].patillas[0].nodo == "PA0",
                  "con la placa y el catalogo, placa_lista() y la placa junta");
        comprueba(!s.arranca(), "antes de T_LISTO, arranca() no manda nada");
        m.manda(T_LISTO);
        comprueba(espera([&] { return listos == 1; }) && s.estado() == Sesion::Estado::Lista,
                  "T_LISTO: listo(), y esperando");
        comprueba(s.arranca() && s.estado() == Sesion::Estado::Corriendo,
                  "arranca() manda T_ARRANCA");
        Arranca a{};
        comprueba(m.espera_leidos(2) && m.leido[1].tipo == T_ARRANCA &&
                  m.leido[1].cuerpo.size() == int(sizeof a) &&
                  (std::memcpy(&a, m.leido[1].cuerpo.constData(), sizeof a), true) &&
                  a.ritmo == RIT_LIBRE && a.ventana_ns == 0,
                  "y el modelo recibe un T_ARRANCA de 16 bytes, ritmo libre y sin ventana");
        comprueba(!s.arranca(), "y no se arranca dos veces");
        m.manda(T_INSTANTANEA, bytes(CabInstantanea{1, 0, 0}));
        m.manda(T_FIN, bytes(Fin{M_VENTANA, 0, 50110000ull}));
        comprueba(espera([&] { return fines == 1; }) && motivo == M_VENTANA &&
                  t_fin == 50110000ull && s.estado() == Sesion::Estado::Terminada,
                  "una instantanea se ignora (fase 4), y T_FIN da su motivo y su instante");
    }

    // -------------------------------------------------------------------------
    std::printf("G3 Lo que no puede ser\n");
    {
        Sesion s;
        s.escucha(QHostAddress::LocalHost, 0);
        int problemas = 0;
        QObject::connect(&s, &Sesion::problema, [&](const QString&) { ++problemas; });
        ModeloFalso m;
        m.conecta(s.puerto());
        m.manda(T_HOLA, "protocolo_max=0\n");
        comprueba(m.espera_leidos(1) && m.leido[0].cuerpo.startsWith("protocolo=0\n") &&
                  espera([&] { return m.s.state() == QAbstractSocket::UnconnectedState; }) &&
                  problemas == 1,
                  "un modelo sin ninguna version comun: protocolo=0, se cierra y se dice");
    }
    {
        Sesion s;
        s.escucha(QHostAddress::LocalHost, 0);
        QString dicho;
        QObject::connect(&s, &Sesion::problema, [&](const QString& t) { dicho = t; });
        ModeloFalso m;
        m.conecta(s.puerto());
        m.manda(T_HOLA, "protocolo_max=1\n");
        m.espera_leidos(1);
        m.version(1);
        m.manda(T_PLACA, PLACA_XML);
        m.manda(T_CATALOGO, "<catalogo><pieza idx=\"7\" id=\"a\" tipo=\"b\"/></catalogo>");
        comprueba(espera([&] { return m.s.state() == QAbstractSocket::UnconnectedState; }) &&
                  dicho.contains("idx"),
                  "un catalogo que no se puede leer: se cierra, y se dice por que");
    }

    return resultado();
}
