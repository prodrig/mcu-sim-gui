// =============================================================================
// panel.h — la pantalla de una placa, construida a partir de los dos XML
//
// Fase 3 del plan: «un panel por pieza, un indicador por observable
// interesante, un control por mando. Sin conocer un tipo.» Y es literal: aquí
// no aparece la palabra «Led». Lo que se dibuja sale de `PlacaGui`:
//
//   * un recuadro por PIEZA, con su id y su tipo como título, y sus patillas
//     con el nodo al que van. Una pieza desoldada lo dice en el título;
//   * un indicador por OBSERVABLE que la pieza sugiere (`interesante`). Los
//     demás no se pintan, pero se dice cuántos hay: sin esto la pantalla de
//     una placa con cuarenta piezas nace ilegible (`doc/protocolo.md` §3);
//   * un control por MANDO, según su tipo: un botón, una casilla o un
//     deslizador.
//
// Los indicadores dicen «—» y los controles están DESACTIVADOS: las muestras
// llegan en la fase 4 y las órdenes salen en la 5. Cada widget lleva un
// `objectName` con su identificador del protocolo -`obs:<id_obs>`,
// `mando:<pieza>:<idx>`- que es como lo encontrarán esas fases, y las pruebas.
// =============================================================================
#ifndef MCU_SIM_GUI_PANEL_H
#define MCU_SIM_GUI_PANEL_H

#include "placa.h"

class QWidget;

namespace mcusim {

QWidget* construye_panel(const PlacaGui& placa, QWidget* padre = nullptr);

} // namespace mcusim

#endif // MCU_SIM_GUI_PANEL_H
