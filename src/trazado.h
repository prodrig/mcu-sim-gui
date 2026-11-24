#pragma once
// =============================================================================
// trazado.h — La geometría de las líneas que unen dos pines (G8 de
// doc/analisis_build.md, D7)
//
// La comparten las dos ventanas: mcu-sim-gui, para los hilos y los acoples de
// un sistema (plan §37 y §44), y mcu-sim-build, para las conexiones de una
// placa y de un sistema. Es geometría y nada más: puntos, cajas y caminos, en
// las unidades de quien la llama, sin escena, sin widgets y sin sesión.
//
// Dos trazados:
//   * en CURVA: sale de cada extremo por el lado que mira al otro y llega con
//     una cúbica (`curva`);
//   * en TRAMOS RECTOS: horizontales y verticales que alternan, con las
//     esquinas redondeadas (`redondeada`). Lo dicen el eje del primer tramo y
//     sus codos -la x o la y de cada tramo intermedio-: es lo mismo que guarda
//     una <ruta> de un sistema y una <conexion trazado="tramos"> de un <build>.
// =============================================================================
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QVector>

namespace mcusim::trazado {

// Por dónde engancha una línea en un extremo, y hacia dónde sale
struct Enganche {
    QPointF p, dir;
    bool valido() const { return !dir.isNull(); }
};

// El enganche en la caja `r` de una línea que va hacia `hacia`: por el lado
// que mira a ella -izquierda o derecha si está más de lado que encima o
// debajo; arriba o abajo si no-, en el centro de ese lado
Enganche enganche(const QRectF& r, const QPointF& hacia);

// La curva de `a` a `b`: sale y entra por su lado, con un brazo del 45 % de la
// mayor separación y nunca menos de `minimo`
QPainterPath curva(const Enganche& a, const Enganche& b, double minimo);

// Los puntos de una línea en tramos rectos, de la caja `ra` a la `rb`. El
// primer tramo es horizontal si `horizontal`, y sale de `ra` por el lado que
// mira a su primer codo; los dos últimos llegan solos a `rb`, por el lado que
// mira al último
QVector<QPointF> puntos_ruta(const QRectF& ra, const QRectF& rb, bool horizontal,
                             const QVector<double>& codos);

// Y de vuelta: los codos de unos puntos que alternan horizontal y vertical,
// sin los dos últimos tramos, que salen solos
QVector<double> codos_de(const QVector<QPointF>& p, bool horizontal);

// Una polilínea con las esquinas redondeadas, de radio `r` como mucho: en
// cada esquina, la mitad del tramo más corto. Los tramos de largo cero y los
// puntos en línea no hacen esquina
QPainterPath redondeada(const QVector<QPointF>& puntos, double r);

// La distancia de `s` al segmento de `a` a `b`
double distancia(const QPointF& s, const QPointF& a, const QPointF& b);

// Al pasar de curva a tramos: el eje del primer tramo -el de la salida de la
// curva desde `pa`- y un codo a mitad de camino
bool eje_de_curva(const QPainterPath& curva, const QPointF& pa);
double codo_medio(const QPointF& pa, const QPointF& pb, bool horizontal);

// Un codo más donde se pincha (`s`): en el tramo más cercano, dos puntos
// iguales -un tramo de largo cero entre dos mitades que siguen alternando-.
// Devuelve los codos nuevos
QVector<double> con_codo(const QVector<QPointF>& puntos, const QPointF& s, bool horizontal);

// El tramo de unos puntos a menos de `tol` de `s`, o -1
int tramo_en(const QVector<QPointF>& puntos, const QPointF& s, double tol);

// ¿Pasa la curva a menos de `tol` de `s`?
bool toca(const QPainterPath& camino, const QPointF& s, double tol);

} // namespace mcusim::trazado
