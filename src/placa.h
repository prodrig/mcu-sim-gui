// =============================================================================
// placa.h — lo que la ventana sabe de la placa: los dos XML del saludo, juntos
//
// Fase 3 del plan. `mcu-sim` manda dos XML al conectarse (`doc/protocolo.md`
// §3):
//
//   * T_PLACA, la placa DECLARADA: MCUs, componentes con su tipo, sus
//     parámetros y a qué nodo va cada patilla. Es el mismo volcado que hace
//     `--netlist`. Desde la versión 2 del protocolo puede ser un <sistema>
//     -varias placas enchufadas-, aplanado: las placas delante con su id, y
//     todo lo demás con el nombre cualificado (`N/LD2`). Una pieza sabe de
//     qué placa es por ese prefijo;
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
    // Un 0/1 que, a 1, es un AVISO -la sobrecorriente de una Fuente-: se
    // pinta para que se note. Un mcu-sim anterior no lo dice: entonces, no.
    bool    alarma = false;
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
    // En un sistema, la placa de la pieza (`N` de `N/LD2`) y su nombre dentro
    // de ella (`LD2`). Fuera de un sistema, `placa` vacía e `id_local` = `id`.
    QString placa, id_local;
    QVector<ObservableGui> observables;
    QVector<MandoGui>      mandos;
    // De T_PLACA. Una pieza que el modelo construye y la placa no declara -no
    // debería haberla- se queda sin patillas y con `en_placa` a false.
    QVector<PatillaGui>    patillas;
    bool    conectada = true;
    bool    en_placa  = false;
};

// UNA PLACA DE UN <sistema>, descrita entera: es lo que hace falta para
// DIBUJAR el sistema algún día -un rectángulo por placa, sus conectores con su
// forma, y líneas entre ellos- sin deducir nada de los prefijos. Hoy la ventana
// solo agrupa por placa y lo dice en las ayudas; el dibujo es para después.
struct ConectorGui {
    QString ref;              // "N/CN5"
    int     filas = 1, columnas = 0;
    bool    zigzag = true;    // numeracion="zigzag" (o "filas")
    int     acople = -1;      // índice en PlacaGui::acoples, o -1 si al aire
};
struct SubPlacaGui {
    QString id, nombre, fichero;
    int     n_piezas = -1;            // -1: un mcu-sim que no lo dice
    QStringList mcus;                 // "N/u0 (STM32F446RE)"
    QVector<ConectorGui> conectores;
};
// Un acople: DOS conectores enchufados, o VARIOS en pila (PC/104), con las
// placas a las que pertenece cada uno, en el mismo orden.
struct AcopleGui {
    QStringList conectores;   // "N/CN5", "S/J5"
    QStringList placas;       // "N", "S"
    bool        espejo = false;
};
struct HiloGui {
    QString a, b;             // "N/CN9.2", "S/J9.1"
    QString placa_a, placa_b;
};
// Una arista del grafo de placas: A y B están unidas, y por qué.
struct EnlaceGui {
    QString placa_a, placa_b;
    QString por;              // "N/CN5 ⇄ S/J5", o "hilo N/CN9.2 - S/J9.1"
};

struct PlacaGui {
    QString           nombre;
    // Vacías en una placa suelta: es como se sabe que esto es un sistema.
    QVector<SubPlacaGui> placas;
    QVector<AcopleGui>   acoples;
    QVector<HiloGui>     hilos;
    int                  n_hilos = 0;     // = hilos.size()
    bool es_sistema() const { return !placas.isEmpty(); }
    // El grafo de placas: una arista por cada par de placas VECINAS en un
    // acople -en una pila, cada una con la siguiente, en el orden del
    // acople- y una por cada hilo que va de una placa a otra.
    QVector<EnlaceGui> enlaces() const;
    const SubPlacaGui* subplaca(const QString& id) const;
    QStringList       mcus;   // "u0 (STM32F407VG)", "N/u0 (...)"; vacía si ninguno
    QVector<PiezaGui> piezas; // en el orden del catálogo: `idx` es su posición
    QStringList       avisos; // componentes de la placa que el catálogo no trae

    int n_observables() const;
    int n_interesantes() const;
    int n_mandos() const;
};

// Lee T_CATALOGO. false, y `error` dice por qué, si no es XML o no es un
// catálogo.
bool lee_catalogo(const QByteArray& xml, QVector<PiezaGui>& piezas, QString& error);

// Lee T_PLACA -una <placa>, o desde la versión 2 un <sistema>- y la junta con
// el catálogo ya leído.
bool junta_placa(const QByteArray& xml, const QVector<PiezaGui>& catalogo,
                 PlacaGui& placa, QString& error);

} // namespace mcusim

#endif // MCU_SIM_GUI_PLACA_H
