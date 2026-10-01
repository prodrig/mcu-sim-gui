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
// Desde la fase 4, con la simulación en marcha:
//
//   * se suscribe, en cuanto el modelo dice T_LISTO, a los observables que
//     pinta -los que cada pieza sugiere-, cada PERIODO_NS de tiempo simulado;
//   * pone en cada indicador el valor que llega en las instantáneas;
//   * enseña los dos relojes de T_ESTADO, y cuántas instantáneas se han
//     perdido si alguna se ha perdido: mejor decir que va por detrás que
//     mentir con un indicador que parece al día;
//   * y una lista de avisos: los de la placa, que llegan antes de arrancar, y
//     los del modelo, en marcha.
//
// Lo que acabará teniendo y aún no tiene: los mandos activos (fase 5), parar,
// pausar y el ritmo (fase 6), y lanzar el modelo ella misma (fase 7). Hoy el
// modelo se lanza a mano, desde una consola, con `--gui`.
// =============================================================================
#ifndef MCU_SIM_GUI_VENTANA_PRINCIPAL_H
#define MCU_SIM_GUI_VENTANA_PRINCIPAL_H

#include <QMainWindow>

#include "panel.h"
#include "sesion.h"

class QLabel;
class QListWidget;
class QPushButton;
class QScrollArea;

namespace mcusim {

class VentanaPrincipal : public QMainWindow {
    Q_OBJECT
public:
    explicit VentanaPrincipal(quint16 puerto = proto::PUERTO_OMISION,
                              QWidget* padre = nullptr);

    Sesion& sesion() { return ses_; }

    // Cada cuánto tiempo SIMULADO se pide una instantánea: 60 por segundo
    // simulado. Con el ritmo libre de hoy son muchas más por segundo de pared;
    // el ritmo es la fase 6.
    static constexpr quint64 PERIODO_NS = 16666667;

private:
    void espera_modelo();
    void pon_placa();
    void termina(quint32 motivo, qint32 codigo, quint64 t_sim_ns);
    void pon_aviso(quint32 nivel, quint64 t_sim_ns, const QString& origen,
                   const QString& texto);
    void pon_relojes(quint64 t_sim_ns, double t_pared_s, quint64 deltas);

    Sesion        ses_;
    quint16       puerto_;
    QLabel*       resumen_   = nullptr;
    QScrollArea*  centro_    = nullptr;
    QPushButton*  arrancar_  = nullptr;
    QPushButton*  parar_     = nullptr;
    Panel*        panel_     = nullptr;
    QLabel*       relojes_   = nullptr;
    QListWidget*  avisos_    = nullptr;
};

} // namespace mcusim

#endif // MCU_SIM_GUI_VENTANA_PRINCIPAL_H
