#pragma once
// =============================================================================
// exporta_placa.h — Lo que escribe mcu-sim-build de una placa nueva (G10 de
// doc/analisis_build.md, §5.3)
//
//   NOMBRE.xml   <mcu>, los <nodo> que hacen falta, un <componente> por pieza
//                con los atributos distintos de la omisión y sus <pin>, y al
//                final el <build> (§7): el marco, cada instancia y cada
//                conexión con su trazado. mcu-sim lo comprueba y no lo manda
//                nunca a la ventana (B7)
//   NOMBRE.svg   el marco como placa y cada pieza en su sitio, girada y
//                escalada, con sus ids: «$» pasa a ser el id de la pieza
//                (LD2) y «$.anodo», LD2.anodo. SIN las conexiones (N12). En
//                unidades de 0,1 mm y SVG 1.2 Tiny, como pide el perfil de
//                autor de mcu-sim-gui
// =============================================================================
#include <QRectF>
#include <QString>

#include "catalogo.h"
#include "documento.h"
#include "redes.h"

namespace mcusim::build {

inline constexpr const char* PROGRAMA = "mcu-sim-build 0.1.0";

// La caja de una instancia en la placa, en mm (girada y escalada)
QRectF caja_instancia(const Instancia& i, const Catalogo& c);
// El marco: el del documento, o el que abarca todo con 3 mm de margen
QRectF marco_efectivo(const Documento& d, const Catalogo& c);

QString xml_placa(const Documento& d, const Catalogo& c, const Redes& r);
QString svg_placa(const Documento& d, const Catalogo& c);

// Escribe NOMBRE.xml y NOMBRE.svg (el svg junto al xml). Vacío si bien; si no,
// por qué
QString exporta(const Documento& d, const Catalogo& c, const QString& ruta_xml);

} // namespace mcusim::build
