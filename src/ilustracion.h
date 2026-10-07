// =============================================================================
// ilustracion.h — cada placa con su dibujo: la pestaña «Ilustración»
//
// Fase 1 de `doc/analisis-uso-ilustraciones.md`: el dibujo SVG de una placa
// pintado en la ventana, con cada pieza que se ha encontrado en él como un
// elemento VIVO encima del fondo, y el informe de lo que se ha encontrado. Lo
// que se lee del SVG y cómo se enlaza con las piezas está en `dibujo.h`.
//
// CÓMO SE PINTA (§4 del análisis, opción A): una QGraphicsScene en las
// coordenadas del dibujo -las de su viewBox-, con
//
//   * el FONDO: el SVG SIN los elementos vivos, pintado una vez y guardado en
//     caché. Sin quitarlos, un botón hundido saldría dos veces: el del fondo,
//     quieto, y el vivo, encima;
//   * un QGraphicsSvgItem por ELEMENTO VIVO, sobre el renderer del SVG entero
//     (`setElementId`), colocado con la transformación de sus grupos: así cae
//     donde estaba, aunque lo muevan, lo escalen o lo giren.
//
// LOS OBSERVABLES SOBRE EL DIBUJO (fase 2, §8 del análisis). Como en el
// panel, sin conocer un tipo: el efecto sale de la DECLARACIÓN de los
// observables y los mandos de la pieza.
//
//   * un 0/1 sin unidad que no es alarma -`encendido`- da BRILLO: un halo del
//     color del elemento, encima de él. Su intensidad la da el primer
//     observable CON unidad de la pieza -la `corriente` de un LED-, con una
//     curva que se parezca a lo que ve el ojo: 0,3 + 0,7·√(I/Imax). Sin uno
//     así, encendido es encendido del todo;
//   * si la pieza tiene además un mando `boton`, ese 0/1 -`pulsado`- es la
//     tapa HUNDIDA: el elemento, un poco más pequeño y más apagado;
//   * un observable `alarma` a 1 -la sobrecorriente de una Fuente- es un
//     CONTORNO ROJO que parpadea dos veces por segundo alrededor del elemento;
//   * los demás numéricos que la pieza sugiere (`interesante`) son una
//     ETIQUETA debajo, con su valor y su unidad;
//   * y la ayuda emergente del elemento dice TODOS sus valores.
//
// La tabla de enlaces puede cambiar el efecto de una pieza: `brillo`,
// `hundido`, `giro`, `pantalla` o `ninguno`. El color del halo es el del elemento: se
// pinta en una imagen pequeña y se promedian sus píxeles, una vez, al cargar.
//
// `giro` solo lo pone la tabla -por declaración no sale nunca-: el elemento
// GIRA sobre el centro de su caja con el primer numérico que la pieza sugiere,
// como la marca de un mando. Ese numérico son posiciones enteras de `min` a
// `max`, y una vuelta son max - min + 1: la posición `min` es el dibujo tal
// cual, y cada una más, 360 / (max - min + 1) grados a la derecha. Es el anillo
// de un encoder (`posicion`, de 0 a 29: 12 grados por clic), y ese numérico no
// lleva etiqueta: ya lo dice el giro. Una pieza sin él se queda sin efecto, y
// se dice.
//
// `pantalla` es el de una pieza que enseña una IMAGEN -un TFT-, y es el que
// le toca por omisión (plan §30): la imagen se pinta encima del elemento,
// llenando su caja, con su luz. Si el elemento es apaisado y la imagen no -o
// al revés-, se pone girada un cuarto de vuelta a la IZQUIERDA: la fila de
// arriba de la imagen, a la izquierda; su columna izquierda, abajo. Hasta la
// primera imagen se ve el elemento tal cual.
//
// Para todo eso la ilustración necesita observables que el panel no pinta -la
// corriente de un LED-: la ventana se suscribe a la UNIÓN de lo que pintan
// las dos vistas (`observados()`).
//
// LOS MANDOS SOBRE EL DIBUJO (fase 3, §9 del análisis). El clic es para el
// PRIMER mando que declara la pieza, según su tipo, como en el panel:
//
//   * boton:       hundido mientras el ratón está abajo -el máximo-, y suelto
//                  al soltarlo -el mínimo-. CTRL+CLIC lo deja hundido hasta el
//                  siguiente Ctrl+clic: el «switch» del panel. Está hundido si
//                  lo está uno U otro, y solo se ordena cuando eso cambia; y
//                  se ve hundido al momento, sin esperar a la muestra;
//   * interruptor: cada clic lo cambia;
//   * continuo:    un clic abre encima un deslizador, con su nombre y su
//                  valor; la rueda del ratón lo mueve a pasos de un veinteavo
//                  del rango;
//   * discreto:    lo mismo, con una caja numérica, y la rueda de uno en uno.
//
// LA RUEDA ATRAVIESA: si lo que está debajo del ratón no tiene un mando
// continuo ni discreto, la rueda es para la primera pieza de debajo que sí lo
// tenga. Un mando de encoder con su pulsador en el centro son dos piezas, una
// encima de otra; el clic en la tapa aprieta el pulsador, y la rueda gira el
// encoder esté donde esté el ratón sobre el mando.
//
// Con el BOTÓN DERECHO, un menú con TODOS los mandos de la pieza: los que no
// son el primero -el rebote de un pulsador- están ahí y, como siempre, en el
// panel. Sobre una pieza con mandos el cursor es una mano. Los mandos se
// encienden y se apagan con los del panel, y las órdenes salen por la misma
// señal, así que el modelo no distingue de dónde viene una orden.
//
// Cada vista lleva su propio estado de cada mando: el «switch» del panel y el
// Ctrl+clic del dibujo no se ven el uno al otro. Lo que de verdad hay en el
// modelo lo dicen las muestras -la tapa hundida es `pulsado`-.
//
// La pestaña tiene un recuadro por placa -uno, si no es un sistema-, con su
// dibujo y su botón «Abrir dibujo…». El dibujo de una placa lo manda `mcu-sim`
// en el saludo (T_ILUSTRACION, fase 4), con la tabla de enlaces de la placa; a
// mano se abre otro, para probar uno nuevo. Y si no tiene ninguno, la ventana
// le GENERA uno con lo que sabe de ella (fase 5, `generado.h`): ninguna placa
// se queda sin ilustración, y ninguna pieza sin sitio -las que a un dibujo le
// faltan van a su BANDEJA, al lado-.
//
// LA ILUSTRACIÓN NO SUSTITUYE AL PANEL (decidido el 2026-10-06): si hay
// dibujo, la ventana enseña la ilustración, y el panel sigue en su pestaña,
// con todo lo que hay, para quien lo quiera.
//
// VARIAS PLACAS (fase 6, §10 del análisis): todas en UN dibujo, una al lado
// de otra en el orden del sistema, centradas y a la misma escala -la de sus
// milímetros-, con una LÍNEA por cada par de conectores enchufados -en una
// pila, cada uno con el siguiente-, del color de su acople, y una de trazos
// por cada hilo. Van de conector a conector, o de pin a pin si están
// dibujados; si no, desde el borde de la placa que mira a la otra. Apiladas
// -un shield encima de su Nucleo- no: lo que se pidió es una al lado de otra.
//
// Como el panel, cada widget lleva un `objectName` para las pruebas:
// `dibujo:<placa>` la fila de cada placa, `abrir:<placa>` su botón,
// `informe:<placa>` lo encontrado, `vista:` el dibujo -uno para todas-, y en
// la escena `vivo:<pieza>`. En una placa suelta, <placa> es vacío: `dibujo:`.
// =============================================================================
#ifndef MCU_SIM_GUI_ILUSTRACION_H
#define MCU_SIM_GUI_ILUSTRACION_H

#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QHash>
#include <QImage>
#include <QMenu>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <memory>

#include "dibujo.h"
#include "placa.h"

class QGraphicsEllipseItem;
class QGraphicsPathItem;
class QGraphicsRectItem;
class QGraphicsSimpleTextItem;
class QGraphicsSvgItem;
class QLabel;
class QSvgRenderer;
class QVBoxLayout;

namespace mcusim {

// Los dibujos de una o varias placas en UNA escena, con sus piezas vivas
// encima. Cada dibujo es una CAPA: el de una placa -el suyo o uno generado- y,
// si a ese le faltan piezas, su BANDEJA (fase 5). Cada capa cuelga de un item
// raíz que la coloca y la escala; dentro, todo va en las coordenadas de su
// dibujo. La escena está en las de la primera placa: con una sola capa, la
// escena ES el dibujo, como en la fase 1.
class VistaPlaca : public QGraphicsView {
    Q_OBJECT
public:
    explicit VistaPlaca(const PlacaGui& placa, QWidget* padre = nullptr);
    // Una vista con el dibujo de una placa. nullptr, y `error` dice por qué,
    // si el SVG no sirve
    static VistaPlaca* crea(const PlacaGui& placa, const QString& placa_id,
                            const QByteArray& svg, const QVector<EnlaceTabla>& tabla,
                            QString& error, QWidget* padre = nullptr);
    ~VistaPlaca() override;

    // Pone el dibujo de una placa -o su bandeja-, o lo cambia, y vuelve a
    // colocarlo todo. `origen` es "svg" o "generado". false, y `error` dice
    // por qué, si el SVG no sirve: entonces se queda el que había.
    bool pon_capa(const QString& placa_id, bool bandeja, const QByteArray& svg,
                  const QVector<EnlaceTabla>& tabla, const QString& origen, QString& error);
    void quita_capa(const QString& placa_id, bool bandeja);
    bool tiene(const QString& placa_id, bool bandeja = false) const;
    // "svg", "generado", o vacío si esa placa no está
    QString origen(const QString& placa_id) const;
    // Dónde ha quedado en la escena el dibujo de una placa, o su bandeja
    QRectF en_escena(const QString& placa_id, bool bandeja = false) const;
    // Fase 6: las LÍNEAS entre placas, una por cada par de conectores
    // enchufados -en una pila, cada uno con el siguiente- y una por cada hilo
    // que va de una placa a otra
    struct Linea {
        QString a, b;                 // "N/CN5", "S/J5"; o los dos nodos del hilo
        bool    hilo = false;
        QPointF pa, pb;               // dónde empieza y dónde acaba, en la escena
        QGraphicsPathItem* item = nullptr;
    };
    const QVector<Linea>& lineas() const { return lineas_; }
    // Dónde está en la escena lo que se llama así en su placa: `N/CN5` es el
    // elemento CN5 del dibujo de N. Si el dibujo no lo tiene, se prueba con lo
    // que hay antes del punto (`N/CN9.2` -> CN9); si tampoco, nulo.
    QRectF caja_de(const QString& ref) const;
    // Lo que se encontró en el dibujo de una placa; sin argumento, la primera
    const InformeDibujo& informe(const QString& placa_id) const;
    const InformeDibujo& informe() const;
    const DibujoPlaca&   dibujo() const;
    // El elemento vivo de una pieza (su `idx`), o nullptr si no está dibujada
    QGraphicsSvgItem* vivo(int pieza) const { return vivos_.value(pieza, nullptr); }
    QGraphicsSvgItem* fondo() const;
    // La escena entera en una imagen de ese ancho, con el alto que le toque.
    // Para las pruebas, y para quien quiera guardar lo que se ve.
    QImage imagen(int ancho) const;
    // Dónde cae en la imagen de ese ancho un punto del dibujo
    QPointF en_imagen(const QPointF& p, int ancho) const;

    // Fase 2. Una muestra: un id_obs que no es de una pieza dibujada se ignora
    void pon_valor(quint16 id_obs, float valor);
    // Una imagen (T_IMAGEN), para la pieza que la enseña
    void pon_imagen(quint16 id_obs, int ancho, int alto, const QByteArray& rgb, float brillo);
    // Lo que se pinta encima de una pieza con pantalla, o nullptr
    QGraphicsPixmapItem* pantalla(int pieza) const;
    // Los observables de las piezas dibujadas: lo que hay que suscribir
    QVector<quint16> observados() const;
    // El efecto de una pieza dibujada: "brillo", "hundido", "giro", "pantalla" o
    // "ninguno"
    QString efecto(int pieza) const;
    // Lo que se pinta encima de cada pieza, o nullptr si no lleva
    QGraphicsEllipseItem*    halo(int pieza) const;
    QGraphicsRectItem*       contorno(int pieza) const;
    QGraphicsSimpleTextItem* etiqueta(quint16 id_obs) const { return etiquetas_.value(id_obs, nullptr); }
    // El color que se ha sacado del elemento para su halo
    QColor color(int pieza) const;

    static constexpr int PARPADEO_MS = 500;

    // Fase 3. Los mandos: apagados hasta que el modelo espera o corre
    void activa_mandos(bool si);
    bool mandos_activos() const { return activos_; }
    // Dónde hay que pinchar, en la vista, para tocar una pieza
    QPoint donde(int pieza) const;
    // El menú que está abierto -el deslizador de un clic o el del botón
    // derecho-, o nullptr. Para las pruebas.
    QMenu* menu_abierto() const { return menu_.data(); }

signals:
    void orden(quint16 pieza, quint16 mando, float valor);

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void showEvent(QShowEvent* e) override;

protected:
    // Coloca las capas en la escena: las placas una al lado de otra, en el
    // orden del sistema y centradas en vertical, cada bandeja a la derecha de
    // su placa, todo a la misma escala (§10); y las líneas entre placas.
    void coloca();

    // Una capa: el dibujo de una placa, o su bandeja
    struct Capa {
        QString placa_id;
        bool    bandeja = false;
        QString origen;                          // "svg" o "generado"
        std::unique_ptr<DibujoPlaca>  dibujo;
        std::unique_ptr<QSvgRenderer> rend_fondo;
        InformeDibujo informe;
        QGraphicsRectItem* raiz = nullptr;       // todo lo suyo cuelga de aquí
        QGraphicsSvgItem*  fondo = nullptr;
        QRectF  lienzo;                          // su viewBox
        double  mm = 0;                          // mm por unidad; 0, no lo dice
    };
    Capa* capa(const QString& placa_id, bool bandeja) const;
    // Lo que mide en la escena una unidad de esa capa, con la regla de §10
    double escala(const Capa& c) const;
    // Los milímetros de una unidad de esa capa: los que dice, o los que se le
    // suponen igualando la altura
    double mm_de(const Capa& c) const;
    const Capa* primera() const;
    std::vector<std::unique_ptr<Capa>> capas_;
    PlacaGui placa_;

private:
    void encaja();
    // Una pieza dibujada: su elemento, lo que se le pone encima y lo que sabe
    struct Viva {
        Capa*   capa = nullptr;
        int     pieza = -1;
        QGraphicsSvgItem* item = nullptr;
        QRectF  caja;
        QString efecto;                      // "brillo", "hundido", "giro", "ninguno"
        QColor  color;
        int     o01 = -1;                    // el id_obs del 0/1 del efecto
        int     intensidad = -1;             // el que da la intensidad del brillo
        double  i_max = 1;
        QGraphicsEllipseItem* halo = nullptr;
        QGraphicsRectItem*    contorno = nullptr;
        QVector<quint16>      obs;           // los id_obs de la pieza, en orden
        QHash<quint16, float> valores;       // lo último de cada observable
        QString ayuda;                       // «LD4 · Led», sin los valores
        bool    alarma = false;              // alguna disparada
        bool    hundido = false;             // como se ve ahora
        bool    hundido_obs = false;         // lo que dice la muestra
        int     giro = -1;                   // el id_obs que lo hace girar
        int     imagen = -1;                 // el id_obs de su imagen, con `pantalla`
        QGraphicsPixmapItem* pixmap = nullptr;
        double  g_min = 0, g_max = 0;        // sus posiciones: una vuelta son max-min+1
        // Fase 3: sus mandos, lo último que se ha ordenado de cada uno y, de
        // los de tipo boton, el dedo y el «switch»
        QVector<MandoGui> mandos;
        QVector<float>    valor;
        QVector<char>     dedo, fijo, enviado;
    };
    void prepara(Capa& c);
    void reindexa();
    void repinta(Viva& v);
    void parpadea();
    int  viva_en(const QPoint& p) const;
    QVector<int> vivas_en(const QPoint& p) const;  // todas, la de encima primero
    void manda(Viva& w, int m, float v);
    void ordena_boton(Viva& w, int m);
    void mueve(Viva& w, int m, float v);         // continuo y discreto
    QWidget* control(Viva& w, int m, QWidget* padre);
    void abre_menu(QMenu* m, const QPoint& donde);

    QHash<int, QGraphicsSvgItem*> vivos_;
    QVector<Linea>                lineas_;
    void traza_lineas();
    QVector<Viva>                 vivas_;
    QHash<int, int>               viva_de_;     // pieza -> índice en vivas_
    QHash<quint16, int>           obs_de_;      // id_obs -> índice en vivas_
    QHash<quint16, int>           img_de_;      // id_obs de una imagen -> índice en vivas_
    QHash<quint16, ObservableGui> decl_;        // id_obs -> su declaración
    QHash<quint16, QGraphicsSimpleTextItem*> etiquetas_;
    QTimer                        parpadeo_;
    bool                          activos_ = false;
    int                           pulsada_ = -1;   // la viva con el dedo encima
    int                           mando_pulsado_ = -1;
    QPointer<QMenu>               menu_;
    bool                          fase_ = true; // del parpadeo: contorno visible
};

// La pestaña: un recuadro por placa, con su dibujo -el suyo o uno generado-
class VistaIlustracion : public QWidget {
    Q_OBJECT
public:
    explicit VistaIlustracion(const PlacaGui& placa, QWidget* padre = nullptr);

    // Los ids de las placas, en el orden del sistema; uno vacío si no lo es
    QStringList placas() const;
    // El nombre de una placa: «nucleo-f446re». Es por lo que la ventana
    // recuerda los dibujos abiertos a mano.
    QString nombre_de(const QString& placa_id) const;

    // Pone el dibujo de una placa, o lo cambia. false, y el recuadro y
    // `error` dicen por qué, si el SVG no sirve: entonces se queda el de antes.
    bool pon_dibujo(const QString& placa_id, const QByteArray& svg,
                    const QVector<EnlaceTabla>& tabla, QString& error);
    // Si alguna placa tiene un dibujo DE VERDAD -no generado-: es lo que hace
    // que la ventana enseñe primero la ilustración
    bool hay_dibujo() const;
    // "svg" o "generado"
    QString origen(const QString& placa_id) const;
    // Fase 2: una muestra, a todos los dibujos; y lo que necesitan entre todos
    void pon_valor(quint16 id_obs, float valor);
    void pon_imagen(quint16 id_obs, int ancho, int alto, const QByteArray& rgb, float brillo);
    QVector<quint16> observados() const;
    // La vista donde está esa placa: desde la fase 6, una para todas
    VistaPlaca* vista(const QString& placa_id) const
    {
        return recuadros_.contains(placa_id) ? vista_ : nullptr;
    }
    // Lo que se encontró en el dibujo de esa placa; vacío si no tiene
    InformeDibujo informe(const QString& placa_id) const;
    // Fase 3: los mandos de todos los dibujos, los de ahora y los que vengan
    void activa_mandos(bool si);

signals:
    // Se ha pulsado «Abrir dibujo…» en el recuadro de esa placa
    void pide_dibujo(const QString& placa_id);
    // Una orden de cualquiera de sus dibujos
    void orden(quint16 pieza, quint16 mando, float valor);

private:
    // La fila de cada placa, encima del dibujo: su nombre, lo que se ha
    // encontrado en su dibujo y el botón para abrir otro
    struct Recuadro {
        QWidget*     marco = nullptr;
        QLabel*      informe = nullptr;
    };
    PlacaGui                     placa_;
    QHash<QString, Recuadro>     recuadros_;
    VistaPlaca*                  vista_ = nullptr;
    bool                         activos_ = false;
};

} // namespace mcusim

#endif // MCU_SIM_GUI_ILUSTRACION_H
