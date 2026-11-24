#pragma once
// =============================================================================
// importa.h — Una placa escrita, leída como mcu-sim-build la entiende
// (G10 de doc/analisis_build.md, para su prueba de ida y vuelta; G11 lo usará
// para reabrir)
//
//   * `particion`: el NETLIST de un XML de placa como conjunto de redes, cada
//     una con lo que une: ID.terminal de cada pieza (el pin de un conector,
//     por su número), mcu:NODO de cada pad o alimentación. Es lo que se
//     compara: dos placas con la misma partición dan el mismo montaje en
//     mcu-sim, se llamen como se llamen sus nodos;
//   * `reconstruye`: el documento de una placa sin <build> -las piezas en una
//     rejilla y cada red como una estrella desde su primer terminal-, si todo
//     lo que lleva se puede dibujar. Si no, dice qué no.
// =============================================================================
#include <QSet>
#include <QString>
#include <QStringList>

#include "catalogo.h"
#include "documento.h"

namespace mcusim::build {

using Particion = QSet<QStringList>;     // cada red, ordenada

struct PlacaLeida {
    QString nombre;
    struct Mcu { QString tipo, id; };
    QVector<Mcu> mcus;
    struct Nodo { QString id; bool externo = false, bus = false; QStringList une; };
    QVector<Nodo> nodos;
    struct Componente {
        QString tipo, id;
        QMap<QString, QString> atributos;
        QVector<QPair<QString, QString>> pines;   // nombre -> nodo
    };
    QVector<Componente> componentes;
    QStringList no_soportado;     // lo que mcu-sim-build todavía no dibuja
};

// Lee un XML de placa. Los comentarios se quitan antes: algunas placas llevan
// «--» dentro, que mcu-sim acepta y un lector de XML estricto no
PlacaLeida lee_placa(const QString& texto, QString* error = nullptr);

Particion particion(const PlacaLeida& p, const Catalogo& c);

// El documento de una placa; vacío y con `motivo` si no se puede
Documento reconstruye(const PlacaLeida& p, const Catalogo& c, QString* motivo = nullptr);

} // namespace mcusim::build
