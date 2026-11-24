#pragma once
// =============================================================================
// documento.h — Una placa en mcu-sim-build: piezas colocadas y conexiones
// dibujadas (G10 de doc/analisis_build.md)
//
// El modelo, sin pantalla. Es un VALOR: el deshacer guarda el documento de
// antes y el de después de cada cambio, y el lienzo se pinta desde él. Lo que
// guarda es lo que irá al <build> (§7 del análisis) más lo que irá al XML:
//
//   * cada INSTANCIA: su id, de qué entrada de la biblioteca sale (`fuente`:
//     piezas/Led, mcus/STM32F446RE, conectores/arduino_uno_r3.digital_alto),
//     su tipo, dónde está su CENTRO en la placa (mm), su giro (0, 90, 180,
//     270) y su escala, el dibujo de un MCU (simple o detalle) y los
//     atributos que se han escrito;
//   * cada CONEXIÓN entre dos terminales, `ID.terminal` -en un MCU, el
//     terminal es la POSICIÓN de la patilla: u0.21-, su trazado (curva o
//     tramos, con el eje del primer tramo y los codos en mm) y si se ve;
//   * el MARCO de la placa (mm), o nulo para el que abarca todo;
//   * las redes que son BUS (varias piezas que conducen en el mismo nodo): por
//     uno cualquiera de sus terminales.
// =============================================================================
#include <QMap>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

namespace mcusim::build {

struct Instancia {
    enum Clase { Pieza, Mcu };
    Clase   clase = Pieza;
    QString id;
    QString fuente;                  // piezas/Led, mcus/STM32F446RE...
    QString tipo;                    // Led, STM32F446RE, Conector
    QString perfil;                  // si es un Conector de un perfil (B6)
    QPointF pos;                     // el centro, en mm
    int     giro = 0;                // 0, 90, 180, 270
    double  escala = 1.0;
    bool    detalle = false;         // MCU: dibujo detallado o simple
    QMap<QString, QString> atributos;    // los escritos
    int     n_serie = 8;             // cuántos de una serie abierta (d0..d7)
    bool operator==(const Instancia& o) const;
};

struct Conexion {
    QString a, b;                    // ID.terminal
    bool    tramos = false;
    bool    horizontal = true;       // el eje del primer tramo
    QVector<double> codos;           // mm
    bool    visible = true;
    bool operator==(const Conexion& o) const;
    bool une(const QString& x, const QString& y) const {
        return (a == x && b == y) || (a == y && b == x);
    }
};

struct Documento {
    QString nombre = QStringLiteral("placa");
    QRectF  marco;                   // mm; nulo: el automático
    QVector<Instancia> instancias;
    QVector<Conexion>  conexiones;
    QStringList bus;                 // un terminal de cada red que es bus

    int indice(const QString& id) const;
    Instancia* instancia(const QString& id);
    const Instancia* instancia(const QString& id) const;
    bool operator==(const Documento& o) const;
    bool operator!=(const Documento& o) const { return !(*this == o); }

    // Quitar una instancia quita sus conexiones
    void quita(const QString& id);
    // Renombrar una instancia renombra sus conexiones
    void renombra(const QString& viejo, const QString& nuevo);
    // ¿Hay ya una conexión entre los dos?
    bool conectados(const QString& a, const QString& b) const;
};

// El id de un terminal `ID.terminal`, y el terminal
QString id_de(const QString& ref);
QString terminal_de(const QString& ref);

// ¿Vale `id` para una pieza nueva? Vacío si sí; si no, por qué. No puede
// repetirse, ni llevar «/» ni «.», tiene que empezar por letra o _ y seguir
// con letras, cifras o _, y no puede hacer de `ID.k` un pad: con P1 o PA,
// `P1.1` sería el pad PB1 (doc/parts.md, «Los pines con nombre»)
QString motivo_id_invalido(const Documento& d, const QString& id);
// Un id libre con ese prefijo: R1, R2... (u0, u1 con desde = 0)
QString id_libre(const Documento& d, const QString& prefijo, int desde = 1);

} // namespace mcusim::build
