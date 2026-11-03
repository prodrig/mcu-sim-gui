// =============================================================================
// vista3d.h — la ventana 3D: la escena de `escena3d.h` dibujada con Qt Quick
//             3D (F8 de la CNC de mcu-sim, su doc/analisis_cnc.md §9, V3)
//
// Un QQuickWidget con `escena3d.qml`: la luz y una cámara que se mueve con el
// ratón -arrastrar gira alrededor de adónde se mira, la rueda acerca y aleja,
// arrastrar con el botón derecho la desplaza (`OrbitCameraController`)-. Cada
// sólido es un objeto de esa escena, y cada muestra que mueve uno le cambia la
// posición o el giro; cada superficie es una malla de C++ (`SuperficieGeo`)
// que se rehace, como mucho diez veces por segundo, cuando llega su imagen de
// alturas: un vértice por celda, más un borde a la altura de la base, y el
// color de la pieza en el 2D -más oscuro cuanto más hondo, la mesa donde se ha
// pasado de lado a lado, y la huella de la fresa en vacío-.
//
// Solo se compila con Qt Quick 3D (MCU_SIM_GUI_3D, CMakeLists.txt): sin él, la
// ventana dice que esta mcu-sim-gui no la lleva.
//
// Si Qt Quick 3D no se puede cargar al abrirla -falta su módulo de QML, o no
// hay OpenGL ni otra API de gráficos-, la ventana lo dice en vez de quedarse
// en negro: `error()`.
// =============================================================================
#ifndef MCU_SIM_GUI_VISTA3D_H
#define MCU_SIM_GUI_VISTA3D_H

#include <QPointer>
#include <QVector>
#include <QWidget>

class QLabel;
class QQuickWidget;
class QTimer;

namespace mcusim {

class Escena3d;
class SuperficieGeo;

class Vista3d : public QWidget {
    Q_OBJECT
public:
    explicit Vista3d(Escena3d* escena, QWidget* padre = nullptr);
    // Por qué no se ve, si no se ve; vacío si va bien
    QString error() const { return error_; }
    // Lo que se ve, en una imagen: para las pruebas
    QImage captura();
    // La cámara, como al principio
    void vista_inicial();
    QSize sizeHint() const override { return QSize(900, 650); }

private:
    void construye();
    void mueve();
    void rehaz_superficies();

    Escena3d*       esc_ = nullptr;
    QQuickWidget*   qw_ = nullptr;
    QLabel*         aviso_ = nullptr;
    QString         error_;
    QVector<QPointer<QObject>> solidos_;
    QVector<QPointer<QObject>> superficies_;
    QVector<SuperficieGeo*>    geos_;
    QVector<quint64>           hechas_;     // la versión de cada superficie ya dibujada
    QTimer*         rehacer_ = nullptr;
};

} // namespace mcusim

#endif // MCU_SIM_GUI_VISTA3D_H
