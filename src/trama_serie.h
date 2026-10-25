// =============================================================================
// trama_serie.h — Lo que se teclea en la pestaña de un puente serie, hecho
// bytes; y lo que llega, hecho texto
//
// La pestaña de cada PuenteSerie en memoria (`consola_serie.h`) manda lo que
// se escribe en su entrada con T_SERIE_ENVIA. Esto es lo que hay entre las
// dos cosas, sin un solo widget, para poder probarlo solo (`prueba_trama`):
//
//   * LA ENTRADA es ASCII, con escapes intercalados donde se quiera:
//
//       \xNN, \xNNMM, \xNNMMOO...  bytes en hexadecimal, de dos en dos cifras:
//                                  tantos como pares de cifras sigan a la x.
//                                  Un número impar de cifras es un error: no
//                                  hay forma de saber de quién es la suelta
//       \r \n \t \0 \\             los de C de siempre
//
//     Un hexadecimal se acaba en lo primero que no es una cifra. Para poner
//     una cifra justo detrás como ASCII, otro escape: `\x41\x42` y no `\x4142`
//     si se quiere «AB», o `\x01` y `2` escrito como `\x32`. Lo demás que no
//     sea ASCII (una ñ, un €) es un error: el byte que se quiera va con \xNN.
//
//   * EL FINAL, en este orden: lo escrito, el CRC si se pide —calculado sobre
//     lo escrito, solo eso—, CR (0x0D) si se pide y LF (0x0A) si se pide. CR
//     y LF no entran nunca en el CRC.
//
//   * LOS CRC conocidos, con sus bytes:
//
//       Modbus RTU     CRC-16/MODBUS: polinomio 0x8005 reflejado (0xA001),
//                      valor inicial 0xFFFF, sin XOR final. Dos bytes, el bajo
//                      primero, como en la trama [Modbus over Serial Line
//                      V1.02, §2.5.1.2 y §6.2.2]
//       Modbus ASCII   LRC: el complemento a dos de la suma, módulo 256, de los
//                      bytes que representan las parejas de cifras que van
//                      después de ':' [§2.5.2.2 y §6.2.1]. Se añade como dos
//                      caracteres ASCII en hexadecimal, en mayúsculas. Lo
//                      escrito tiene que ser una trama ASCII: ':' (opcional) y
//                      un número par de cifras hexadecimales
//       Suma mód. 256  la suma de todos los bytes, módulo 256: un byte
//
//   * LO QUE LLEGA se enseña como texto (`legible`): lo imprimible tal cual,
//     un salto de línea por cada CR, LF o CR+LF, y lo demás como \xNN.
// =============================================================================
#ifndef MCU_SIM_GUI_TRAMA_SERIE_H
#define MCU_SIM_GUI_TRAMA_SERIE_H

#include <QByteArray>
#include <QString>
#include <QVector>

namespace mcusim {
namespace trama {

enum class Crc { ModbusRtu = 0, ModbusAscii = 1, Suma256 = 2 };

struct CrcConocido {
    Crc     id;
    QString nombre;      // lo que sale en la lista
    QString ayuda;       // la ayuda emergente
};
// Los de la lista, en su orden
QVector<CrcConocido> crcs_conocidos();

// Lo escrito, en bytes. false, con `error` diciendo qué y dónde (desde 1), si
// algo no vale.
bool desescapa(const QString& texto, QByteArray& out, QString* error = nullptr);

// Los tres cálculos
quint16 crc16_modbus(const QByteArray& d);
quint8  lrc_modbus(const QByteArray& d);
quint8  suma256(const QByteArray& d);

// Los bytes que se añaden con ese CRC. false, con `error`, si no se puede
// calcular (Modbus ASCII sobre algo que no es una trama ASCII).
bool bytes_crc(Crc c, const QByteArray& datos, QByteArray& out, QString* error = nullptr);

struct Opciones {
    bool cr = false, lf = false, crc = false;
    Crc  tipo = Crc::ModbusRtu;
};
// La trama entera: lo escrito, el CRC, CR y LF.
bool construye(const QString& texto, const Opciones& o, QByteArray& trama,
               QString* error = nullptr);

// Lo que llega, para la pantalla. `tras_cr` dice si lo anterior acabó en CR,
// para que un CR+LF partido entre dos mensajes sea un salto y no dos.
QString legible(const QByteArray& b, bool& tras_cr);

} // namespace trama
} // namespace mcusim

#endif // MCU_SIM_GUI_TRAMA_SERIE_H
