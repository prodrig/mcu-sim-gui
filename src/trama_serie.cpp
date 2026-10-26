#include "trama_serie.h"

#include <QCoreApplication>

namespace mcusim {
namespace trama {

namespace {
QString tr_(const char* s) { return QCoreApplication::translate("trama", s); }

int cifra(QChar c)
{
    const ushort u = c.unicode();
    if (u >= '0' && u <= '9') return u - '0';
    if (u >= 'a' && u <= 'f') return u - 'a' + 10;
    if (u >= 'A' && u <= 'F') return u - 'A' + 10;
    return -1;
}
int cifra(char c) { return cifra(QChar(QLatin1Char(c))); }

bool falla(QString* error, const QString& e)
{
    if (error) *error = e;
    return false;
}
} // namespace

QVector<CrcConocido> crcs_conocidos()
{
    return {
        {Crc::ModbusRtu, tr_("Modbus RTU (CRC-16)"),
         tr_("CRC-16/MODBUS: polinomio 0xA001, inicial 0xFFFF. Dos bytes, el bajo primero.")},
        {Crc::ModbusAscii, tr_("Modbus ASCII (LRC)"),
         tr_("LRC de las parejas de cifras que siguen a ':'. Dos caracteres hexadecimales.")},
        {Crc::Suma256, tr_("Suma modulo 256"),
         tr_("La suma de todos los bytes, modulo 256. Un byte.")},
    };
}

bool desescapa(const QString& texto, QByteArray& out, QString* error)
{
    QByteArray r;
    const int n = int(texto.size());
    for (int i = 0; i < n; ++i) {
        const QChar c = texto.at(i);
        if (c != QLatin1Char('\\')) {
            if (c.unicode() > 0x7F)
                return falla(error, tr_("'%1' en la posicion %2 no es ASCII: ese byte va con \\xNN")
                                        .arg(c).arg(i + 1));
            r.append(char(c.unicode()));
            continue;
        }
        if (i + 1 >= n)
            return falla(error, tr_("acaba en '\\' sin nada detras (para la barra, \\\\)"));
        const QChar e = texto.at(++i);
        switch (e.unicode()) {
        case 'r':  r.append('\r'); break;
        case 'n':  r.append('\n'); break;
        case 't':  r.append('\t'); break;
        case '0':  r.append('\0'); break;
        case '\\': r.append('\\'); break;
        case 'x': {
            // \x{...}: un número par de cifras entre llaves
            if (i + 1 < n && texto.at(i + 1) == QLatin1Char('{')) {
                const int cierre = int(texto.indexOf(QLatin1Char('}'), i + 2));
                if (cierre < 0)
                    return falla(error, tr_("'\\x{' en la posicion %1 sin la llave de cierre")
                                            .arg(i));
                const int k = cierre - (i + 2);
                for (int m = i + 2; m < cierre; ++m)
                    if (cifra(texto.at(m)) < 0)
                        return falla(error, tr_("'%1' en la posicion %2, dentro de \\x{...}, no "
                                                "es una cifra hexadecimal")
                                                .arg(texto.at(m)).arg(m + 1));
                if (k == 0)
                    return falla(error, tr_("'\\x{}' en la posicion %1 esta vacio").arg(i));
                if (k % 2)
                    return falla(error, tr_("'\\x{...}' en la posicion %1 lleva %2 cifras: van "
                                            "de dos en dos, un numero par")
                                            .arg(i).arg(k));
                for (int m = i + 2; m < cierre; m += 2)
                    r.append(char(cifra(texto.at(m)) * 16 + cifra(texto.at(m + 1))));
                i = cierre;
                break;
            }
            // \xNN: exactamente dos cifras; lo que siga ya es texto
            if (i + 2 >= n || cifra(texto.at(i + 1)) < 0 || cifra(texto.at(i + 2)) < 0)
                return falla(error, tr_("'\\x' en la posicion %1 necesita dos cifras "
                                        "hexadecimales detras (o varias, entre llaves: "
                                        "\\x{0D0A})").arg(i));
            r.append(char(cifra(texto.at(i + 1)) * 16 + cifra(texto.at(i + 2))));
            i += 2;
            break;
        }
        default:
            return falla(error, tr_("'\\%1' en la posicion %2 no es un escape: los que hay son "
                                    "\\xNN, \\x{...}, \\r, \\n, \\t, \\0 y \\\\").arg(e).arg(i));
        }
    }
    out = r;
    return true;
}

quint16 crc16_modbus(const QByteArray& d)
{
    quint16 c = 0xFFFF;
    for (char b : d) {
        c ^= quint8(b);
        for (int k = 0; k < 8; ++k) c = (c & 1u) ? quint16((c >> 1) ^ 0xA001u) : quint16(c >> 1);
    }
    return c;
}

quint8 lrc_modbus(const QByteArray& d)
{
    quint8 s = 0;
    for (char b : d) s = quint8(s + quint8(b));
    return quint8(-s);
}

quint8 suma256(const QByteArray& d)
{
    quint8 s = 0;
    for (char b : d) s = quint8(s + quint8(b));
    return s;
}

bool bytes_crc(Crc c, const QByteArray& datos, QByteArray& out, QString* error)
{
    out.clear();
    switch (c) {
    case Crc::ModbusRtu: {
        const quint16 v = crc16_modbus(datos);
        out.append(char(v & 0xFF));
        out.append(char(v >> 8));
        return true;
    }
    case Crc::ModbusAscii: {
        // ':' y parejas de cifras: el LRC es de los bytes que representan
        const QByteArray h = datos.startsWith(':') ? datos.mid(1) : datos;
        if (h.isEmpty() || h.size() % 2)
            return falla(error, tr_("Modbus ASCII: despues de ':' tiene que haber un numero par "
                                    "de cifras hexadecimales, y hay %1 caracteres").arg(h.size()));
        QByteArray b;
        for (int i = 0; i < h.size(); i += 2) {
            const int a = cifra(h.at(i)), z = cifra(h.at(i + 1));
            if (a < 0 || z < 0)
                return falla(error, tr_("Modbus ASCII: '%1' no es una pareja de cifras "
                                        "hexadecimales")
                                        .arg(QString::fromLatin1(h.mid(i, 2))));
            b.append(char(a * 16 + z));
        }
        out = QByteArray::number(lrc_modbus(b), 16).toUpper().rightJustified(2, '0');
        return true;
    }
    case Crc::Suma256:
        out.append(char(suma256(datos)));
        return true;
    }
    return falla(error, tr_("CRC desconocido"));
}

bool construye(const QString& texto, const Opciones& o, QByteArray& trama, QString* error)
{
    QByteArray d;
    if (!desescapa(texto, d, error)) return false;
    if (o.crc) {
        QByteArray c;
        if (!bytes_crc(o.tipo, d, c, error)) return false;
        d += c;
    }
    if (o.cr) d.append('\r');
    if (o.lf) d.append('\n');
    trama = d;
    return true;
}

bool imprimible(quint8 c)
{
    return c == '\t' || (c >= 0x20 && c < 0x7F);
}

QString legible(const QByteArray& b, bool& tras_cr, int* cola)
{
    static const char hex[] = "0123456789ABCDEF";
    QString s;
    s.reserve(b.size());
    const int n = int(b.size());
    int i = 0;
    while (i < n) {
        const quint8 c = quint8(b.at(i));
        if (c == '\n') {
            if (!tras_cr) s += QLatin1Char('\n');
            tras_cr = false;
            ++i;
            continue;
        }
        tras_cr = false;
        if (c == '\r') {
            s += QLatin1Char('\n');
            tras_cr = true;
            ++i;
            continue;
        }
        if (c == '\\') {                 // la barra, doblada: si no, \x41 escrito
            s += QLatin1String("\\\\");  // como texto se leería como un byte
            ++i;
            continue;
        }
        if (imprimible(c)) {
            s += QLatin1Char(char(c));
            ++i;
            continue;
        }
        // Los que no se imprimen, seguidos: uno solo, \xNN; varios, \x{...}
        int j = i;
        while (j < n && quint8(b.at(j)) != '\r' && quint8(b.at(j)) != '\n' &&
               !imprimible(quint8(b.at(j))))
            ++j;
        s += QLatin1String(j - i > 1 ? "\\x{" : "\\x");
        for (int k = i; k < j; ++k) {
            const quint8 x = quint8(b.at(k));
            s += QLatin1Char(hex[x >> 4]);
            s += QLatin1Char(hex[x & 15]);
        }
        if (j - i > 1) s += QLatin1Char('}');
        if (cola) *cola = j == n ? j - i : 0;
        i = j;
    }
    if (cola && (n == 0 || imprimible(quint8(b.at(n - 1))) || quint8(b.at(n - 1)) == '\r' ||
                 quint8(b.at(n - 1)) == '\n'))
        *cola = 0;
    return s;
}

} // namespace trama
} // namespace mcusim
