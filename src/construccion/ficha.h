#pragma once
// =============================================================================
// ficha.h — Lo que mcu-sim-build sabe de cada cosa de la biblioteca
// (G9 de doc/analisis_build.md)
//
// Lee los ficheros que exporta mcu-sim (src/biblioteca):
//   * TIPO.pieza.xml   la ficha de un tipo de pieza: terminales y atributos (B1)
//   * TIPO.forma.xml   dónde está cada terminal en su dibujo, o su generador (B5)
//   * TIPO.mcu.xml     el patillaje de un MCU y lo que hace cada patilla (B3, B4)
//   * P.conector.xml   una tira con nombre y el uso de cada pin (B6)
// y, para las piezas PARAMÉTRICAS (Conector, Jumper, I2cWire, CameraSensor),
// genera aquí su dibujo a partir de sus atributos (D5).
//
// Sin pantalla: QtCore y QtXml-a-mano (QXmlStreamReader). El dibujo es texto
// SVG; pintarlo es cosa del lienzo.
// =============================================================================
#include <QMap>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

namespace mcusim::build {

// Un terminal de una pieza, y si hace falta
struct Terminal {
    QString nombre;
    QString cuando;          // "obligatorio", "al menos uno", "con diodo=si"...
    bool    alternativo = false;   // de una fila con «o»: anodo o catodo
};

// Una serie abierta: d0, d1, ... hasta donde se quiera
struct Serie {
    QString prefijo;
    int     desde = 0;
};

struct Atributo {
    QString nombre;
    QString omision;
    QString texto;
};

struct FichaPieza {
    QString tipo;
    QString resumen;
    QVector<Terminal> terminales;     // los fijos, en orden
    QVector<Serie>    series;         // las abiertas
    QVector<Atributo> atributos;      // uno por nombre (los compuestos, partidos)
    bool sin_terminales = false;
    bool valida() const { return !tipo.isEmpty(); }
    const Atributo* atributo(const QString& n) const;
};

// Dónde está un terminal en el dibujo, en unidades del viewBox (0,1 mm)
struct Ancla {
    QString terminal;
    QPointF p;
    QPointF dir;             // hacia dónde sale el hilo: (0,1) abajo...
};

struct Forma {
    QString generador;                // "conector", "bus", "modulo"; o vacío
    QString svg;                      // el texto del SVG
    QSizeF  tam;                      // en mm
    QVector<Ancla> anclas;
    bool valida() const { return !svg.isEmpty(); }
    const Ancla* ancla(const QString& t) const;
};

// Una patilla de un MCU
struct Funcion {
    QString nombre;
    QString modo;                     // "1", "af5"... como lo dice el .mcu.xml
    bool    simulada = false;
    bool    analogica = false;
};
struct Patilla {
    QString n;                        // la posición: "21", "A1"
    QString nombre;                   // "PA5", "VDD"
    QString nodo;                     // el nombre del nodo en el XML de placa
    QString clase;                    // es, alimentacion, reset, control...
    QVector<Funcion> funciones;
};
struct FichaMcu {
    QString tipo, encapsulado, familia, fuente;
    QVector<Patilla> patillas;
    QString svg_detalle, svg_simple;  // rutas
    bool valida() const { return !tipo.isEmpty(); }
    const Patilla* patilla(const QString& n) const;
};

// Un perfil de conector (B6)
struct PinPerfil {
    int n = 0;
    QString uso, clase, otros;
};
struct Perfil {
    QString nombre, titulo, fuente;
    int filas = 1, columnas = 1;
    QVector<PinPerfil> pines;
    bool valido() const { return !nombre.isEmpty(); }
};

// Los lectores. Si algo no se entiende, `error` lo dice y se devuelve vacío
FichaPieza lee_pieza(const QString& ruta, QString* error = nullptr);
Forma      lee_forma(const QString& ruta_forma, QString* error = nullptr);
FichaMcu   lee_mcu(const QString& ruta, QString* error = nullptr);
Perfil     lee_perfil(const QString& ruta, QString* error = nullptr);

// --- Las paramétricas (D5) ---------------------------------------------------
// El dibujo de una pieza que depende de sus atributos, con sus anclas. Para
// `conector` (Conector, Jumper): filas, columnas, numeracion, nombres. Para
// `bus` y `modulo`: los terminales fijos de la ficha más `n_serie` de cada
// serie abierta
Forma genera_forma(const QString& generador, const FichaPieza& ficha,
                   const QMap<QString, QString>& atributos, int n_serie);

// Los terminales que tiene una instancia: los fijos y `n_serie` de cada serie
// abierta; en un Conector o un Jumper, sus pines (1..N, o sus nombres)
QStringList terminales_de(const FichaPieza& ficha, const QMap<QString, QString>& atributos,
                          int n_serie);

// Los nombres de los pines de un Conector: el número, o el nombre de
// `nombres=` («-» deja el número)
QStringList pines_conector(const QMap<QString, QString>& atributos);

} // namespace mcusim::build
