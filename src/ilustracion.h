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
//     observable CON unidad de la pieza -la `corriente` de un LED-, en
//     ESCALA LOGARÍTMICA, que es como ve el ojo (plan §43): de Imax/2500
//     -0,01 mA de los 25 de un LED- a Imax/12,5 -2 mA-, la opacidad va de 0,3
//     a 1 con el logaritmo de la corriente (`opacidad_brillo`). Un LED azul a
//     0,15 mA se ve claramente, y a 1 mA, casi del todo: un LED de hoy, con
//     1 mA, ya se ve bien. Sin un
//     observable así, encendido es encendido del todo;
//   * si la pieza tiene además un mando `boton`, ese 0/1 -`pulsado`- es la
//     tapa HUNDIDA: el elemento, un poco más pequeño y más apagado;
//   * un observable `alarma` a 1 -la sobrecorriente de una Fuente- es un
//     CONTORNO ROJO que parpadea dos veces por segundo alrededor del elemento;
//   * los demás numéricos que la pieza sugiere (`interesante`) son una
//     ETIQUETA debajo, con su valor y su unidad;
//   * y la ayuda emergente del elemento dice TODOS sus valores.
//
// La tabla de enlaces puede cambiar el efecto de una pieza: `brillo`,
// `hundido`, `giro`, `pantalla`, `angulo` o `ninguno`. El color del halo es el
// del elemento: se pinta en una imagen pequeña y se promedian sus píxeles, una
// vez, al cargar.
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
// `angulo` es el de una pieza que sugiere un numérico EN GRADOS -la unidad
// `°`-, y el que le toca por omisión (plan §32): el elemento GIRA sobre el
// centro de su caja tantos grados como diga, a la derecha los positivos, sin
// vueltas ni posiciones. Es el aspa de un servo (`angulo`, de -90 a 90), y ese
// numérico tampoco lleva etiqueta. Pedido por la tabla en una pieza sin él,
// se queda sin efecto, y se dice.
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
// La pestaña tiene un recuadro por placa -uno, si no es un sistema-, con lo
// encontrado en su dibujo, y arriba UN botón «Abrir dibujo…» con, en un
// sistema, un desplegable de las placas a su izquierda (plan §40). El dibujo de una placa lo manda `mcu-sim`
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
// LAS LÍNEAS NACEN ESCONDIDAS (plan §33): con varias placas cableadas, todas
// a la vez tapan el dibujo. La fila de cada placa lleva un botón con un icono
// -dos placas y un cable- que enseña o esconde LAS SUYAS: una línea se ve si
// se ven las de cualquiera de sus dos placas. Una placa que no está unida a
// ninguna otra lo tiene apagado.
//
// Y SE VEN A LA PRIMERA (plan §39): ese botón dice «Conexiones» al lado del
// icono y tiene aspecto de botón -solo, el icono gris parecía apagado-;
// arriba, «Todas las conexiones» las enseña o las esconde todas a la vez; y
// al entrar en la edición sin ninguna a la vista se enseñan todas, que es
// como se editan, y al salir se vuelven a esconder si nadie las ha tocado.
//
// EL MODO DE EDICIÓN (plan §34, `doc/analisis_disposicion_ilustracion.md`):
// el botón «Edición» de la ventana, que se queda pulsado, pone el ratón a
// COLOCAR las placas en vez de tocar sus piezas. Mientras está pulsado los
// mandos no hacen nada; al soltarlo vuelven como estuvieran. Arrastrar una
// placa la mueve; un doble clic o Mayús+rueda la gira de 90 en 90 grados; el
// botón derecho tiene los dos giros, los tamaños y «colocar como al
// principio»; la rueda sola la ESCALA (plan §35). La primera
// vez que se toca una, las demás se quedan donde están. Lo que se mueve va con
// la placa: sus piezas, sus halos, la imagen de su pantalla, su bandeja y las
// líneas; las etiquetas, derechas y debajo de su pieza, y del mismo tamaño
// aunque la placa crezca.
//
// LAS LÍNEAS, EN LA EDICIÓN (plan §37): cada hilo de su color -negro una
// masa, rojo una alimentación, y los demás de una paleta-; y cualquier línea
// se puede ENRUTAR en tramos horizontales y verticales con las esquinas
// redondeadas: un doble clic en una curva la pasa a tramos rectos, uno en un
// tramo le pone un codo ahí, arrastrar un tramo lo mueve, y el botón derecho
// tiene todo eso y volver a la curva. Las líneas se tocan si se ven: las
// enseña el botón de conexiones de su placa.
//
// LO QUE SE HA COLOCADO SE RECUERDA (plan §36): al soltar «Edición», si algo
// ha cambiado, la ventana lo guarda en su configuración, por sistema -o por
// placa-, y lo pone la próxima vez que se abra el mismo. «Restablecer», en la
// edición, lo deja como al principio y lo olvida.
//
// Y EL XML TAMBIÉN LO PUEDE DECIR (plan §38): `mcu-sim` manda en T_PLACA el
// sitio y la escala de cada placa, el lienzo y las rutas que diga el XML, y
// eso es «como al principio». Lo de la configuración, si lo hay, va encima.
// «Copiar como XML» pone en el portapapeles las líneas para el XML, y
// «Guardar en el XML» -solo si la ventana lanzó `mcu-sim` y sabe dónde está
// el fichero- las escribe en él (`disposicion_xml.h`).
//
// EL LIENZO Y EL ZOOM (plan §35): sin decir nada, la vista enseña todas las
// placas, justas, como siempre. Ctrl+rueda acerca o aleja -en la edición y
// fuera de ella- y entonces hay barras para moverse; «Ajustar» vuelve a
// enseñarlo todo. El lienzo puede tener un tamaño en milímetros -«Lienzo...»,
// en la edición-: un rectángulo blanco sobre gris donde colocar las placas,
// que es lo que «Ajustar» enseña.
//
// Como el panel, cada widget lleva un `objectName` para las pruebas:
// `dibujo:<placa>` la fila de cada placa, `abrir` el botón de abrir otro
// dibujo y `dibujo_de` el desplegable de su izquierda,
// `conexiones:<placa>` el de sus líneas, `conexiones_todas` el de todas,
// `edicion` el botón del modo de
// edición, `lienzo` y `ajustar` los del lienzo y el zoom, `restablecer` el
// de volver al principio, `copiar_xml` y `guardar_xml` los del XML,
// `informe:<placa>` lo encontrado, `vista:` el dibujo -uno para todas-, y en
// la escena `vivo:<pieza>`. En una placa suelta, <placa> es vacío: `dibujo:`.
// =============================================================================
#ifndef MCU_SIM_GUI_ILUSTRACION_H
#define MCU_SIM_GUI_ILUSTRACION_H

#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QMenu>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QWidget>

#include <memory>

#include "dibujo.h"
#include "disposicion_xml.h"
#include "placa.h"

class QGraphicsEllipseItem;
class QGraphicsPathItem;
class QGraphicsRectItem;
class QGraphicsSimpleTextItem;
class QGraphicsSvgItem;
class QLabel;
class QToolButton;
class QPushButton;
class QComboBox;
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
        // Plan §37: cómo se la conoce en la disposición -«hilo N/CN9.6
        // S/P1.PWM», «acople N/CN5 S/J5»-, su color, y si va en tramos
        // rectos, sus puntos de `pa` a `pb`, codos incluidos
        QString clave;
        QColor  color;
        bool    recta = false;
        QVector<QPointF> puntos;
    };
    const QVector<Linea>& lineas() const { return lineas_; }
    // Plan §33: enseñar o esconder las líneas de una placa. Una línea se ve
    // si se ven las de cualquiera de sus dos placas; nacen escondidas, y se
    // recuerda al volver a trazarlas -un dibujo nuevo en una placa-
    void muestra_lineas(const QString& placa_id, bool si);
    bool lineas_visibles(const QString& placa_id) const { return con_lineas_.contains(placa_id); }
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
    // Plan §43: la opacidad del halo de algo encendido con `i` de `i_max`:
    // 0,3 hasta i_max/2500, 1 desde i_max/12,5, y entre medias con el
    // logaritmo
    static double opacidad_brillo(double i, double i_max);
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
    // El efecto de una pieza dibujada: "brillo", "hundido", "giro", "pantalla",
    // "angulo" o "ninguno"
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
    bool mandos_activos() const { return activos_ && !edicion_; }
    // Dónde hay que pinchar, en la vista, para tocar una pieza
    QPoint donde(int pieza) const;
    // El menú que está abierto -el deslizador de un clic o el del botón
    // derecho-, o nullptr. Para las pruebas.
    QMenu* menu_abierto() const { return menu_.data(); }

    // Plan §34: EL MODO DE EDICIÓN. El ratón coloca las placas y los mandos no
    // hacen nada; `mandos_activos()` lo dice. Al salir vuelven como estaban.
    void pon_edicion(bool si);
    bool edicion() const { return edicion_; }
    // Lo que se ha cambiado de una placa: su posición -la esquina de arriba a
    // la izquierda de su caja, ya girada, en mm de la escena- y su giro.
    // Una placa sin posición fija se coloca sola, como siempre.
    struct Ajuste {
        bool    fija = false;
        QPointF pos_mm;
        int     giro = 0;             // 0, 90, 180 o 270, a la derecha
        double  escala = 1.0;         // sobre su tamaño real (plan §35)
    };
    // Los tamaños que da la rueda, de menos a más; 1 es el tamaño real
    static const QVector<double>& escalas();
    Ajuste ajuste(const QString& placa_id) const { return ajustes_.value(placa_id); }
    // Cambian una placa y la recolocan. Girar es de 90 en 90 grados, sobre su
    // centro; mover deja la placa con esa esquina. La primera vez que se toca
    // una, las demás se quedan donde están
    void gira_placa(const QString& placa_id, int grados);
    void mueve_placa(const QString& placa_id, const QPointF& pos_mm);
    // Plan §35: la escala, sobre su centro; `pasos` sube o baja por `escalas()`
    void escala_placa(const QString& placa_id, double escala);
    void escala_pasos(const QString& placa_id, int pasos);
    // EL LIENZO, en mm de la escena. Automático -lo de siempre: las placas
    // justas- o fijo; `lienzo()` es el que hay ahora, sea cual sea
    void pon_lienzo(const QRectF& mm);
    void lienzo_automatico();
    bool lienzo_fijo() const { return lienzo_fijo_; }
    QRectF lienzo() const;
    // Lo que ocupan las placas, en mm de la escena
    QRectF caja_placas() const;
    // EL ZOOM: `factor` sobre lo que hay, alrededor de ese punto de la vista;
    // `ajusta()` vuelve a enseñarlo todo. Ajustada, la vista sigue al tamaño
    // de la ventana
    void zoom(double factor, const QPoint& centro);
    void ajusta();
    bool ajustada() const { return ajustada_; }
    // Plan §36: todo lo colocado -cada placa con algo cambiado, y el lienzo si
    // es fijo-, en JSON y en mm: {"lienzo": [x, y, ancho, alto], "placas":
    // {"N": {"x": .., "y": .., "giro": 90, "escala": 1.5}}}. Vacío si no se ha
    // tocado nada. `pon_disposicion` lo aplica: lo que no vale -un giro que no
    // es de un cuarto, una escala fuera de rango, una placa que no está- se
    // ignora
    QJsonObject disposicion() const;
    void pon_disposicion(const QJsonObject& d);
    // Como al principio: una placa -vuelve a colocarse sola- o todas
    void restablece(const QString& placa_id);
    void restablece_todas();
    // Plan §37: EL ENRUTADO de una línea en tramos horizontales y verticales.
    // `horizontal` dice el primer tramo, el que sale de `a`; `codos`, en mm,
    // la coordenada a la que llega cada tramo -una x si es horizontal, una y
    // si es vertical-, alternando. Los dos últimos tramos, hasta `b`, salen
    // solos. Sin ruta, la línea es la curva de siempre
    struct Ruta {
        bool horizontal = true;
        QVector<double> codos;
    };
    bool enrutada(const QString& clave) const { return rutas_.contains(clave); }
    Ruta ruta(const QString& clave) const { return rutas_.value(clave); }
    void pon_ruta(const QString& clave, const Ruta& r);
    // En tramos rectos, con un codo a mitad de camino
    void enruta(const QString& clave);
    // Otra vez la curva
    void desenruta(const QString& clave);
    // Un codo en el tramo más cercano a ese punto de la escena: el tramo se
    // parte en dos, que luego se separan arrastrando
    void anade_codo(const QString& clave, const QPointF& escena);
    // Mueve un tramo -1 el que sale de `a`, ... -: a esa coordenada en mm,
    // la y si es horizontal y la x si es vertical. El primero y el último
    // van pegados a sus extremos y no se mueven
    void mueve_tramo(const QString& clave, int tramo, double mm);
    void restablece_lineas();
    // La línea visible bajo ese punto de la vista, y el tramo -si va en
    // tramos rectos-: -1 si no hay
    int linea_en(const QPoint& p, int* tramo = nullptr) const;
    // El color de un hilo entre `a` y `b`, el `k`-ésimo: negro si alguno de
    // sus dos extremos es una masa, rojo si es una alimentación, y si no, de
    // una paleta que no repite en diez
    static QColor color_de_hilo(const QString& a, const QString& b, int k);
    // La placa que hay bajo ese punto de la vista -la de encima-, y si hay
    // alguna: en una placa suelta el id es vacío
    bool placa_en(const QPoint& p, QString& placa_id) const;
    // Los milímetros de una unidad de la escena
    double mm_escena() const;

signals:
    void orden(quint16 pieza, quint16 mando, float valor);
    // Plan §34: alguien ha cambiado dónde está una placa, o cómo; o el lienzo
    void disposicion_cambiada();
    // Plan §35: la vista deja de estar ajustada, o vuelve a estarlo
    void ajuste_vista(bool ajustada);

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
    void coloca(bool ajusta_vista = true);

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
        QString efecto;                      // "brillo", "hundido", "giro", "angulo"...
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
        int     giro = -1;                   // el id_obs que lo hace girar (giro y angulo)
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
    QSet<QString>                 con_lineas_;   // las placas que enseñan las suyas
    // Plan §34: la edición, lo cambiado de cada placa y el arrastre
    bool                          edicion_ = false;
    QHash<QString, Ajuste>        ajustes_;
    QHash<QString, Ruta>          rutas_;        // plan §37
    bool                          arr_linea_ = false;
    QString                       arr_clave_;
    int                           arr_tramo_ = -1;
    QStringList claves_lineas() const;
    void menu_linea(int i, const QPointF& escena, const QPoint& donde);
    bool                          hay_sel_ = false;
    QString                       sel_;          // la placa elegida
    bool                          arrastre_ = false;
    QPointF                       arr_ini_;      // dónde empezó, en la escena
    QPointF                       arr_mm_;       // la esquina de la placa entonces
    QGraphicsRectItem*            marco_sel_ = nullptr;
    // Plan §35: el lienzo fijo y el zoom
    bool                          lienzo_fijo_ = false;
    QRectF                        lienzo_mm_;
    QGraphicsRectItem*            hoja_ = nullptr;   // el lienzo fijo, en blanco
    bool                          ajustada_ = true;
    void congela();
    void suelta_mandos();
    void coloca_etiqueta(QGraphicsSimpleTextItem* t) const;
    void menu_placa(const QString& placa_id, const QPoint& donde);
    void traza_lineas();
    void aplica_lineas();
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

    // Plan §34: el modo de edición, como el botón «Edición»
    void pon_edicion(bool si);
    bool edicion() const;
    // Plan §35: el diálogo del lienzo (`dialogo_lienzo`), sin esperar a que se
    // cierre: lo que se acepte se aplica
    void abre_lienzo();
    // Plan §36: lo colocado, para la configuración. La clave es el sistema
    // con sus placas -«nucleo-f446re-servo [N=nucleo-f446re S=servo-sg90]»-
    // o «placa nombre» en una placa suelta: otro sistema, u otras placas, no
    // reciben lo que no es suyo
    static QString clave_disposicion(const PlacaGui& p);
    QString clave_disposicion() const { return clave_disposicion(placa_); }
    QJsonObject disposicion() const;
    void pon_disposicion(const QJsonObject& d);
    // Sale de la edición si se estaba en ella -y guarda, si hay algo nuevo-
    void termina_edicion();
    // Plan §38: la disposición que dice el XML, en el formato de la de
    // `disposicion()`: «como al principio»
    QJsonObject disposicion_xml() const;
    // Lo colocado, para el XML: con el giro de la ventana sumado al del XML,
    // y el giro y la escala solo de las placas en las que cambian
    DisposicionXml para_xml() const;
    // El XML del que salió la placa, si la ventana lo sabe -lanzó ella
    // `mcu-sim`-: enciende «Guardar en el XML»
    void pon_ruta_xml(const QString& ruta);
    QString ruta_xml() const { return ruta_xml_; }
    // Ya se ha escrito en el XML: lo de esta vez no se guarda además en la
    // configuración, que la próxima vez lo pondría encima del XML -con el
    // giro dos veces-
    void xml_guardado();
    bool xml_escrito() const { return xml_escrito_; }
    // Lo mismo que el botón «Copiar como XML»: el texto, y al portapapeles
    QString copia_xml();

    // Plan §33: el botón de las líneas de esa placa; nulo en una placa suelta
    QToolButton* boton_conexiones(const QString& placa_id) const
    {
        return recuadros_.value(placa_id).conexiones;
    }
    // Plan §39: las de todas las placas, como «Todas las conexiones»; y si
    // se ve alguna
    void muestra_conexiones(bool si);
    bool hay_conexiones_a_la_vista() const;
    // Plan §40: la placa del desplegable de «Abrir dibujo...» -vacía en una
    // placa suelta-, y elegir otra. false si no está
    QString placa_elegida() const;
    bool elige_placa(const QString& placa_id);

signals:
    // Se ha pulsado «Abrir dibujo…», con esa placa en el desplegable
    void pide_dibujo(const QString& placa_id);
    // Una orden de cualquiera de sus dibujos
    void orden(quint16 pieza, quint16 mando, float valor);
    // Plan §36: al salir de la edición con algo cambiado. `d` vacío: no hay
    // nada que recordar
    void guarda_disposicion(const QString& clave, const QJsonObject& d);
    // Plan §38: se ha pulsado «Guardar en el XML»; y algo que decir en la
    // barra de estado
    void pide_guardar_xml();
    void dice(const QString& texto);

private:
    // La fila de cada placa, encima del dibujo: su nombre, lo que se ha
    // encontrado en su dibujo y el botón de sus líneas
    struct Recuadro {
        QWidget*     marco = nullptr;
        QLabel*      informe = nullptr;
        QToolButton* conexiones = nullptr;   // sus líneas; nulo si no es un sistema
    };
    PlacaGui                     placa_;
    QHash<QString, Recuadro>     recuadros_;
    VistaPlaca*                  vista_ = nullptr;
    bool                         activos_ = false;
    QToolButton*                 edicion_ = nullptr;
    QToolButton*                 boton_lienzo_ = nullptr;
    QToolButton*                 ajustar_ = nullptr;
    QToolButton*                 restablecer_ = nullptr;
    QToolButton*                 copiar_xml_ = nullptr;
    QToolButton*                 guardar_xml_ = nullptr;
    QString                      ruta_xml_;
    bool                         xml_escrito_ = false;
    bool                         cambiada_ = false;   // algo nuevo desde que se entró
    QLabel*                      ayuda_edicion_ = nullptr;
    // Plan §39: «Todas las conexiones»; si las enseñó la edición y nadie las
    // ha tocado desde entonces; y si las está cambiando ella misma
    QToolButton*                 todas_ = nullptr;
    // Plan §40: «Abrir dibujo...» y, en un sistema, el desplegable de placas
    QPushButton*                 abrir_ = nullptr;
    QComboBox*                   dibujo_de_ = nullptr;
    bool                         conexiones_de_edicion_ = false;
    bool                         cambiando_conexiones_ = false;
    void sincroniza_conexiones();
};

} // namespace mcusim

#endif // MCU_SIM_GUI_ILUSTRACION_H
