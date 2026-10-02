// =============================================================================
// lanzador.h — mcu-sim como proceso hijo de la ventana
//
// Fase 7 del plan. Es la mitad de lo que desactiva la objeción de los dos
// procesos (`doc/plan_dos_procesos.md` §1): el alumno no lanza dos cosas,
// lanza la ventana, y la ventana —que YA está escuchando— lanza `mcu-sim` con
// `--gui host:puerto` y el resto de sus argumentos. No hay carrera de arranque:
// cuando el hijo intenta conectarse, el escuchador lleva puesto desde antes de
// que existiera.
//
// Lo que hace, con `QProcess`:
//
//   * `lanza()`: comprueba que el ejecutable exista —con o sin `.exe`— antes
//     de intentarlo, para decir «no existe» y no un error del sistema;
//   * su salida estándar y la de error, LÍNEA A LÍNEA, por `linea()`: la
//     ventana las enseña en un panel, la de error de otro color. Es donde se
//     ve todo lo que `mcu-sim` dice y no es protocolo —el arranque, el resumen
//     de los LEDs, un `muere()`—;
//   * `termino()`, con el código y si se estrelló o lo mataron;
//   * `detiene()`: pide terminar y, si no lo hace en un plazo, lo mata.
//
// Y `argumentos_de()`, que ejecuta `mcu-sim --argumentos` y espera: es lo que
// usa el diálogo de lanzamiento para saber qué opciones ofrecer.
//
// QtCore solo: se prueba sin pantalla.
// =============================================================================
#ifndef MCU_SIM_GUI_LANZADOR_H
#define MCU_SIM_GUI_LANZADOR_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace mcusim {

class Lanzador : public QObject {
    Q_OBJECT
public:
    explicit Lanzador(QObject* padre = nullptr);
    ~Lanzador() override;

    // false si ya hay uno corriendo o si el ejecutable no existe; en el
    // segundo caso, también `fallo()`.
    bool lanza(const QString& ejecutable, const QString& directorio,
               const QStringList& argumentos);
    // Pide terminar y, pasado `plazo_ms`, mata. Espera a que acabe.
    void detiene(int plazo_ms = 2000);
    bool    corriendo() const { return p_.state() != QProcess::NotRunning; }
    qint64  pid() const { return p_.processId(); }
    QString orden() const { return orden_; }

    // El ejecutable tal y como se lanzaría: con `.exe` si hace falta. Vacío si
    // no existe ni con ni sin él.
    static QString resuelve(const QString& ejecutable);

    // `ejecutable --argumentos`, esperando como mucho `plazo_ms`. Devuelve su
    // salida estándar, o vacío con el porqué en `error`.
    static QByteArray argumentos_de(const QString& ejecutable, const QString& directorio,
                                    QString& error, int plazo_ms = 10000);

signals:
    void linea(const QString& texto, bool de_error);
    void termino(int codigo, bool estrellado);
    void fallo(const QString& por);

private:
    void lee(QProcess::ProcessChannel canal, QByteArray& resto, bool vacia);

    QProcess   p_;
    QByteArray resto_out_, resto_err_;
    QString    orden_;
};

} // namespace mcusim

#endif // MCU_SIM_GUI_LANZADOR_H
