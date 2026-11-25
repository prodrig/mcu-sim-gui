// =============================================================================
// panel_propiedades.h — Lo elegido en el lienzo, para cambiarlo (G10 de
// doc/analisis_build.md, §4)
//
//   - una instancia: su id, su giro y su escala; en un MCU, el dibujo simple
//     o el detallado; en una pieza con una serie abierta, cuántos; y sus
//     atributos, cada uno con su valor por omisión de gris;
//   - una conexión: su trazado, si se ve, y si su red es un bus;
//   - nada: el nombre de la placa y su marco.
// Cada cambio va al lienzo como un documento nuevo (Lienzo::cambia).
// =============================================================================
#pragma once

#include <QWidget>

class QFormLayout;
class QLabel;

namespace mcusim::build {

class Lienzo;

class PanelPropiedades : public QWidget {
    Q_OBJECT
public:
    explicit PanelPropiedades(Lienzo* l, QWidget* padre = nullptr);
public slots:
    void actualiza();
private:
    void instancia(const QString& id);
    void conexion(int k);
    void placa();
    void limpia();

    Lienzo* l_;
    QWidget* cuerpo_ = nullptr;
    QFormLayout* form_ = nullptr;
    QLabel* titulo_;
};

} // namespace mcusim::build
