// =============================================================================
// abre.h — Reabrir en mcu-sim-build una placa que salió de él
// (doc/analisis_build.md, §7)
//
// Lo que hay en el lienzo sale del <build>: las instancias, con su sitio, y
// las conexiones, con su trazado. Los atributos de cada pieza, de su
// <componente>, y qué redes son bus, de los <nodo bus="si">. Los <pin> y
// los demás <nodo> no se leen: se vuelven a calcular de las conexiones.
// =============================================================================
#pragma once

#include <QString>

#include "documento.h"

namespace mcusim::build {

class Catalogo;

// ¿Lleva el texto un <build>?
bool tiene_build(const QString& texto);

// El documento de una placa con <build>. Vacío, y `error`, si no se puede:
// sin <build>, o con una pieza que la biblioteca no tiene
Documento abre_build(const QString& texto, const Catalogo& c, QString* error = nullptr);

} // namespace mcusim::build
