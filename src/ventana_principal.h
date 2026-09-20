// =============================================================================
// ventana_principal.h — la ventana, que hoy está vacía a propósito
//
// Lo que acabará teniendo, y de dónde sale cada cosa (véase
// `doc/plan_dos_procesos.md`):
//
//   * un panel por PIEZA de la placa, construido a partir del XML que manda
//     `mcu-sim` al conectarse (`T_PLACA`, que es el volcado de `--netlist`);
//   * un indicador por OBSERVABLE y un control por MANDO, construidos a partir
//     de `T_CATALOGO`. La GUI no conoce ni un tipo de C++ del modelo: si mañana
//     el simulador gana un servo, aparece aquí sin recompilar esto;
//   * los dos relojes, el simulado y el de pared, y el factor entre ellos, que
//     es lo que contesta a «¿se ha colgado?» cuando la respuesta es no;
//   * un panel de avisos, que es por donde salen los del modelo —incluidos los
//     de placa que hoy solo ve quien ejecuta `--valida`.
// =============================================================================
#ifndef MCU_SIM_GUI_VENTANA_PRINCIPAL_H
#define MCU_SIM_GUI_VENTANA_PRINCIPAL_H

#include <QMainWindow>

namespace mcusim {

class VentanaPrincipal : public QMainWindow {
    Q_OBJECT
public:
    explicit VentanaPrincipal(QWidget* padre = nullptr);
};

} // namespace mcusim

#endif // MCU_SIM_GUI_VENTANA_PRINCIPAL_H
