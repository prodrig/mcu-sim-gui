// =============================================================================
// argumentos.h — la lista de opciones de mcu-sim, leída de él mismo
//
// Fase 7 del plan (`doc/plan_dos_procesos.md`). El diálogo de lanzamiento NO
// lleva escrita la lista de opciones de `mcu-sim`: se la pide al propio
// programa con `mcu-sim --argumentos`, que la vuelca en XML (el formato está
// en `top/sim_main.cpp` de `mcu-sim`), y se construye con ella. Así hay un solo
// sitio que sabe qué opciones existen, y una nueva aparece sola en la ventana.
//
// Aquí está lo que no necesita pantalla: leer ese XML y, a partir de los
// valores que haya puesto alguien, escribir la línea de órdenes. Sin widgets,
// así que se prueba sin pantalla (`prueba_argumentos`).
//
// Los VALORES van en un mapa por nombre: "placa" y "firmware" para los
// posicionales, "--ms", "--ondas"... para las opciones. Una bandera puesta vale
// "si"; una opción vacía no se escribe —ojo: no es lo mismo que su valor por
// omisión, porque con `--gui` un `--ms` ausente quiere decir «sin fin»—; y las
// repetibles llevan sus valores separados por «;».
// =============================================================================
#ifndef MCU_SIM_GUI_ARGUMENTOS_H
#define MCU_SIM_GUI_ARGUMENTOS_H

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

namespace mcusim {

struct PosicionalCli {
    QString nombre, tipo, filtro, ayuda;
    bool    obligatorio = false;
};

struct OpcionCli {
    QString     nombre;          // "--ms"
    QString     forma;           // bandera, valor, valor_opcional, accion
    QString     tipo;            // numero, entero, texto, eleccion, fichero
    QString     omision, unidad, grupo, ejemplo, ayuda;
    QStringList valores;         // los de una eleccion
    bool        repetible = false;
    bool        con_gui   = true;
    // Lo que el diálogo ofrece: ni las acciones (--help) ni lo que con --gui no
    // tiene sentido (--gui, --tiempo-real).
    bool se_ofrece() const { return con_gui && forma != QLatin1String("accion"); }
};

struct ArgumentosCli {
    QString                programa, version;
    QVector<PosicionalCli> posicionales;
    QVector<OpcionCli>     opciones;
    bool vacio() const { return posicionales.isEmpty() && opciones.isEmpty(); }
};

using ValoresCli = QMap<QString, QString>;

// Lee la salida de `mcu-sim --argumentos`. Lo que haya antes de <argumentos>
// —la cabecera de copyright de SystemC, que sale antes de que empiece el
// programa— se salta. false, con el porqué, si no se puede leer.
bool lee_argumentos(const QByteArray& salida, ArgumentosCli& a, QString& error);

// La línea de órdenes, SIN `--gui`, que la pone quien lanza: los posicionales
// en su orden, las opciones en el orden de la lista, y detrás lo escrito a
// mano, troceado como lo trocearía una consola. Si falta un posicional
// obligatorio, `error` lo dice y la lista sale vacía. Sin lista de opciones
// —si no se pudo leer— solo van los posicionales y lo escrito a mano.
QStringList linea_de_ordenes(const ArgumentosCli& a, const ValoresCli& v,
                             const QString& a_mano, QString* error = nullptr);

// Trocea como una consola: por espacios, con comillas simples o dobles para
// lo que lleva espacios dentro.
QStringList trocea(const QString& texto);

// Para enseñarla: con comillas donde hagan falta.
QString como_texto(const QString& programa, const QStringList& args);

} // namespace mcusim

#endif // MCU_SIM_GUI_ARGUMENTOS_H
