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
#include <QWidget>

#include <memory>

#include "dibujo.h"
#include "placa.h"

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

protected:
    void resizeEvent(QResizeEvent* e) override;
    void showEvent(QShowEvent* e) override;

private:
    VistaPlaca(QWidget* padre);
    void encaja();

    std::unique_ptr<DibujoPlaca>  dibujo_;
    std::unique_ptr<QSvgRenderer> rend_fondo_;
    InformeDibujo                 informe_;
    QGraphicsSvgItem*             fondo_ = nullptr;
    QHash<int, QGraphicsSvgItem*> vivos_;
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
