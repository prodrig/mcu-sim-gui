// =============================================================================
// placa.h — lo que la ventana sabe de la placa: los dos XML del saludo, juntos
//
// Fase 3 del plan. `mcu-sim` manda dos XML al conectarse (`doc/protocolo.md`
// §3):
//
//   * T_PLACA, la placa DECLARADA: MCUs, componentes con su tipo, sus
//     parámetros y a qué nodo va cada patilla. Es el mismo volcado que hace
//     `--netlist`;
//   * T_CATALOGO, lo que cada pieza deja VER y TOCAR: sus observables y sus
//     mandos, con los índices que se usarán en el protocolo.
//
// Aquí se leen los dos y se juntan por el `id` de cada pieza, que es el mismo
// en los dos porque lo pone la placa. El resultado es todo lo que la ventana
// necesita para construirse, y NO contiene un solo tipo de C++ del modelo: un
// «Led» es una cadena, no una clase. Si mañana el simulador gana un servo,
// llega aquí como una pieza más con sus observables, sin recompilar esto.
//
// Solo QtCore: se prueba sin pantalla.
// =============================================================================
#ifndef MCU_SIM_GUI_PLACA_H
#define MCU_SIM_GUI_PLACA_H

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

namespace mcusim {

struct ObservableGui {
    int     idx = 0;          // dentro de su pieza
    int     id_obs = 0;       // global: el que viaja en T_SUSCRIBE y en las muestras
    QString nombre, unidad;
    double  min = 0, max = 0;
    bool    interesante = false;
};

struct MandoGui {
    int     idx = 0;          // dentro de su pieza: el `mando` de una Orden
    QString nombre;
    QString tipo;             // "boton", "interruptor", "continuo", "discreto"
    double  min = 0, max = 0;
    // Lo que vale en el modelo al mandar el catálogo, para que el control nazca
    // ahí y no en el mínimo. Un mcu-sim anterior no lo dice: entonces, el mínimo.
    double  valor = 0;
};

struct PatillaGui {
    QString nombre;           // "anodo", "osc_in"
    QString nodo;             // "PD12", "vdd"
};

struct PiezaGui {
    int     idx = 0;          // el `pieza` de una Orden
    QString id, tipo;
    QVector<ObservableGui> observables;
    QVector<MandoGui>      mandos;
    // De T_PLACA. Una pieza que el modelo construye y la placa no declara -no
    // debería haberla- se queda sin patillas y con `en_placa` a false.
    QVector<PatillaGui>    patillas;
    bool    conectada = true;
    bool    en_placa  = false;
};

struct PlacaGui {
    QString           nombre;
    QStringList       mcus;   // "u0 (STM32F407VG)", o solo el tipo si no tiene id
    QVector<PiezaGui> piezas; // en el orden del catálogo: `idx` es su posición
    QStringList       avisos; // componentes de la placa que el catálogo no trae

    int n_observables() const;
    int n_interesantes() const;
    int n_mandos() const;
};

// Lee T_CATALOGO. false, y `error` dice por qué, si no es XML o no es un
// catálogo.
bool lee_catalogo(const QByteArray& xml, QVector<PiezaGui>& piezas, QString& error);

// Lee T_PLACA y la junta con el catálogo ya leído.
bool junta_placa(const QByteArray& xml, const QVector<PiezaGui>& catalogo,
                 PlacaGui& placa, QString& error);

} // namespace mcusim

#endif // MCU_SIM_GUI_PLACA_H
