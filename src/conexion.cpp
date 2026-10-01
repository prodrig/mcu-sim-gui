#include "conexion.h"

#include <QTcpServer>
#include <QTcpSocket>

namespace mcusim {

Conexion::Conexion(QObject* padre)
    : QObject(padre), srv_(new QTcpServer(this))
{
    connect(srv_, &QTcpServer::newConnection, this, &Conexion::nueva);
}

Conexion::~Conexion() = default;

bool Conexion::escucha(const QHostAddress& dir, quint16 puerto)
{
    if (!srv_->listen(dir, puerto)) {
        error_ = srv_->errorString();
        return false;
    }
    error_.clear();
    return true;
}

quint16 Conexion::puerto() const { return srv_->serverPort(); }

void Conexion::nueva()
{
    while (QTcpSocket* s = srv_->nextPendingConnection()) {
        if (sock_) {                       // ya hay un modelo: este sobra
            ++rechazadas_;
            s->abort();
            s->deleteLater();
            continue;
        }
        sock_ = s;
        sock_->setSocketOption(QAbstractSocket::LowDelayOption, 1);   // sin Nagle
        // Cada conexión empieza de cero: lector, emisor y versión.
        lector_ = proto::Lector(proto::Origen::Modelo);
        emisor_ = proto::Emisor();
        connect(sock_, &QTcpSocket::readyRead, this, &Conexion::hay_datos);
        connect(sock_, &QTcpSocket::disconnected, this, &Conexion::se_fue);
        emit conectado();
        hay_datos();                       // por si ya traía algo
    }
}

void Conexion::hay_datos()
{
    if (!sock_) return;
    const QByteArray d = sock_->readAll();
    lector_.mete(d.constData(), std::size_t(d.size()));
    proto::Mensaje m;
    for (;;) {
        const proto::Lector::Estado e = lector_.saca(m);
        if (e == proto::Lector::LEC_FALTA) break;
        if (e == proto::Lector::LEC_ERROR) {
            suelta(QString::fromStdString(lector_.error()));
            return;
        }
        if (!proto::es_conocido(m.tipo)) { ++desconocidos_; continue; }
        // Se copia: el puntero del lector vale hasta la siguiente mete(), y
        // quien recibe la señal puede guardarlo.
        emit mensaje(m.tipo, QByteArray(reinterpret_cast<const char*>(m.cuerpo),
                                        int(m.longitud)));
        if (!sock_) return;                // alguien cerró desde la señal
    }
}

void Conexion::se_fue() { suelta(QString()); }

void Conexion::suelta(const QString& motivo)
{
    if (!sock_) return;
    QTcpSocket* s = sock_;
    sock_ = nullptr;                       // antes de cerrar: se_fue() no repite
    s->disconnect(this);
    s->abort();
    s->deleteLater();
    emit desconectado(motivo);
}

void Conexion::cierra() { suelta(QString()); }

bool Conexion::envia(quint16 tipo, const QByteArray& cuerpo)
{
    if (!sock_) return false;
    sal_.clear();                          // conserva la capacidad: sin reservar por mensaje
    if (!emisor_.mensaje(sal_, tipo, cuerpo.constData(), std::size_t(cuerpo.size())))
        return false;
    return sock_->write(sal_.data(), qint64(sal_.size())) == qint64(sal_.size());
}

void Conexion::fija_version(quint16 v)
{
    lector_.fija_version(v);
    emisor_.fija_version(v);
}

} // namespace mcusim
