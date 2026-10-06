// =============================================================================
// dibujo.h — el dibujo SVG de una placa, y qué pieza es cada elemento
//
// Fase 1 de `doc/analisis-uso-ilustraciones.md`. Aquí no hay widgets: leer el
// SVG, quitarle lo que no debe llevar, encontrar en él cada pieza de la placa
// y decir qué ha encontrado y qué no. Lo que se pinta está en `ilustracion.h`.
//
// CÓMO SE ENCUENTRA CADA PIEZA (decidido el 2026-10-06), de más a menos fuerte:
//
//   1. la TABLA DE ENLACES de la placa: `pieza` -> `elemento`. Es para los
//      dibujos que no se quieren tocar. Hoy la rellena quien llama; cuando
//      `mcu-sim` mande la tabla del XML de la placa (fase 4), saldrá de ahí;
//   2. el ID: el elemento con `id="LD2"` es la pieza `LD2`. En un sistema se
//      compara con el nombre de la pieza DENTRO de su placa (`id_local`): el
//      dibujo es de una placa, y en él no hay barras.
//
// Los atributos propios en el SVG (`mcusim:pieza`...) quedan fuera, de
// momento; si un día entran, su espacio de nombres se llama `mcusim`.
//
// Y SIEMPRE UN INFORME: qué piezas no están en el dibujo, qué entradas de la
// tabla no casan, qué ids se repiten. Sin él, arreglar un dibujo es adivinar.
//
// Lo que NO se acepta de un dibujo, porque puede llegar de otra máquina: una
// <image> que apunte fuera del propio fichero -podría leer un fichero de esta-
// se quita, y se dice; y nada de más de 8 MiB, el techo del protocolo.
// =============================================================================
#ifndef MCU_SIM_GUI_DIBUJO_H
#define MCU_SIM_GUI_DIBUJO_H

#include <QByteArray>
#include <QHash>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

#include "placa.h"

class QSvgRenderer;

namespace mcusim {

// Una línea de la tabla de enlaces: qué elemento del dibujo es una pieza, y,
// si se quiere, qué efecto le toca (fase 2).
struct EnlaceTabla {
    QString pieza;            // el nombre en la placa: "LD2", no "N/LD2"
    QString elemento;         // el id en el SVG
    QString efecto;           // "", "brillo", "hundido", "ninguno"
};

// Una pieza encontrada en el dibujo
struct EnlaceDibujo {
    int     pieza = -1;       // su `idx` en el catálogo
    QString elemento;         // el id del elemento del SVG
    QString por;              // "id" o "tabla"
    QString efecto;           // el de la tabla, si lo dice
    QRectF  caja;             // dónde está, en coordenadas del dibujo
};

struct InformeDibujo {
    QVector<EnlaceDibujo> enlaces;
    QStringList sin_elemento;  // piezas de la placa que el dibujo no trae
    QStringList tabla_rota;    // entradas de la tabla que no casan, y por qué
    QStringList repetidos;     // ids que aparecen más de una vez
    QStringList avisos;        // lo demás: imágenes quitadas, elementos girados
    // "4 piezas en el dibujo; sin dibujar: R35", en una línea
    QString resumen() const;
    // Todo, una cosa por línea, para la lista de avisos y la ayuda
    QStringList detalle() const;
};

class DibujoPlaca {
public:
    DibujoPlaca();
    ~DibujoPlaca();
    DibujoPlaca(const DibujoPlaca&) = delete;
    DibujoPlaca& operator=(const DibujoPlaca&) = delete;

    // Lee el SVG. false, y `error` dice por qué, si no es un SVG que se pueda
    // pintar. Lo que se le quite por seguridad va a `avisos()`.
    bool carga(const QByteArray& svg, QString& error);

    const QByteArray& svg() const { return svg_; }
    const QStringList& avisos() const { return avisos_; }
    // Los ids del dibujo, en el orden del fichero, sin repetir
    const QStringList& ids() const { return ids_; }
    const QStringList& repetidos() const { return repetidos_; }
    bool existe(const QString& id) const;
    // Dónde está un elemento en el dibujo, con las transformaciones de sus
    // grupos ya aplicadas: `boundsOnElement` NO las aplica, y un elemento
    // dentro de un <g transform="translate(...)"> saldría en otro sitio.
    QRectF caja(const QString& id) const;
    // ¿Lo gira o lo inclina algún grupo? Entonces `caja` es solo aproximada.
    bool girado(const QString& id) const;
    // El tamaño del dibujo (su viewBox)
    QRectF lienzo() const;
    // El renderer del dibujo ENTERO, para pintar los elementos vivos
    QSvgRenderer* renderer() const { return rend_.get(); }

    // El mismo SVG SIN esos elementos (ni lo que llevan dentro): el fondo
    // sobre el que se pintan los vivos, para que no salgan dos veces.
    QByteArray sin(const QSet<QString>& quitar) const;

    // Encuentra en el dibujo las piezas de la placa `placa_id` del sistema
    // -o todas, si `placa_id` está vacío y la placa no es un sistema-.
    InformeDibujo enlaza(const PlacaGui& placa, const QString& placa_id,
                         const QVector<EnlaceTabla>& tabla) const;

    static constexpr int TAMANO_MAX = 8 * 1024 * 1024;

private:
    QByteArray svg_;
    QStringList ids_, repetidos_, avisos_;
    std::unique_ptr<QSvgRenderer> rend_;
};

} // namespace mcusim

#endif // MCU_SIM_GUI_DIBUJO_H
