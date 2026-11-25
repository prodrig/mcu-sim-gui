// =============================================================================
// lienzo.h — El lienzo de mcu-sim-build (G10 de doc/analisis_build.md, §4)
//
// Una QGraphicsScene en milímetros: cada instancia, su dibujo SVG con un
// cuadradito en cada terminal; cada conexión, una curva o unos tramos
// (trazado.h, lo mismo que la ilustración de mcu-sim-gui). El documento
// manda: el lienzo lo dibuja entero cada vez que cambia y, cuando el usuario
// hace algo, calcula el documento nuevo y lo emite con `cambiado`, que la
// ventana mete en la pila de deshacer y le devuelve con `pon_documento`.
//
// Lo que se hace aquí:
//   - arrastrar desde la biblioteca: se coloca, con un id que se pide;
//   - mover (con el ratón o las flechas), girar 90°, borrar;
//   - Ctrl+clic en terminales: se eligen, numerados; `une` los une en
//     estrella desde el primero, con curva o con tramos;
//   - en una conexión de tramos, arrastrar un tramo lo mueve y un doble clic
//     le añade un codo;
//   - el marco, a mano: se mueve y se cambia de tamaño por su esquina.
// =============================================================================
#pragma once

#include <QGraphicsView>
#include <QHash>
#include <QPointer>

#include "documento.h"
#include "redes.h"

class QGraphicsRectItem;

namespace mcusim::build {

class Catalogo;
struct Entrada;
class ItemPieza;
class ItemTerminal;
class ItemConexion;
class ItemMarco;

class Lienzo : public QGraphicsView {
    Q_OBJECT
public:
    explicit Lienzo(const Catalogo* c, QWidget* padre = nullptr);
    ~Lienzo() override;

    const Catalogo* catalogo() const { return cat_; }
    const Documento& documento() const { return doc_; }
    const Redes& redes() const { return redes_; }
    // Lo dibuja todo de nuevo, conservando lo elegido si sigue ahí
    void pon_documento(const Documento& d);

    // --- Lo elegido -------------------------------------------------------
    QStringList instancias_elegidas() const;
    int conexion_elegida() const;            // la única, o -1
    QList<int> conexiones_elegidas() const;
    void elige_instancia(const QString& id);
    // Los terminales del Ctrl+clic, en orden
    const QStringList& terminales_elegidos() const { return terminales_; }
    void elige_terminal(const QString& ref);  // lo añade, o lo quita si estaba
    void olvida_terminales();

    // --- Las acciones (cada una emite `cambiado` si hace algo) -------------
    // Coloca una pieza de la biblioteca en `centro` (mm). Con `id` vacío, se
    // pide con un diálogo; devuelve el id puesto, o vacío
    QString coloca(const Entrada& e, const QPointF& centro, QString id = QString());
    bool une(bool tramos);
    bool gira(int grados = 90);
    bool borra();
    bool alterna_trazado();
    bool alterna_visible();
    bool marco_a_mano(bool si);
    void mostrar_ocultas(bool si);
    bool muestra_ocultas() const { return ocultas_; }
    void ajusta_vista();
    void pon_zoom(double px_por_mm);
    double zoom() const;

    // Para quien sabe lo que hace (el panel de propiedades, las pruebas)
    void cambia(const Documento& d, const QString& que) { emit cambiado(d, que); }

    // Usadas por los elementos
    void pieza_movida(ItemPieza* p);
    void suelta_piezas();
    void suelta_marco();
    int orden_de(const QString& ref) const { return terminales_.indexOf(ref); }
    bool sin_conectar(const QString& ref) const { return redes_.sin_conectar.contains(ref); }
    QString ayuda_terminal(const Instancia& i, const QString& t) const;

signals:
    void cambiado(const mcusim::build::Documento& d, const QString& que);
    void seleccion_cambiada();
    void mensaje(const QString& texto);

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    void drawBackground(QPainter* p, const QRectF& r) override;

private:
    void reconstruye();
    void actualiza_lineas(const QString& id = QString());
    void actualiza_terminales();
    void actualiza_marco();
    QPointF punto_de(const QString& ref, QPointF* dir = nullptr, QRectF* caja = nullptr) const;
    ItemConexion* conexion_en(const QPointF& s) const;

    const Catalogo* cat_;
    QGraphicsScene* escena_;
    Documento doc_;
    Redes redes_;
    QHash<QString, ItemPieza*> piezas_;
    QHash<QString, ItemTerminal*> terms_;
    QVector<ItemConexion*> lineas_;
    ItemMarco* marco_ = nullptr;
    QGraphicsRectItem* automatico_ = nullptr;
    QStringList terminales_;
    bool ocultas_ = true;
    bool reconstruyendo_ = false;
    // Arrastrando un tramo
    int tramo_con_ = -1, tramo_k_ = -1;
    QVector<QPointF> tramo_puntos_;
};

// El tipo MIME de lo que se arrastra desde la biblioteca: la clave de la entrada
QString mime_entrada();

// El prefijo del id de una pieza nueva: LD para un Led, R para una
// resistencia, u para un MCU...
QString prefijo_id(const Instancia& i);

} // namespace mcusim::build
