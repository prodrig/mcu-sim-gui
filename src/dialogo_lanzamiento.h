// =============================================================================
// dialogo_lanzamiento.h — con qué se lanza mcu-sim
//
// Fase 7 del plan. Se construye a partir de lo que dice `mcu-sim --argumentos`
// (`argumentos.h`): un campo por argumento posicional y uno por opción que con
// `--gui` tenga sentido, según su forma y su tipo —una casilla para una
// bandera, un desplegable para una elección, un campo de texto con su valor
// por omisión de muestra para lo demás—. Ninguna opción está escrita aquí.
//
// Y abajo, «a mano»: cualquier argumento, tal cual, para lo que el diálogo no
// sepa o no ofrezca. Debajo de todo, la línea de órdenes que va a salir, tal y
// como se ejecutará, con el `--gui` que pondrá la ventana.
//
// Si el ejecutable cambia, se le vuelven a pedir sus opciones. Si no contesta
// —no existe, o es un `mcu-sim` anterior a `--argumentos`—, el diálogo lo dice
// y deja la placa, el firmware y «a mano»: se puede lanzar igual.
//
// Arriba del todo dice de dónde salen los valores: el fichero de configuración
// que se leyó o, si no había ninguno —y entonces el diálogo sale vacío—, en qué
// sitios se buscó y dónde se guardará al lanzar.
//
// EL FIRMWARE DE CADA MCU (plan §42): con la placa escrita, se le pregunta a
// `mcu-sim placa --mcus` qué chips lleva. Con uno, la fila del firmware dice
// de cuál es y lleva al lado una casilla «sin firmware»; con varios, en vez
// de esa fila, una por chip, cada una con su fichero y su casilla. Un campo
// vacío es el firmware que diga el XML, y el campo lo enseña de muestra. Si
// `mcu-sim` no sabe contestar -uno anterior-, la fila de siempre.
//
// Cada campo lleva de `objectName` `arg:<nombre>` («arg:placa», «arg:--ms»,
// «arg:firmware:N/u0», «arg:sin-firmware:N/u0»...): así lo encuentran las
// pruebas.
// =============================================================================
#ifndef MCU_SIM_GUI_DIALOGO_LANZAMIENTO_H
#define MCU_SIM_GUI_DIALOGO_LANZAMIENTO_H

#include <QDialog>
#include <QHash>

#include "argumentos.h"
#include "configuracion.h"

class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;

namespace mcusim {

class DialogoLanzamiento : public QDialog {
    Q_OBJECT
public:
    // Con `ya` se usa esa lista de opciones y no se ejecuta nada (pruebas);
    // sin ella, se le pide a `mcu-sim` la suya al abrir.
    // `destino_gui` es lo que irá detrás de `--gui`: solo se enseña.
    DialogoLanzamiento(const Configuracion& c, const QString& destino_gui,
                       const ArgumentosCli* ya = nullptr, QWidget* padre = nullptr);

    // La configuración con lo que haya en los campos.
    Configuracion configuracion() const;
    const ArgumentosCli& argumentos() const { return args_; }
    // Lo que va a salir, sin el ejecutable ni `--gui`. Vacía si falta algo.
    QStringList linea(QString* error = nullptr) const;
    QString     aviso() const;          // por qué no hay lista de opciones, si no la hay

    // Pide otra vez las opciones al ejecutable de los campos.
    void relee();
    // Plan §42: los MCUs de la placa. `pon_mcus` los pone sin preguntar a
    // nadie (pruebas); `relee_mcus` se los pide a `mcu-sim` con la placa de
    // los campos. Los dos rehacen el diálogo sin perder lo escrito
    void pon_mcus(const QVector<McuCli>& m);
    void relee_mcus();
    const QVector<McuCli>& mcus() const { return mcus_; }

private:
    void construye();
    bool pide_mcus();
    QWidget* fila_firmware(const QString& clave, const QString& clave_sin,
                           const QString& del_xml);
    QString valor(const QString& nombre) const;
    void actualiza();

    Configuracion       cfg_;
    QString             destino_;
    ArgumentosCli       args_;
    QString             aviso_;
    QVector<McuCli>     mcus_;
    QString             aviso_mcus_;
    QLineEdit*          exe_ = nullptr;
    QLineEdit*          dir_ = nullptr;
    QLineEdit*          a_mano_ = nullptr;
    QWidget*            campos_ = nullptr;
    QFormLayout*        forma_ = nullptr;
    QLabel*             orden_ = nullptr;
    QLabel*             estado_ = nullptr;
    QPushButton*        lanzar_ = nullptr;
    QHash<QString, QWidget*> w_;
};

} // namespace mcusim

#endif // MCU_SIM_GUI_DIALOGO_LANZAMIENTO_H
