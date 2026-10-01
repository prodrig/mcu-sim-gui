// =============================================================================
// sesion.h — el saludo del lado de la ventana (fase 3 del plan)
//
// Encima de `Conexion`, que mueve bytes, esto sabe qué significan los mensajes
// del saludo (`doc/protocolo.md` §3):
//
//   T_HOLA      -> se contesta T_VERSION con la versión elegida: la más alta
//                  que conocen los dos. Si no hay ninguna, `protocolo=0` y se
//                  cierra, que es lo que manda el protocolo;
//   T_PLACA     -> se guarda;
//   T_CATALOGO  -> se lee y se junta con la placa: `placa()` y la señal
//                  `placa_lista()`. La ventana se construye con eso;
//   T_LISTO     -> el modelo está construido y ESPERANDO. `arranca()` manda
//                  T_ARRANCA; `para()`, T_PARA;
//   T_FIN       -> se acabó, con motivo, código e instante.
//
// Y desde la fase 4, el sentido modelo -> pantalla en marcha:
//
//   T_INSTANTANEA -> `instantanea()` con el instante y las perdidas, y una
//                    `muestra()` por observable;
//   T_AVISO       -> `aviso()`. Los de la placa llegan durante el saludo, entre
//                    T_CATALOGO y T_LISTO; los del modelo, en marcha;
//   T_ESTADO      -> `estado_modelo()`, con los dos relojes;
//   `suscribe()`  -> T_SUSCRIBE.
//
// Y desde la fase 5, las órdenes (`doc/protocolo.md` §5):
//
//   `ordena()`    -> T_ORDENES. La primera orden es un instante ABSOLUTO si se
//                    manda antes de arrancar, y relativa al instante en que el
//                    modelo la lee si ya corre; las demás, deltas;
//   T_ORDEN_HECHA -> `orden_hecha()`, con el instante REAL en que se aplicó y
//                    el resultado. Uno por orden: ninguna se calla.
//
// Y desde la fase 6, el control (`doc/protocolo.md` §4.2):
//
//   `arranca(ritmo, factor, ventana)` -> T_ARRANCA con su contenido;
//   `pausa()`, `sigue()`, `paso(ns)` y `para()` -> T_PAUSA, T_SIGUE, T_PASO y
//                    T_PARA. `para()` vale antes de arrancar y en marcha;
//   `pausada()`   -> lo que dijo el último T_ESTADO. La pausa la decide el
//                    modelo: hasta que no lo dice, no está en pausa.
// =============================================================================
#ifndef MCU_SIM_GUI_SESION_H
#define MCU_SIM_GUI_SESION_H

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

#include "conexion.h"
#include "placa.h"

namespace mcusim {

class Sesion : public QObject {
    Q_OBJECT
public:
    enum class Estado {
        Escuchando,   // sin modelo conectado
        Saludando,    // conectado, entre T_HOLA y T_LISTO
        Lista,        // T_LISTO recibido: esperando a que alguien diga arranca
        Corriendo,    // T_ARRANCA mandado
        Terminada     // T_FIN recibido, o el modelo se fue
    };

    explicit Sesion(QObject* padre = nullptr);

    bool    escucha(const QHostAddress& dir, quint16 puerto);
    quint16 puerto() const { return cx_.puerto(); }
    QString error() const { return cx_.error(); }

    Estado          estado() const { return estado_; }
    quint16         version() const { return version_; }
    const QHash<QString, QString>& hola() const { return hola_; }
    const PlacaGui& placa() const { return placa_; }

    // T_ARRANCA. El ritmo (proto::RIT_REAL con su factor, RIT_LIBRE o
    // RIT_DEMANDA, que arranca en pausa) y la ventana de tiempo: 0 es la de
    // mcu-sim, la de su línea de órdenes o sin fin. false si el modelo no
    // está esperando.
    bool arranca(quint32 ritmo = proto::RIT_REAL, float factor = 1.f,
                 quint64 ventana_ns = 0);
    // T_PARA: antes de arrancar, termina sin simular; en marcha, `sc_stop()`,
    // y el T_FIN dice M_PARA. false si no hay un modelo esperando o corriendo.
    bool para();
    // T_PAUSA y T_SIGUE: solo en marcha. Con ritmo a demanda T_SIGUE no vale.
    bool pausa();
    bool sigue();
    // T_PASO: solo en marcha y con ritmo a demanda; avanza `ns` simulados y
    // vuelve a la pausa.
    bool paso(quint64 ns);
    quint32 ritmo() const { return ritmo_; }
    bool    pausada() const { return fase_ == proto::F_PAUSADA; }
    // T_SUSCRIBE: qué observables, y cada cuánto tiempo SIMULADO. Reemplaza a
    // la anterior; vacía, las apaga. false si no hay un modelo saludado.
    bool suscribe(quint64 periodo_ns, const QVector<quint16>& ids);
    // T_ORDENES. Antes de arrancar (Lista) o en marcha (Corriendo); false si
    // no, o si no hay ninguna. La comprobación de pieza, mando y rango la
    // hace el modelo, y lo dice en el eco: esta ventana no la duplica.
    bool ordena(const QVector<proto::Orden>& ordenes);
    // Una sola, «ahora»: en t = 0 si aún no ha arrancado, en el instante en
    // que el modelo la lea si ya corre.
    bool ordena(quint16 pieza, quint16 mando, float valor);
    quint64 ecos() const { return ecos_; }

    // Instantáneas que el modelo dice haber tirado porque esto no leía.
    quint64 perdidas() const { return perdidas_; }

    // La versión que elige la ventana ante un `protocolo_max`: la más alta que
    // conocen los dos, o 0 si no hay ninguna. Pública porque se prueba sola.
    static quint16 elige_version(long protocolo_max);
    // Las líneas clave=valor de T_HOLA.
    static QHash<QString, QString> claves(const QByteArray& texto);

signals:
    void conectado();
    void hola_recibido();
    void placa_lista();                     // placa y catálogo leídos y juntos
    void listo();                           // T_LISTO
    void fin(quint32 motivo, qint32 codigo, quint64 t_sim_ns);
    void desconectado(const QString& motivo);
    void problema(const QString& texto);   // algo que la persona tiene que saber
    // Fase 4
    void instantanea(quint64 t_sim_ns, quint32 perdidas);
    void muestra(quint16 id_obs, float valor);
    void aviso(quint32 nivel, quint64 t_sim_ns, const QString& origen, const QString& texto);
    void estado_modelo(quint32 fase, quint64 t_sim_ns, double t_pared_s, quint64 deltas);
    // Fase 5
    void orden_hecha(quint64 t_sim_ns, quint16 pieza, quint16 mando, float valor,
                     quint32 resultado);

private:
    void llega(quint16 tipo, const QByteArray& cuerpo);
    void cambia(Estado e) { estado_ = e; }

    Conexion                cx_;
    Estado                  estado_ = Estado::Escuchando;
    quint16                 version_ = 0;
    QHash<QString, QString> hola_;
    QByteArray              placa_xml_;
    PlacaGui                placa_;
    quint64                 perdidas_ = 0;
    quint64                 ecos_ = 0;
    quint32                 ritmo_ = proto::RIT_REAL;
    quint32                 fase_ = proto::F_ESPERANDO;
};

} // namespace mcusim

#endif // MCU_SIM_GUI_SESION_H
