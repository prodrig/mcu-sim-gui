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
//       manda un catálogo que no se puede leer;
//   G4  (fase 4) los avisos de placa durante el saludo, T_SUSCRIBE, y las
//       instantáneas, los avisos y los estados en marcha;
//   G5  (fase 5) las órdenes: `ordena()` -> T_ORDENES, antes de arrancar y en
//       marcha, y T_ORDEN_HECHA -> `orden_hecha()`.
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

    // -------------------------------------------------------------------------
    std::printf("G4 La fase 4: avisos, suscripcion e instantaneas\n");
    {
        Sesion s;
        comprueba(!s.suscribe(1000000, {1}), "sin un modelo saludado, suscribe() no manda nada");
        s.escucha(QHostAddress::LocalHost, 0);
        struct Av { quint32 n; quint64 t; QString o, x; };
        std::vector<Av> avs;
        std::vector<std::pair<quint64, quint32>> insts;
        std::vector<std::pair<quint16, float>> muestras;
        quint64 t_est = 0; double pared = 0; quint64 deltas = 0; quint32 fase = 9;
        QObject::connect(&s, &Sesion::aviso, [&](quint32 n, quint64 t, const QString& o,
                                                 const QString& x) { avs.push_back({n, t, o, x}); });
        QObject::connect(&s, &Sesion::instantanea,
                         [&](quint64 t, quint32 p) { insts.push_back({t, p}); });
        QObject::connect(&s, &Sesion::muestra,
                         [&](quint16 i, float v) { muestras.push_back({i, v}); });
        QObject::connect(&s, &Sesion::estado_modelo, [&](quint32 f, quint64 t, double p, quint64 d) {
            fase = f; t_est = t; pared = p; deltas = d;
        });
        ModeloFalso m;
        m.conecta(s.puerto());
        m.manda(T_HOLA, "protocolo_max=1\n");
        m.espera_leidos(1);
        m.version(1);
        m.manda(T_PLACA, PLACA_XML);
        m.manda(T_CATALOGO, CATALOGO_XML);
        m.manda(T_AVISO, bytes(CabAviso{N_AVISO, 5, 0}) + "placa" +
                         "nodo suelto: conducen a la vez RA.a y RB.a");
        m.manda(T_LISTO);
        comprueba(espera([&] { return s.estado() == Sesion::Estado::Lista; }) &&
                  avs.size() == 1 && avs[0].n == N_AVISO && avs[0].o == "placa" &&
                  avs[0].x.contains("conducen a la vez"),
                  "un aviso de placa entre T_CATALOGO y T_LISTO llega como aviso(), antes "
                  "de arrancar");
        comprueba(s.suscribe(16666667ull, {1, 3}), "suscribe() con el modelo esperando");
        CabSuscribe c{};
        comprueba(m.espera_leidos(2) && m.leido[1].tipo == T_SUSCRIBE &&
                  m.leido[1].cuerpo.size() == int(sizeof c + 4) &&
                  (std::memcpy(&c, m.leido[1].cuerpo.constData(), sizeof c), true) &&
                  c.periodo_ns_lo == 16666667u && c.periodo_ns_hi == 0 && c.n == 2,
                  "y el modelo recibe un T_SUSCRIBE con el periodo y los dos ids");
        s.arranca();
        comprueba(!s.para(), "en marcha, para() no manda nada: es la fase 6");
        std::string in = bytes(CabInstantanea{7000000ull, 2, 4});
        in += bytes(Muestra{1, 0, 1.f}) + bytes(Muestra{3, 0, 0.f});
        m.manda(T_INSTANTANEA, in);
        m.manda(T_ESTADO, bytes(Estado{F_CORRIENDO, 0, 7000000ull, 0.5, 99ull}));
        m.manda(T_AVISO, bytes(CabAviso{N_ERROR, 6, 7000000ull}) + "/stm32" + "algo");
        comprueba(espera([&] { return avs.size() == 2; }) && insts.size() == 1 &&
                  insts[0].first == 7000000ull && insts[0].second == 4 && s.perdidas() == 4,
                  "una instantanea: su instante y sus perdidas, que se acumulan");
        comprueba(muestras.size() == 2 && muestras[0].first == 1 && muestras[0].second == 1.f &&
                  muestras[1].first == 3 && muestras[1].second == 0.f,
                  "y una muestra() por observable, con su id y su valor");
        comprueba(fase == F_CORRIENDO && t_est == 7000000ull && pared == 0.5 && deltas == 99,
                  "T_ESTADO: la fase, los dos relojes y los deltas");
        comprueba(avs[1].n == N_ERROR && avs[1].t == 7000000ull && avs[1].o == "/stm32" &&
                  avs[1].x == "algo",
                  "y un aviso en marcha, con su nivel, su instante, su origen y su texto");
    }

    // -------------------------------------------------------------------------
    std::printf("G5 La fase 5: ordenes y sus ecos\n");
    {
        Sesion s;
        comprueba(!s.ordena(2, 0, 1.f), "sin un modelo saludado, ordena() no manda nada");
        s.escucha(QHostAddress::LocalHost, 0);
        struct Eco { quint64 t; quint16 p, m; float v; quint32 r; };
        std::vector<Eco> ecos;
        QStringList problemas;
        QObject::connect(&s, &Sesion::orden_hecha,
                         [&](quint64 t, quint16 p, quint16 md, float v, quint32 r) {
                             ecos.push_back({t, p, md, v, r});
                         });
        QObject::connect(&s, &Sesion::problema, [&](const QString& t) { problemas << t; });
        ModeloFalso m;
        m.conecta(s.puerto());
        comprueba(saluda_hasta_listo(m) &&
                      espera([&] { return s.estado() == Sesion::Estado::Lista; }),
                  "saludo hasta T_LISTO");
        // El ejemplo de doc/protocolo.md §5, antes de arrancar
        const QVector<Orden> ej{{1000000000ull, 2, 0, 1.f}, {500000000ull, 2, 0, 0.f},
                                {2500000000ull, 2, 0, 1.f}, {220000000ull, 2, 0, 0.f}};
        comprueba(s.ordena(ej) && !s.ordena(QVector<Orden>{}),
                  "con el modelo esperando, ordena() manda; sin ninguna orden, no");
        comprueba(m.espera_leidos(2) && m.leido[1].tipo == T_ORDENES &&
                  m.leido[1].cuerpo == QByteArray::fromStdString(
                      bytes(ej[0]) + bytes(ej[1]) + bytes(ej[2]) + bytes(ej[3])),
                  "el modelo recibe UN T_ORDENES con las cuatro, tal cual: 64 bytes");
        s.arranca();
        comprueba(s.ordena(2, 0, 1.f) && m.espera_leidos(4) && m.leido[3].tipo == T_ORDENES &&
                  m.leido[3].cuerpo == QByteArray::fromStdString(bytes(Orden{0, 2, 0, 1.f})),
                  "en marcha, ordena(pieza, mando, valor) es una sola orden con delta 0: "
                  "\"ahora\"");
        m.manda(T_ORDEN_HECHA, bytes(OrdenHecha{1000000000ull, 2, 0, 1.f, RES_OK, 0}));
        m.manda(T_ORDEN_HECHA, bytes(OrdenHecha{1500000000ull, 7, 0, 0.f, RES_PIEZA, 0}));
        comprueba(espera([&] { return ecos.size() == 2; }) && s.ecos() == 2 &&
                  ecos[0].t == 1000000000ull && ecos[0].p == 2 && ecos[0].m == 0 &&
                  ecos[0].v == 1.f && ecos[0].r == RES_OK &&
                  ecos[1].p == 7 && ecos[1].r == RES_PIEZA,
                  "cada T_ORDEN_HECHA es un orden_hecha(), con el instante real, la pieza, "
                  "el mando, el valor aplicado y el resultado");
        m.manda(T_ORDEN_HECHA, "corto");
        comprueba(espera([&] { return !problemas.isEmpty(); }) && ecos.size() == 2 &&
                  problemas.last().contains("eco de orden"),
                  "un eco que no mide lo que debe no se inventa: se dice como problema");
        m.manda(T_FIN, bytes(Fin{M_VENTANA, 0, 5000110000ull}));
        comprueba(espera([&] { return s.estado() == Sesion::Estado::Terminada; }) &&
                  !s.ordena(2, 0, 0.f),
                  "y con el modelo terminado, ordena() ya no manda nada");
    }

    return resultado();
}
