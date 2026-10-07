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
//   * y cada IMAGEN que la pieza enseña -la pantalla de un TFT- tal cual, a
//     su tamaño en píxeles (`img:<id_obs>`), con su luz (plan §30).
//
// Los indicadores dicen «—» hasta que llega la primera muestra (fase 4). Los
// controles nacen DESACTIVADOS, y `activa_mandos()` los enciende cuando el
// modelo está esperando o corriendo (fase 5). Cada widget lleva un
// `objectName` con su identificador del protocolo -`obs:<id_obs>`,
// `mando:<pieza>:<idx>`- que es como lo encuentran las pruebas.
//
// QUÉ ORDENA CADA CONTROL, también sin conocer un tipo: lo dice el TIPO del
// mando y su rango declarado.
//
//   * boton:       el máximo al hundirlo y el mínimo al soltarlo. Es un dedo:
//                  la pulsación dura lo que dura el ratón abajo. Y al lado, un
//                  «switch» (`fija:<pieza>:<idx>`) que la deja puesta hasta que
//                  se vuelva a tocar. El mando está hundido si lo está uno U
//                  otro, y solo se ordena cuando eso cambia;
//   * interruptor: el máximo marcado, el mínimo desmarcado;
//   * continuo:    la posición del deslizador, de mínimo a máximo. Al lado,
//                  su nombre y su valor (`valor:<pieza>:<idx>`);
//   * discreto:    el entero elegido en un desplegable con los enteros del
//                  rango (una caja numérica si son más de 31).
//
// Y cada control NACE DONDE ESTÁ EL MODELO: el `valor` que el catálogo da de
// cada mando. Colocarlo ahí no ordena nada.
//
// El panel no habla con el modelo: emite `orden()`, y quien lo monta decide.
//
// CÓMO SE ESCRIBE UN VALOR, sin conocer un tipo: lo decide la DECLARACIÓN del
// observable, no la pieza. Uno de 0 a 1 sin unidad es un sí o un no, y se
// pinta ● / ○; cualquier otro, el número con su unidad. Así un LED se ve
// encendido y una corriente se lee en mA, y un observable de una pieza que
// esta ventana no ha visto nunca se ve igual de bien.
//
// Y UNA ALARMA se nota: un observable que el catálogo marca `alarma="si"` -la
// sobrecorriente de una Fuente o de una Gnd- se pinta ○ en reposo y, cuando
// vale 1, «⚠ SI» en rojo y negrita, y el título de su recuadro también se pone
// en rojo. Tampoco aquí se sabe qué pieza es: lo dice la declaración.
// =============================================================================
#ifndef MCU_SIM_GUI_PANEL_H
#define MCU_SIM_GUI_PANEL_H

#include <QHash>
#include <QImage>
#include <QWidget>

#include "placa.h"

class QLabel;

namespace mcusim {

class Panel : public QWidget {
    Q_OBJECT
public:
    explicit Panel(const PlacaGui& placa, QWidget* padre = nullptr);

    // Una muestra. Un id_obs que no está pintado se ignora: la suscripción
    // puede pedir más de lo que se ve.
    void pon_valor(quint16 id_obs, float valor);

    // Una imagen (T_IMAGEN). Una que no está pintada se ignora.
    void pon_imagen(quint16 id_obs, int ancho, int alto, const QByteArray& rgb, float brillo);
    // Los píxeles RGB888 de T_IMAGEN como imagen, ya con su luz: con 0, negra
    static QImage imagen_de(int ancho, int alto, const QByteArray& rgb, float brillo);

    // Los observables que la ventana pinta, que son los que se suscriben: con
    // las imágenes, que se piden igual.
    QVector<quint16> pintados() const { return pintados_; }

    static QString texto_de(const ObservableGui& o, float valor);
    // Si esa muestra es una alarma disparada: un observable `alarma` a 1.
    static bool en_alarma(const ObservableGui& o, float valor);

    // Enciende o apaga TODOS los controles: con el modelo esperando o
    // corriendo, sí; antes del saludo o con el modelo terminado, no.
    void activa_mandos(bool si);
    bool mandos_activos() const { return activos_; }

signals:
    void orden(quint16 pieza, quint16 mando, float valor);

private:
    struct Indicador {
        QLabel*       etiqueta;
        ObservableGui obs;
        QWidget*      recuadro;          // el de su pieza, que también avisa
        bool          disparada = false; // la alarma, en la última muestra
    };
    QHash<quint16, Indicador> ind_;
    QHash<quint16, QLabel*>   img_;
    QVector<quint16>          pintados_;
    QVector<QWidget*>         controles_;
    bool                      activos_ = false;
};

Panel* construye_panel(const PlacaGui& placa, QWidget* padre = nullptr);

} // namespace mcusim

#endif // MCU_SIM_GUI_PANEL_H
