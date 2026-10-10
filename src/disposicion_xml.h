// =============================================================================
// disposicion_xml.h — Lo colocado en la ilustración, escrito en el XML de mcu-sim
//
// Plan §38 (fase 4 de `doc/analisis_disposicion_ilustracion.md`). La
// disposición de una ilustración -dónde va cada placa, su giro y su escala, el
// lienzo y las líneas en tramos rectos- se puede llevar al XML del sistema, o
// de la placa suelta, que es de donde `mcu-sim` la lee y la manda en T_PLACA.
// Aquí están las dos formas de llevarla:
//
//   * `texto_disposicion`: las líneas que hay que poner en el XML, para
//     copiarlas y pegarlas a mano -«Copiar como XML»-;
//   * `escribe_disposicion`: el XML ya escrito con ellas -«Guardar en el XML»,
//     cuando la ventana lanzó `mcu-sim` y sabe dónde está el fichero-.
//
// SE ESCRIBE COMO TEXTO, sin rehacer el XML, como hace `mcu-sim` con los SVG
// (`common/svg_variantes.h`): se buscan las etiquetas que importan -la raíz,
// cada <placa id> del sistema, cada <ruta>-, se cambian, se quitan o se ponen
// solo sus atributos, y todo lo demás -comentarios, espacios, el orden de los
// atributos, las placas escritas dentro- se queda como estaba. Un XML que no
// se entiende no se toca: se dice por qué.
//
// Qué se escribe, por placa:
//
//   * `x` e `y`, si tiene sitio fijo -solo en un sistema-; si no, se quitan;
//   * `giro` y `escala`, solo si cambian respecto a lo que el XML ya decía
//     -puede venir de la placa y no del montaje, y entonces hay que decirlo
//     en el montaje aunque sea el valor de siempre-. En la raíz de una placa
//     suelta, el de siempre se quita;
//
// y el `lienzo` en la raíz, y las <ruta>: las que había se quitan y las nuevas
// van al final del sistema.
// =============================================================================
#ifndef MCU_SIM_GUI_DISPOSICION_XML_H
#define MCU_SIM_GUI_DISPOSICION_XML_H

#include <QByteArray>
#include <QString>
#include <QVector>
#include <optional>

#include "placa.h"

namespace mcusim {

struct DisposicionXml {
    bool sistema = false;
    bool lienzo_fijo = false;
    double lienzo[4] = {0, 0, 0, 0};     // x, y, ancho, alto, en mm
    struct Placa {
        QString id;                      // vacío en una placa suelta
        QString fichero;                 // el de su <placa id>, para el texto
        bool    colocada = false;
        double  x = 0, y = 0;
        std::optional<int>    giro;      // sin valor: no se toca
        std::optional<double> escala;
    };
    QVector<Placa>   placas;
    QVector<RutaGui> rutas;
};

// Un número como se escribe en el XML: «12», «-3.5»
QString numero_xml(double v);

// Las líneas para pegar en el XML, con un comentario que dice dónde
QString texto_disposicion(const DisposicionXml& d, const QString& nombre);

// Pone la disposición en `xml`. false, y `error` dice por qué, si el XML no se
// entiende o no tiene alguna de las placas; entonces `xml` no cambia
bool escribe_disposicion(QByteArray& xml, const DisposicionXml& d, QString& error);

} // namespace mcusim

#endif // MCU_SIM_GUI_DISPOSICION_XML_H
