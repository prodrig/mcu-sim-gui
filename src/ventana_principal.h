// =============================================================================
// ventana_principal.h — la ventana
//
// Desde la fase 3 del plan (`doc/plan_dos_procesos.md`) escucha al abrirse, y
// cuando un `mcu-sim --gui` se conecta:
//
//   * contesta el saludo (`Sesion`);
//   * construye un panel por PIEZA a partir de la placa y el catálogo que
//     manda el modelo (`construye_panel`), sin conocer un tipo de C++ suyo;
//   * habilita «Arrancar» cuando el modelo dice que está esperando, y «Parar».
//
// Lo que acabará teniendo y aún no tiene: los valores de los observables
// (fase 4), los mandos activos (fase 5), los dos relojes y el ritmo (fase 6),
// y lanzar el modelo ella misma (fase 7). Hoy el modelo se lanza a mano, desde
// una consola, con `--gui`.
// =============================================================================
#ifndef MCU_SIM_GUI_VENTANA_PRINCIPAL_H
#define MCU_SIM_GUI_VENTANA_PRINCIPAL_H

#include <QMainWindow>

#include "sesion.h"

class QLabel;
class QPushButton;
class QScrollArea;

namespace mcusim {

class VentanaPrincipal : public QMainWindow {
    Q_OBJECT
public:
    explicit VentanaPrincipal(quint16 puerto = proto::PUERTO_OMISION,
                              QWidget* padre = nullptr);

    Sesion& sesion() { return ses_; }

private:
    void espera_modelo();
    void pon_placa();
    void termina(quint32 motivo, qint32 codigo, quint64 t_sim_ns);

    Sesion        ses_;
    quint16       puerto_;
    QLabel*       resumen_   = nullptr;
    QScrollArea*  centro_    = nullptr;
    QPushButton*  arrancar_  = nullptr;
    QPushButton*  parar_     = nullptr;
};

} // namespace mcusim

#endif // MCU_SIM_GUI_VENTANA_PRINCIPAL_H
