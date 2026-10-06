#include "sesion.h"

#include <algorithm>
#include <cstring>

#include <QCoreApplication>
#include <QStringList>

namespace mcusim {

Sesion::Sesion(QObject* padre) : QObject(padre)
{
    connect(&cx_, &Conexion::conectado, this, [this] {
        cambia(Estado::Saludando);
        fase_ = proto::F_ESPERANDO;
        ritmo_ = proto::RIT_REAL;
        perdidas_ = 0;
        ecos_ = 0;
        version_ = 0;
        hola_.clear();
        placa_xml_.clear();
        placa_ = PlacaGui();
        ilus_.clear();
        emit conectado();
    });
    connect(&cx_, &Conexion::mensaje, this, &Sesion::llega);
    connect(&cx_, &Conexion::desconectado, this, [this](const QString& m) {
        if (estado_ != Estado::Terminada) cambia(Estado::Terminada);
        emit desconectado(m);
    });
}

bool Sesion::escucha(const QHostAddress& dir, quint16 puerto)
{
    return cx_.escucha(dir, puerto);
}

quint16 Sesion::elige_version(long protocolo_max)
{
    if (protocolo_max < 1) return 0;
    return quint16(std::min<long>(protocolo_max, proto::VERSION_PROTO));
}

// Las cabeceras hasta la primera línea en blanco, como T_HOLA, y detrás el SVG
// tal cual. `placas=` vacío es la placa suelta.
bool Sesion::lee_ilustracion(const QByteArray& cuerpo, Ilustracion& i)
{
    const int b = cuerpo.indexOf("\n\n");
    if (b < 0) return false;
    const QHash<QString, QString> c = claves(cuerpo.left(b + 1));
    i.placas = c.value(QStringLiteral("placas")).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (i.placas.isEmpty()) i.placas << QString();
    i.fichero = c.value(QStringLiteral("fichero"));
    i.svg = cuerpo.mid(b + 2);
    return true;
}

QHash<QString, QString> Sesion::claves(const QByteArray& texto)
{
    QHash<QString, QString> h;
    for (const QString& l : QString::fromUtf8(texto).split(QLatin1Char('\n'))) {
        const int i = l.indexOf(QLatin1Char('='));
        if (i > 0) h.insert(l.left(i), l.mid(i + 1).trimmed());
    }
    return h;
}

void Sesion::llega(quint16 tipo, const QByteArray& cuerpo)
{
    using namespace proto;
    switch (tipo) {
    case T_HOLA: {
        hola_ = claves(cuerpo);
        bool ok = false;
        const long max = hola_.value(QStringLiteral("protocolo_max")).toLong(&ok);
        version_ = ok ? elige_version(max) : 0;
        const QString gui = QCoreApplication::applicationVersion().isEmpty()
                                ? QStringLiteral("0.1.0")
                                : QCoreApplication::applicationVersion();
        cx_.envia(T_VERSION, QStringLiteral("protocolo=%1\ngui=%2\n")
                                 .arg(version_).arg(gui).toUtf8());
        if (version_ == 0) {
            emit problema(tr("el modelo ofrece el protocolo hasta la version '%1', y "
                             "esta ventana solo habla desde la 1 hasta la %2")
                              .arg(hola_.value(QStringLiteral("protocolo_max")))
                              .arg(VERSION_PROTO));
            cx_.cierra();
            return;
        }
        cx_.fija_version(version_);
        emit hola_recibido();
        break;
    }
    case T_PLACA:
        placa_xml_ = cuerpo;
        break;
    case T_CATALOGO: {
        QVector<PiezaGui> cat;
        QString e;
        if (!lee_catalogo(cuerpo, cat, e) || !junta_placa(placa_xml_, cat, placa_, e)) {
            emit problema(tr("no se puede leer lo que manda el modelo: %1").arg(e));
            cx_.cierra();
            return;
        }
        for (const QString& a : placa_.avisos) emit problema(a);
        emit placa_lista();
        break;
    }
    case T_ILUSTRACION: {
        Ilustracion i;
        if (!lee_ilustracion(cuerpo, i)) {
            emit problema(tr("un T_ILUSTRACION sin la linea en blanco que separa las "
                             "cabeceras del dibujo: se ignora"));
            break;
        }
        ilus_.push_back(i);
        emit ilustracion(int(ilus_.size()) - 1);
        break;
    }
    case T_LISTO:
        cambia(Estado::Lista);
        emit listo();
        break;
    case T_FIN: {
        Fin f{};
        if (cuerpo.size() == int(sizeof f)) std::memcpy(&f, cuerpo.constData(), sizeof f);
        cambia(Estado::Terminada);
        emit fin(f.motivo, f.codigo, f.t_sim_ns);
        break;
    }
    case T_INSTANTANEA: {
        CabInstantanea c{};
        if (cuerpo.size() < int(sizeof c)) break;
        std::memcpy(&c, cuerpo.constData(), sizeof c);
        if (cuerpo.size() != int(sizeof c + c.n * sizeof(Muestra))) {
            emit problema(tr("una instantanea de %1 bytes no mide lo que dice").arg(cuerpo.size()));
            break;
        }
        perdidas_ += c.perdidas;
        emit instantanea(c.t_sim_ns, c.perdidas);
        for (quint32 i = 0; i < c.n; ++i) {
            Muestra m{};
            std::memcpy(&m, cuerpo.constData() + sizeof c + i * sizeof m, sizeof m);
            emit muestra(m.id, m.valor);
        }
        break;
    }
    case T_AVISO: {
        CabAviso c{};
        if (cuerpo.size() < int(sizeof c)) break;
        std::memcpy(&c, cuerpo.constData(), sizeof c);
        const QByteArray resto = cuerpo.mid(int(sizeof c));
        emit aviso(c.nivel, c.t_sim_ns, QString::fromUtf8(resto.left(int(c.origen_len))),
                   QString::fromUtf8(resto.mid(int(c.origen_len))));
        break;
    }
    case T_ESTADO: {
        proto::Estado e{};
        if (cuerpo.size() != int(sizeof e)) break;
        std::memcpy(&e, cuerpo.constData(), sizeof e);
        fase_ = e.fase;
        emit estado_modelo(e.fase, e.t_sim_ns, e.t_pared_s, e.deltas);
        break;
    }
    case T_ORDEN_HECHA: {
        OrdenHecha h{};
        if (cuerpo.size() != int(sizeof h)) {
            emit problema(tr("un eco de orden de %1 bytes no mide lo que dice").arg(cuerpo.size()));
            break;
        }
        std::memcpy(&h, cuerpo.constData(), sizeof h);
        ++ecos_;
        emit orden_hecha(h.t_sim_ns, h.pieza, h.mando, h.valor, h.resultado);
        break;
    }
    default:
        // T_PONG, y lo que esta version no conozca: se salta.
        break;
    }
}

bool Sesion::arranca(quint32 ritmo, float factor, quint64 ventana_ns)
{
    if (estado_ != Estado::Lista) return false;
    if (!cx_.envia_pod(proto::T_ARRANCA, proto::Arranca{ritmo, factor, ventana_ns}))
        return false;
    ritmo_ = ritmo;
    cambia(Estado::Corriendo);
    return true;
}

bool Sesion::para()
{
    if (estado_ != Estado::Lista && estado_ != Estado::Corriendo) return false;
    return cx_.envia(proto::T_PARA);
}

bool Sesion::pausa()
{
    if (estado_ != Estado::Corriendo) return false;
    return cx_.envia(proto::T_PAUSA);
}

bool Sesion::sigue()
{
    if (estado_ != Estado::Corriendo || ritmo_ == proto::RIT_DEMANDA) return false;
    return cx_.envia(proto::T_SIGUE);
}

bool Sesion::paso(quint64 ns)
{
    if (estado_ != Estado::Corriendo || ritmo_ != proto::RIT_DEMANDA) return false;
    return cx_.envia_pod(proto::T_PASO, proto::Paso{ns});
}

bool Sesion::suscribe(quint64 periodo_ns, const QVector<quint16>& ids)
{
    if (estado_ != Estado::Lista && estado_ != Estado::Corriendo) return false;
    proto::CabSuscribe c{quint32(periodo_ns & 0xFFFFFFFFu), quint32(periodo_ns >> 32),
                         quint32(ids.size()), 0u};
    QByteArray b(reinterpret_cast<const char*>(&c), int(sizeof c));
    for (quint16 id : ids) b.append(reinterpret_cast<const char*>(&id), 2);
    return cx_.envia(proto::T_SUSCRIBE, b);
}

bool Sesion::ordena(const QVector<proto::Orden>& ordenes)
{
    if (ordenes.isEmpty()) return false;
    if (estado_ != Estado::Lista && estado_ != Estado::Corriendo) return false;
    return cx_.envia(proto::T_ORDENES,
                     QByteArray(reinterpret_cast<const char*>(ordenes.constData()),
                                int(ordenes.size() * sizeof(proto::Orden))));
}

bool Sesion::ordena(quint16 pieza, quint16 mando, float valor)
{
    return ordena(QVector<proto::Orden>{proto::Orden{0, pieza, mando, valor}});
}

} // namespace mcusim
