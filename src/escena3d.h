// =============================================================================
// escena3d.h — lo que dice la vista 3D de una placa, ya resuelto: la fase F8
//              de la CNC de mcu-sim (su doc/analisis_cnc.md, §9, V3)
//
// mcu-sim manda en T_PLACA el <vista3d> de cada placa que lo declara, tal cual
// (`Vista3dGui`, placa.h): sólidos -una caja, un cilindro- y superficies -el
// mapa de alturas de una pieza-, en mm, con Z hacia arriba. Aquí se resuelve
// contra el catálogo -qué observable es cada nombre, qué imagen cada
// superficie- y se lleva el estado: con cada muestra, dónde está cada cosa; con
// cada imagen de alturas, la rejilla. No dibuja: eso es `vista3d.h`, que
// necesita Qt Quick 3D. Esto, solo QtCore: se prueba sin pantalla.
//
//   <solido forma="caja" tam="dx dy dz" en="x y z" color="#rrggbb"/>
//   <solido forma="cilindro" tam="diametro largo" eje="x|y|z" en="x y z"
//           pieza="HX" gira="angulo"/>
//   <solido pieza="CX" forma="caja" ... observable="posicion" vector="1 0 0"/>
//   <superficie pieza="PZA" imagen="alturas" tam="largo ancho" en="x y z"
//               observable="mesa" vector="0 -1 0" color="#rrggbb"/>
//
// `en` es el centro de un sólido con sus observables a cero, y la esquina de
// abajo a la izquierda de una superficie, a la altura de su base. Cada
// observable lo traslada su vector -x, y y z por unidad-, como `desplaza` en
// el dibujo, y un cilindro con `gira` gira sobre su eje tantos grados como
// diga ese observable. Lo que no casa con el catálogo -un observable que la
// pieza no tiene, una imagen que no existe- se queda quieto o sin dibujar, y
// se dice en `avisos`.
//
// LA IMAGEN DE ALTURAS (la `alturas` de una Pieza de mcu-sim): ancho x alto
// píxeles RGB888, la fila de arriba la de más Y; en cada uno, la altura en
// centésimas de mm, el byte alto en el rojo y el bajo en el verde, y en el
// azul, 255 si la fresa pasó por encima.
// =============================================================================
#ifndef MCU_SIM_GUI_ESCENA3D_H
#define MCU_SIM_GUI_ESCENA3D_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "placa.h"

namespace mcusim {

struct Vec3 {
    double x = 0, y = 0, z = 0;
};

struct Solido3d {
    bool    cilindro = false;
    Vec3    tam;                 // caja: largo, ancho, alto; cilindro: diámetro, largo, -
    char    eje = 'z';           // el de un cilindro
    Vec3    en;
    QString color;               // #rrggbb
    QString pieza;               // el nombre cualificado (`M/CX`), o vacío
    QVector<quint16> obs;        // los id_obs que lo mueven
    QVector<Vec3>    vec;        // y cuánto cada uno
    int     gira = -1;           // el id_obs de su giro, o -1
};

struct Superficie3d {
    QString pieza;
    int     id_img = -1;         // el id_obs de la imagen, o -1 si no la hay
    double  largo = 0, ancho = 0;
    Vec3    en;
    QString color;
    QVector<quint16> obs;
    QVector<Vec3>    vec;
    // La rejilla: nx x ny celdas, la fila j = 0 la de menos Y
    int     nx = 0, ny = 0;
    QVector<float>   alturas;    // mm
    QVector<quint8>  paso;       // la fresa pasó por encima
    float   maxima = 0;          // la más alta que se ha visto: el grueso
    quint64 version = 0;         // cambia con cada imagen
};

class Escena3d : public QObject {
    Q_OBJECT
public:
    explicit Escena3d(const PlacaGui& p, QObject* padre = nullptr);

    bool vacia() const { return solidos.isEmpty() && superficies.isEmpty(); }
    QVector<Solido3d>     solidos;
    QVector<Superficie3d> superficies;
    QStringList           avisos;
    bool  hay_camara = false;
    Vec3  camara, mira;          // en mm, con Z hacia arriba

    // Lo que hay que pedir: los numéricos y, si se pide, las imágenes
    QVector<quint16> observados(bool con_imagenes) const;

    void pon_valor(quint16 id, float v);
    void pon_imagen(quint16 id, int ancho, int alto, const QByteArray& rgb);

    // Dónde está ahora cada cosa, con los valores que han llegado
    Vec3   posicion(const Solido3d& s) const { return mueve(s.en, s.obs, s.vec); }
    Vec3   posicion(const Superficie3d& s) const { return mueve(s.en, s.obs, s.vec); }
    double giro(const Solido3d& s) const { return s.gira >= 0 ? valor_.value(quint16(s.gira)) : 0.0; }

signals:
    // Algo se ha movido: una muestra de un observable de la escena
    void movida();
    // Ha llegado una imagen de alturas para la superficie `i`
    void superficie(int i);

private:
    Vec3 mueve(const Vec3& en, const QVector<quint16>& obs, const QVector<Vec3>& vec) const;
    QHash<quint16, float> valor_;
    QHash<quint16, bool>  mio_;        // los id_obs numéricos de la escena
};

} // namespace mcusim

#endif // MCU_SIM_GUI_ESCENA3D_H
