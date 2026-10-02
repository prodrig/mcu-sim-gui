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
// Cada campo lleva de `objectName` `arg:<nombre>` («arg:placa», «arg:--ms»...):
// así lo encuentran las pruebas.
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

private:
    void construye();
    QString valor(const QString& nombre) const;
    void actualiza();

    Configuracion       cfg_;
    QString             destino_;
    ArgumentosCli       args_;
    QString             aviso_;
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
