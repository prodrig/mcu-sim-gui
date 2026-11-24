#pragma once
// =============================================================================
// valida.h — Preguntarle a mcu-sim si una placa vale (G10, N18)
//
// `mcu-sim PLACA.xml --valida` monta la placa sin simularla y dice qué falla:
// es la única forma fiable de saber que lo que se va a escribir se usará tal
// cual. Se escribe en una carpeta temporal, con su SVG, y se le pregunta.
// =============================================================================
#include <QString>
#include <QStringList>

#include "catalogo.h"
#include "documento.h"

namespace mcusim::build {

struct Validacion {
    bool    lanzado = false;     // si mcu-sim se pudo ejecutar
    bool    valida = false;      // si dijo que sí (código 0)
    int     codigo = -1;
    QStringList lineas;          // lo que dijo, sin la cabecera de SystemC
    QString texto() const { return lineas.join(QLatin1Char('\n')); }
};

// Con el ejecutable `mcu_sim`, sobre un fichero ya escrito
Validacion valida_fichero(const QString& mcu_sim, const QString& ruta_xml, int ms = 60000);
// Escribe el documento en una carpeta temporal y lo valida
Validacion valida(const QString& mcu_sim, const Documento& d, const Catalogo& c);

} // namespace mcusim::build
