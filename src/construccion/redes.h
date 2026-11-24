#pragma once
// =============================================================================
// redes.h — De las conexiones dibujadas a los nodos del XML (G10 de
// doc/analisis_build.md, §3.1)
//
// Una placa de mcu-sim no es un esquema: cada patilla va a un NODO. Aquí, cada
// conexión une dos terminales (union-find) y cada clase que sale es una red.
// Su nombre, como lo escribiría una persona:
//
//   la red tiene...                   el nodo es...
//   un pad de un MCU                  el pad (PA5; u0.PA5 con dos MCU)
//   dos o más pads                    <nodo id="n_PB9" une="PB9 PD3"/>: un
//                                     PUENTE de verdad, que se avisa
//   una alimentación o un control     su nombre (VDD, VSS, NRST)
//   ningún pad y el pin de un         el pin (CN9.8), sin nodo nuevo
//   conector
//   nada de eso                       <nodo id="n_LD2_anodo" externo="si"/>
//
// Una red de un solo terminal no es nada: su <pin> no se escribe. Y se dice
// lo que falta: un terminal obligatorio sin conectar, una alimentación unida
// a otra o a un pad.
// =============================================================================
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include "catalogo.h"
#include "documento.h"

namespace mcusim::build {

struct NodoDeclarado {
    QString id;
    bool    externo = false;
    bool    bus = false;
    QStringList une;                 // pads de un puente
};

struct Red {
    QStringList terminales;          // ID.terminal, en orden
    QString     nodo;                // el nombre que se escribe
};

struct Redes {
    QVector<Red> redes;
    QMap<QString, QString> nodo_de;      // ID.terminal -> nodo, de lo que se escribe
    QVector<NodoDeclarado> nodos;        // los <nodo> que hacen falta
    QStringList avisos;                  // se puede exportar, pero conviene mirarlo
    QStringList errores;                 // no se debe exportar
    // Los terminales que se marcan en rojo: obligatorios sin conectar
    QStringList sin_conectar;
};

Redes calcula_redes(const Documento& d, const Catalogo& c);

} // namespace mcusim::build
