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
// Lo de las fases 4 a 6 —instantáneas, avisos, estado, ecos de órdenes— se
// recibe y se ignora todavía.
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

    // T_ARRANCA, con ritmo libre y ventana indefinida: el contenido lo usa la
    // fase 6. false si el modelo no está esperando.
    bool arranca();
    // T_PARA: antes de arrancar, termina sin simular; en marcha, es la fase 6.
    bool para();

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

private:
    void llega(quint16 tipo, const QByteArray& cuerpo);
    void cambia(Estado e) { estado_ = e; }

    Conexion                cx_;
    Estado                  estado_ = Estado::Escuchando;
    quint16                 version_ = 0;
    QHash<QString, QString> hola_;
    QByteArray              placa_xml_;
    PlacaGui                placa_;
};

} // namespace mcusim

#endif // MCU_SIM_GUI_SESION_H
