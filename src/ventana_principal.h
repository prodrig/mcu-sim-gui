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
// Desde la fase 5, los mandos:
//
//   * se activan con T_LISTO -lo que se toca antes de arrancar se aplica en
//     t = 0- y se apagan cuando el modelo termina o se va;
//   * cada control manda su orden al momento (`Sesion::ordena`);
//   * los ecos que no son RES_OK van a la lista de avisos: una orden a una
//     pieza o un mando que no existen lo dice aquí, y una fuera de rango la
//     dice ya el modelo con su propio T_AVISO, así que no se repite.
//
// Desde la fase 6, el control:
//
//   * el RITMO se elige antes de arrancar: tiempo real, a la mitad, libre o a
//     demanda -que arranca en pausa y solo avanza con «Paso»-;
//   * en marcha, «Pausa» / «Sigue» y «Parar». El botón de pausa dice lo que
//     dijo el último T_ESTADO, no lo que se pidió: la pausa la decide el
//     modelo;
//   * a demanda, «Paso» avanza los milisegundos que diga su casilla, y se
//     vuelve a habilitar cuando el modelo dice que está otra vez en pausa.
//
// Lo que acabará teniendo y aún no tiene: lanzar el modelo ella misma (fase 7).
// Hoy el modelo se lanza a mano, desde una consola, con `--gui`.
// =============================================================================
#ifndef MCU_SIM_GUI_VENTANA_PRINCIPAL_H
#define MCU_SIM_GUI_VENTANA_PRINCIPAL_H

#include <QMainWindow>

#include "panel.h"
#include "sesion.h"

class QComboBox;
class QLabel;
class QListWidget;
class QSpinBox;
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
    // simulado, que a tiempo real -el ritmo por omisión- son 60 por segundo de
    // pared. Con ritmo libre son muchas más, y la cola las tira si no da
    // abasto: se ve que va por detrás.
    static constexpr quint64 PERIODO_NS = 16666667;

private:
    void espera_modelo();
    void pon_placa();
    void termina(quint32 motivo, qint32 codigo, quint64 t_sim_ns);
    void pon_aviso(quint32 nivel, quint64 t_sim_ns, const QString& origen,
                   const QString& texto);
    void pon_relojes(quint64 t_sim_ns, double t_pared_s, quint64 deltas);
    void pon_eco(quint64 t_sim_ns, quint16 pieza, quint16 mando, float valor,
                 quint32 resultado);
    void apaga_mandos() { if (panel_) panel_->activa_mandos(false); }
    void pon_controles();

    Sesion        ses_;
    quint16       puerto_;
    QLabel*       resumen_   = nullptr;
    QScrollArea*  centro_    = nullptr;
    QPushButton*  arrancar_  = nullptr;
    QPushButton*  parar_     = nullptr;
    QComboBox*    ritmo_     = nullptr;
    QPushButton*  pausa_     = nullptr;
    QPushButton*  paso_      = nullptr;
    QSpinBox*     paso_ms_   = nullptr;
    Panel*        panel_     = nullptr;
    QLabel*       relojes_   = nullptr;
    QListWidget*  avisos_    = nullptr;
};

} // namespace mcusim

#endif // MCU_SIM_GUI_VENTANA_PRINCIPAL_H
