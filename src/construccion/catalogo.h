#pragma once
// =============================================================================
// catalogo.h — De una instancia a su ficha, su dibujo y sus terminales
// (G10 de doc/analisis_build.md)
//
// Sobre la Biblioteca: lee cada .pieza.xml, .forma.xml y .mcu.xml una vez y
// los guarda. Para una instancia dice:
//   * su dibujo (el SVG, con su tamaño en mm) y dónde está cada terminal en
//     él: el de la biblioteca, el que se genera para una paramétrica con sus
//     atributos (D5), o el simple o el detallado de un MCU;
//   * sus terminales: los de la ficha; en un MCU, las posiciones de sus
//     patillas;
//   * el nodo que da a la red cada patilla de un MCU (PA5, VDD).
// =============================================================================
#include <QHash>
#include <QSizeF>
#include <QString>

#include "biblioteca.h"
#include "documento.h"
#include "ficha.h"

namespace mcusim::build {

struct Dibujo {
    QString svg;                     // el texto
    QSizeF  tam;                     // mm
    QVector<Ancla> anclas;           // en 0,1 mm, desde la esquina del viewBox
    bool valido() const { return !svg.isEmpty(); }
    const Ancla* ancla(const QString& t) const;
};

class Catalogo {
public:
    explicit Catalogo(const Biblioteca* b) : bib_(b) {}
    const Biblioteca* biblioteca() const { return bib_; }

    const FichaPieza* pieza(const QString& tipo) const;
    const FichaMcu*   mcu(const QString& tipo) const;
    const Perfil*     perfil(const QString& nombre) const;

    Dibujo dibujo(const Instancia& i) const;
    QStringList terminales(const Instancia& i) const;
    // Un MCU: el nodo de la patilla en la posición `n`
    QString nodo_mcu(const Instancia& i, const QString& n) const;
    // Y al revés: la primera posición cuyo nodo es `nodo`
    QString posicion_mcu(const Instancia& i, const QString& nodo) const;
    // El nombre que se enseña de un terminal: el del pad en un MCU (21 PA5),
    // el uso en un Conector de un perfil (6 D13)
    QString rotulo(const Instancia& i, const QString& terminal) const;

    // La instancia nueva de una entrada, sin id ni posición: tipo, clase y
    // atributos de partida (filas y columnas de un perfil)
    Instancia nueva(const Entrada& e) const;

    void olvida();           // tras refrescar la biblioteca

private:
    const Biblioteca* bib_;
    mutable QHash<QString, FichaPieza> piezas_;
    mutable QHash<QString, Forma> formas_;
    mutable QHash<QString, FichaMcu> mcus_;
    mutable QHash<QString, Perfil> perfiles_;
    mutable QHash<QString, QString> svgs_;
};

} // namespace mcusim::build
