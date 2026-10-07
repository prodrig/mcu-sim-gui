// =============================================================================
// generado.h — el dibujo de una placa que no trae el suyo
//
// Fase 5 de `doc/analisis-uso-ilustraciones.md` (§7): si una placa no tiene
// dibujo, la ventana lo GENERA con lo que ya sabe de ella, y sin conocer un
// tipo. No es bonito, pero se usa igual que uno de verdad, porque ES uno de
// verdad: un SVG con un elemento por pieza, cuyo id es el nombre de la pieza
// en su placa, que pasa por el mismo camino que cualquier otro -`DibujoPlaca`,
// sus efectos y sus mandos-.
//
//   * un rectángulo con el nombre de la placa;
//   * sus CHIPS, como cuadrados oscuros con su tipo: decorado;
//   * cada PIEZA con un glifo según su DECLARACIÓN (`glifo_de`): un vidrio
//     oscuro con la forma de su imagen si enseña una (plan §30), un botón si
//     su primer mando es `boton`, un interruptor, un mando giratorio si es
//     `continuo` o `discreto`; si no tiene mandos, un piloto redondo si tiene
//     un 0/1 que no es alarma, un recuadro de medida si tiene otro
//     observable, y si no tiene nada, una pieza pequeña y gris. Con su nombre
//     debajo;
//   * cada CONECTOR con su forma de verdad -filas, columnas y numeración, que
//     `T_PLACA` dice de cada placa de un sistema- como una tira de pines, con
//     `id="CN5"` el cuerpo y `id="CN5.1"`... cada pin. En una placa suelta
//     `T_PLACA` no los describe, y van como una pieza más.
//
// Todo en MILÍMETROS (`width="..mm"`, el viewBox en mm): los pines van a 2,54
// mm, y la placa sale a la misma escala que un dibujo de verdad que diga su
// tamaño (§10).
//
// LA BANDEJA (§7): si el dibujo de una placa no tiene todas sus piezas, las
// que faltan -las que dejan ver o tocar algo- van a una bandeja al lado, que
// es otro dibujo generado, solo con ellas (`OpcionesGenerado::solo`).
// =============================================================================
#ifndef MCU_SIM_GUI_GENERADO_H
#define MCU_SIM_GUI_GENERADO_H

#include <QByteArray>
#include <QString>
#include <QVector>

#include "placa.h"

namespace mcusim {

struct OpcionesGenerado {
    // Solo estas piezas (sus `idx`), sin chips ni conectores: una bandeja.
    // Vacío: la placa entera.
    QVector<int> solo;
    // Lo que dice arriba; vacío, el id y el nombre de la placa
    QString      titulo;
};

// El SVG de la placa `placa_id` del sistema, o de la placa entera si está
// vacío.
QByteArray dibujo_generado(const PlacaGui& placa, const QString& placa_id,
                           const OpcionesGenerado& o = OpcionesGenerado());

// El glifo de una pieza, por lo que declara: "pantalla" si enseña una imagen,
// "boton", "interruptor", "mando", "piloto", "medida" o "pieza"
QString glifo_de(const PiezaGui& p);

// Las piezas que van a la bandeja de un dibujo: de las que faltan en él, las
// que tienen algo que ver o que tocar
QVector<int> para_bandeja(const PlacaGui& placa, const QString& placa_id,
                          const QStringList& sin_elemento);

} // namespace mcusim

#endif // MCU_SIM_GUI_GENERADO_H
