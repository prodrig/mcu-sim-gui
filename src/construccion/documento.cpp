#include "documento.h"

#include <QRegularExpression>

namespace mcusim::build {

bool Instancia::operator==(const Instancia& o) const
{
    return clase == o.clase && id == o.id && fuente == o.fuente && tipo == o.tipo &&
           perfil == o.perfil && pos == o.pos && giro == o.giro && escala == o.escala &&
           detalle == o.detalle && atributos == o.atributos && n_serie == o.n_serie;
}

bool Conexion::operator==(const Conexion& o) const
{
    return a == o.a && b == o.b && tramos == o.tramos && horizontal == o.horizontal &&
           codos == o.codos && visible == o.visible;
}

int Documento::indice(const QString& id) const
{
    for (int i = 0; i < instancias.size(); ++i)
        if (instancias[i].id == id) return i;
    return -1;
}

Instancia* Documento::instancia(const QString& id)
{
    const int i = indice(id);
    return i < 0 ? nullptr : &instancias[i];
}

const Instancia* Documento::instancia(const QString& id) const
{
    const int i = indice(id);
    return i < 0 ? nullptr : &instancias[i];
}

bool Documento::operator==(const Documento& o) const
{
    return nombre == o.nombre && marco == o.marco && instancias == o.instancias &&
           conexiones == o.conexiones && bus == o.bus;
}

void Documento::quita(const QString& id)
{
    const int i = indice(id);
    if (i < 0) return;
    instancias.removeAt(i);
    for (int k = conexiones.size() - 1; k >= 0; --k)
        if (id_de(conexiones[k].a) == id || id_de(conexiones[k].b) == id) conexiones.removeAt(k);
    for (int k = bus.size() - 1; k >= 0; --k)
        if (id_de(bus[k]) == id) bus.removeAt(k);
}

void Documento::renombra(const QString& viejo, const QString& nuevo)
{
    Instancia* x = instancia(viejo);
    if (!x) return;
    x->id = nuevo;
    auto cambia = [&](QString& r) {
        if (id_de(r) == viejo) r = nuevo + QLatin1Char('.') + terminal_de(r);
    };
    for (Conexion& c : conexiones) {
        cambia(c.a);
        cambia(c.b);
    }
    for (QString& b : bus) cambia(b);
}

bool Documento::conectados(const QString& a, const QString& b) const
{
    for (const Conexion& c : conexiones)
        if (c.une(a, b)) return true;
    return false;
}

QString id_de(const QString& ref)
{
    return ref.section(QLatin1Char('.'), 0, 0);
}

QString terminal_de(const QString& ref)
{
    return ref.section(QLatin1Char('.'), 1);
}

QString motivo_id_invalido(const Documento& d, const QString& id)
{
    if (id.isEmpty()) return QStringLiteral("el id está vacío");
    static const QRegularExpression forma(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    if (!forma.match(id).hasMatch())
        return QStringLiteral("«%1»: empieza por letra o _ y sigue con letras, cifras o _ "
                              "(sin «/» ni «.»)").arg(id);
    static const QRegularExpression pad(QStringLiteral("^P([A-Z]|[0-9]+|J)?$"));
    if (pad.match(id).hasMatch())
        return QStringLiteral("«%1»: con ese id, «%1.1» sería un pad (P1.1 es PB1 en un STM32, "
                              "PA.1 es PA1): mejor otro").arg(id);
    if (d.indice(id) >= 0) return QStringLiteral("ya hay una pieza «%1»").arg(id);
    return {};
}

QString id_libre(const Documento& d, const QString& prefijo, int desde)
{
    for (int k = desde;; ++k) {
        const QString c = prefijo + QString::number(k);
        if (motivo_id_invalido(d, c).isEmpty()) return c;
    }
}

} // namespace mcusim::build
