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
// `hundido` o `ninguno`. El color del halo es el del elemento: se pinta en
// una imagen pequeña y se promedian sus píxeles, una vez, al cargar.
//
// Para todo eso la ilustración necesita observables que el panel no pinta -la
// corriente de un LED-: la ventana se suscribe a la UNIÓN de lo que pintan
// las dos vistas (`observados()`).
//
// La pestaña tiene un recuadro por placa -uno, si no es un sistema-, con su
// dibujo o, si no tiene, un hueco con «Abrir dibujo…». Mientras `mcu-sim` no
// mande los dibujos (fase 4), se abren a mano.
//
// LA ILUSTRACIÓN NO SUSTITUYE AL PANEL (decidido el 2026-10-06): si hay
// dibujo, la ventana enseña la ilustración, y el panel sigue en su pestaña,
// con todo lo que hay, para quien lo quiera.
//
// Como el panel, cada widget lleva un `objectName` para las pruebas:
// `dibujo:<placa>` el recuadro, `abrir:<placa>` su botón, `informe:<placa>`
// lo encontrado, `vista:<placa>` el dibujo, y en la escena `vivo:<pieza>`. En
// una placa suelta, <placa> es vacío: `dibujo:`.
// =============================================================================
#ifndef MCU_SIM_GUI_ILUSTRACION_H
#define MCU_SIM_GUI_ILUSTRACION_H

#include <QGraphicsView>
#include <QHash>
#include <QImage>
#include <QTimer>
#include <QWidget>

#include <memory>

#include "dibujo.h"
#include "placa.h"

class QGraphicsEllipseItem;
class QGraphicsRectItem;
class QGraphicsSimpleTextItem;
class QGraphicsSvgItem;
class QLabel;
class QSvgRenderer;
class QVBoxLayout;

namespace mcusim {

// El dibujo de UNA placa, con sus piezas vivas encima
class VistaPlaca : public QGraphicsView {
    Q_OBJECT
public:
    // nullptr, y `error` dice por qué, si el SVG no sirve
    static VistaPlaca* crea(const PlacaGui& placa, const QString& placa_id,
                            const QByteArray& svg, const QVector<EnlaceTabla>& tabla,
                            QString& error, QWidget* padre = nullptr);
    ~VistaPlaca() override;

    const InformeDibujo& informe() const { return informe_; }
    const DibujoPlaca&   dibujo() const { return *dibujo_; }
    // El elemento vivo de una pieza (su `idx`), o nullptr si no está dibujada
    QGraphicsSvgItem* vivo(int pieza) const { return vivos_.value(pieza, nullptr); }
    QGraphicsSvgItem* fondo() const { return fondo_; }
    // La escena entera en una imagen de ese ancho, con el alto que le toque.
    // Para las pruebas, y para quien quiera guardar lo que se ve.
    QImage imagen(int ancho) const;
    // Dónde cae en la imagen de ese ancho un punto del dibujo
    QPointF en_imagen(const QPointF& p, int ancho) const;

    // Fase 2. Una muestra: un id_obs que no es de una pieza dibujada se ignora
    void pon_valor(quint16 id_obs, float valor);
    // Los observables de las piezas dibujadas: lo que hay que suscribir
    QVector<quint16> observados() const;
    // El efecto de una pieza dibujada: "brillo", "hundido" o "ninguno"
    QString efecto(int pieza) const;
    // Lo que se pinta encima de cada pieza, o nullptr si no lleva
    QGraphicsEllipseItem*    halo(int pieza) const;
    QGraphicsRectItem*       contorno(int pieza) const;
    QGraphicsSimpleTextItem* etiqueta(quint16 id_obs) const { return etiquetas_.value(id_obs, nullptr); }
    // El color que se ha sacado del elemento para su halo
    QColor color(int pieza) const;

    static constexpr int PARPADEO_MS = 500;

protected:
    void resizeEvent(QResizeEvent* e) override;
    void showEvent(QShowEvent* e) override;

private:
    VistaPlaca(QWidget* padre);
    void encaja();
    // Una pieza dibujada: su elemento, lo que se le pone encima y lo que sabe
    struct Viva {
        int     pieza = -1;
        QGraphicsSvgItem* item = nullptr;
        QRectF  caja;
        QString efecto;                      // "brillo", "hundido", "ninguno"
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
        bool    hundido = false;
    };
    void prepara(const PlacaGui& placa);
    void repinta(Viva& v);
    void parpadea();

    std::unique_ptr<DibujoPlaca>  dibujo_;
    std::unique_ptr<QSvgRenderer> rend_fondo_;
    InformeDibujo                 informe_;
    QGraphicsSvgItem*             fondo_ = nullptr;
    QHash<int, QGraphicsSvgItem*> vivos_;
    QVector<Viva>                 vivas_;
    QHash<int, int>               viva_de_;     // pieza -> índice en vivas_
    QHash<quint16, int>           obs_de_;      // id_obs -> índice en vivas_
    QHash<quint16, ObservableGui> decl_;        // id_obs -> su declaración
    QHash<quint16, QGraphicsSimpleTextItem*> etiquetas_;
    QTimer                        parpadeo_;
    bool                          fase_ = true; // del parpadeo: contorno visible
};

// La pestaña: un recuadro por placa, con su dibujo o un hueco para abrirlo
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
    bool hay_dibujo() const { return !vistas_.isEmpty(); }
    // Fase 2: una muestra, a todos los dibujos; y lo que necesitan entre todos
    void pon_valor(quint16 id_obs, float valor);
    QVector<quint16> observados() const;
    VistaPlaca* vista(const QString& placa_id) const { return vistas_.value(placa_id, nullptr); }

signals:
    // Se ha pulsado «Abrir dibujo…» en el recuadro de esa placa
    void pide_dibujo(const QString& placa_id);

private:
    struct Recuadro {
        QWidget*     marco = nullptr;
        QVBoxLayout* caja = nullptr;
        QWidget*     contenido = nullptr;     // el hueco o la vista
        QLabel*      informe = nullptr;
    };
    PlacaGui                     placa_;
    QHash<QString, Recuadro>     recuadros_;
    QHash<QString, VistaPlaca*>  vistas_;
};

} // namespace mcusim

#endif // MCU_SIM_GUI_ILUSTRACION_H
