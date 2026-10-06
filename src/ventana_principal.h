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
// Desde la fase 7, lanza el modelo ella misma:
//
//   * escucha PRIMERO, en el puerto de la configuración; si está cogido, en
//     otro que le dé el sistema, sin decir nada —es una preferencia, no una
//     exigencia—, y ese es el que le pasa al hijo en `--gui`;
//   * «Simulación ▸ Lanzar mcu-sim…» abre el diálogo de lanzamiento
//     (`dialogo_lanzamiento.h`), construido con lo que dice `mcu-sim
//     --argumentos`; al aceptarlo guarda la configuración y lanza
//     (`lanzador.h`). «Lanzar otra vez» repite lo último sin diálogo;
//   * abajo, junto a los avisos, una pestaña con la salida de `mcu-sim`, la de
//     error en rojo: el arranque, el resumen de los LEDs, un `muere()`;
//   * si `mcu-sim` termina, se estrella o lo matan desde fuera, lo dice; si se
//     cierra la ventana con él corriendo, primero le pide parar (T_PARA), y si
//     no termina en un par de segundos, lo mata. Nunca queda un hijo huérfano
//     simulando para nadie.
//
// Desde la fase 1 de las ilustraciones (`doc/analisis-uso-ilustraciones.md`),
// el dibujo de cada placa:
//
//   * en el centro, dos pestañas: «Ilustración», con el dibujo SVG de cada
//     placa -el suyo o uno generado (fase 5)-, todas juntas y unidas por
//     líneas en un sistema (fase 6), y «Panel», el de siempre. Si alguna
//     placa trae dibujo, se enseña la ilustración; el panel sigue ahí, con
//     todo;
//   * (fase 4) el dibujo de cada placa lo manda `mcu-sim` (T_ILUSTRACION), y
//     se le aplica la tabla de enlaces que la placa trae en T_PLACA;
//   * «Vista ▸ Abrir dibujo de la placa…» (o el botón de su recuadro) abre
//     otro SVG a mano. Lo que se ha encontrado en él -y lo que no- va a la
//     lista de avisos;
//   * (fase 2) los observables se ven en el dibujo -el brillo de un LED, la
//     tapa hundida, la alarma, las etiquetas-, y la suscripción es la UNIÓN
//     de lo que pintan el panel y la ilustración;
//   * (fase 3) los mandos se tocan en el dibujo: un clic, Ctrl+clic, la rueda
//     y el botón derecho, y sus órdenes salen por el mismo sitio que las del
//     panel;
//   * la ventana RECUERDA cada dibujo abierto por el nombre de la placa: al
//     volver a lanzar, o en las dos placas iguales de una pila, sale solo.
//
// Lanzarlo desde una consola con `--gui` sigue valiendo: la ventana no
// distingue quién lanzó al modelo que se le conecta.
// =============================================================================
#ifndef MCU_SIM_GUI_VENTANA_PRINCIPAL_H
#define MCU_SIM_GUI_VENTANA_PRINCIPAL_H

#include <QMainWindow>

#include "argumentos.h"
#include "configuracion.h"
#include "lanzador.h"
#include "ilustracion.h"
#include "panel.h"
#include "sesion.h"

class QComboBox;
class QLabel;
class QAction;
class QListWidget;
class QPlainTextEdit;
class QTabWidget;
class QSpinBox;
class QPushButton;
class QScrollArea;

namespace mcusim {

class VentanaPrincipal : public QMainWindow {
    Q_OBJECT
public:
    explicit VentanaPrincipal(quint16 puerto = proto::PUERTO_OMISION,
                              QWidget* padre = nullptr);
    explicit VentanaPrincipal(const Configuracion& c, QWidget* padre = nullptr);

    Sesion&   sesion() { return ses_; }
    Lanzador& lanzador() { return lanz_; }
    const Configuracion& configuracion() const { return cfg_; }
    // Lo que va detrás de `--gui` al lanzar: el host y el puerto en el que de
    // verdad se escucha.
    QString destino_gui() const;

    // Lanza `mcu-sim` con la configuración que tenga, sin diálogo. Si no
    // tiene aún su lista de opciones, se la pide. false si no se pudo, y la
    // barra de estado y la consola dicen por qué.
    bool lanza();
    // Abre el diálogo; si se acepta, guarda la configuración y lanza.
    void abre_dialogo();

    // Cada cuánto tiempo SIMULADO se pide una instantánea por omisión: 60 por
    // segundo simulado, que a tiempo real -el ritmo por omisión- son 60 por
    // segundo de pared. Con ritmo libre son muchas más, y la cola las tira si no
    // da abasto: se ve que va por detrás. La configuración lo puede cambiar
    // (`vista.periodo_ms`).
    static constexpr quint64 PERIODO_NS = 16666667;

    // Pone el dibujo de una placa -su id en el sistema, o vacío si no lo es-
    // y en las demás placas que se llamen igual, y lo recuerda por ese nombre.
    // false, con `error`, si no hay placa o el SVG no sirve.
    bool abre_dibujo(const QString& placa_id, const QByteArray& svg,
                     QString* error = nullptr);
    // Elige el fichero con un diálogo y lo abre. Con `placa_id` nulo, pregunta
    // antes de qué placa es, si hay varias.
    void elige_dibujo(const QString& placa_id = QString());

private:
    void espera_modelo();
    void pon_placa();
    void termina(quint32 motivo, qint32 codigo, quint64 t_sim_ns);
    void pon_aviso(quint32 nivel, quint64 t_sim_ns, const QString& origen,
                   const QString& texto);
    void pon_relojes(quint64 t_sim_ns, double t_pared_s, quint64 deltas);
    void pon_eco(quint64 t_sim_ns, quint16 pieza, quint16 mando, float valor,
                 quint32 resultado);
    void apaga_mandos()
    {
        if (panel_) panel_->activa_mandos(false);
        if (ilus_) ilus_->activa_mandos(false);
    }
    void ordena(quint16 pieza, quint16 mando, float v);
    void pon_controles();
    void suscribe();
    bool pon_dibujos(const QStringList& cuales, const QByteArray& svg, const QString& fichero,
                     QString* error = nullptr);
    void llega_dibujo(int i);
    void construye();
    void escucha();
    void consola(const QString& texto, const QString& color = QString());
    void hijo_termino(int codigo, bool estrellado);
    void closeEvent(QCloseEvent* e) override;

    Configuracion cfg_;
    ArgumentosCli args_;
    Sesion        ses_;
    Lanzador      lanz_;
    quint16       puerto_ = 0;
    bool          conecto_hijo_ = false;
    QTabWidget*   abajo_     = nullptr;
    QPlainTextEdit* consola_ = nullptr;
    QAction*      act_lanzar_ = nullptr;
    QAction*      act_otra_   = nullptr;
    QAction*      act_detener_ = nullptr;
    QAction*      act_encima_ = nullptr;
    QLabel*       resumen_   = nullptr;
    QScrollArea*  centro_    = nullptr;
    QPushButton*  arrancar_  = nullptr;
    QPushButton*  parar_     = nullptr;
    QComboBox*    ritmo_     = nullptr;
    QPushButton*  pausa_     = nullptr;
    QPushButton*  paso_      = nullptr;
    QSpinBox*     paso_ms_   = nullptr;
    Panel*        panel_     = nullptr;
    QTabWidget*   vistas_    = nullptr;   // «Ilustración» y «Panel»
    VistaIlustracion* ilus_  = nullptr;
    QAction*      act_dibujo_ = nullptr;
    // Los dibujos abiertos a mano, por el NOMBRE de la placa (`nucleo-f446re`)
    QHash<QString, QByteArray> dibujos_;
    QLabel*       relojes_   = nullptr;
    QListWidget*  avisos_    = nullptr;
};

} // namespace mcusim

#endif // MCU_SIM_GUI_VENTANA_PRINCIPAL_H
