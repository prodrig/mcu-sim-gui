// =============================================================================
// conexion.h — el extremo de la GUI del transporte: escuchar, aceptar a UN
// modelo, y convertir bytes en mensajes y mensajes en bytes
//
// Fase 2 del plan (`doc/plan_dos_procesos.md`). Es el mismo par que en
// `mcu-sim` hacen `common/red.h` y `common/proto_io.h`, pero sobre
// `QTcpServer`/`QTcpSocket`, porque aquí el bucle de eventos es el de Qt y un
// socket que bloquea congelaría la ventana. El marco NO se vuelve a escribir:
// se lee y se escribe con el mismo `proto_io.h`, que es una copia idéntica
// byte a byte de la de `mcu-sim` (lo comprueba `make gui-proto` allí).
//
// Lo que hace, y es todo lo que hace —esta fase mueve bytes—:
//
//   * escucha en `host:puerto`. La GUI escucha y el modelo se conecta
//     (`doc/protocolo.md` §1), porque es la GUI la que lanza el modelo y así
//     ya está escuchando cuando el otro arranca;
//   * acepta UNA conexión. Mientras hay una, cualquier otra se cierra en el
//     acto y se cuenta: dos modelos hablando con la misma ventana no es un
//     caso que el protocolo contemple;
//   * entrega cada mensaje completo con `mensaje()`. Los tipos que esta
//     versión no conoce se SALTAN y se cuentan, que es lo que permite añadir
//     mensajes sin subir la versión;
//   * si el otro extremo manda algo que no es el protocolo —magia mala,
//     versión imposible, una longitud que pasa del techo, un tipo del sentido
//     equivocado— cierra la conexión y dice por qué con `desconectado()`. No
//     intenta resincronizar.
//
// Lo que no hace: saber lo que significa un mensaje. Eso es de la fase 3 en
// adelante, y vivirá encima de esto.
// =============================================================================
#ifndef MCU_SIM_GUI_CONEXION_H
#define MCU_SIM_GUI_CONEXION_H

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QString>

#include <string>

#include "proto_io.h"

class QTcpServer;
class QTcpSocket;

namespace mcusim {

class Conexion : public QObject {
    Q_OBJECT
public:
    explicit Conexion(QObject* padre = nullptr);
    ~Conexion() override;

    // Empieza a escuchar. Con el puerto 0 el sistema elige uno, y `puerto()`
    // dice cuál. false si no se puede, y `error()` explica por qué.
    bool    escucha(const QHostAddress& dir, quint16 puerto);
    quint16 puerto() const;
    QString error() const { return error_; }

    bool conectada() const { return sock_ != nullptr; }

    // Manda un mensaje. false si no hay conexión o el cuerpo pasa del techo.
    bool envia(quint16 tipo, const QByteArray& cuerpo = QByteArray());
    template <class T>
    bool envia_pod(quint16 tipo, const T& t) {
        return envia(tipo, QByteArray(reinterpret_cast<const char*>(&t), int(sizeof t)));
    }

    // La versión negociada en el saludo; hasta entonces, la 1.
    void fija_version(quint16 v);

    // Cierra la conexión con el modelo, si la hay, DESPUÉS de mandar lo que
    // quedara por mandar. Se sigue escuchando.
    void cierra();

    quint64 desconocidos() const { return desconocidos_; }
    quint64 rechazadas() const { return rechazadas_; }
    quint64 saltos_de_secuencia() const { return lector_.saltos_de_secuencia(); }

signals:
    void conectado();
    void mensaje(quint16 tipo, const QByteArray& cuerpo);
    // `motivo` vacío: el otro extremo cerró, o se cerró desde aquí. Si no, es
    // lo que el lector encontró mal, y la conexión ya está cerrada.
    void desconectado(const QString& motivo);

private:
    void nueva();
    void hay_datos();
    void se_fue();
    // `ordenado`: escribe lo pendiente antes de cerrar; si no, se corta en seco.
    void suelta(const QString& motivo, bool ordenado = false);

    QTcpServer*      srv_  = nullptr;
    QTcpSocket*      sock_ = nullptr;
    proto::Lector    lector_{proto::Origen::Modelo};
    proto::Emisor    emisor_;
    std::string      sal_;               // el búfer de salida, reutilizado
    QString          error_;
    quint64          desconocidos_ = 0, rechazadas_ = 0;
};

} // namespace mcusim

#endif // MCU_SIM_GUI_CONEXION_H
