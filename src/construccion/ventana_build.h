// =============================================================================
// ventana_build.h — La ventana de mcu-sim-build (G9 y G10 de
// doc/analisis_build.md)
//
// La biblioteca a la izquierda, el lienzo en medio, las propiedades a la
// derecha y los mensajes abajo: lo que dicen las redes (avisos y errores) y
// lo que dice mcu-sim al validar. Deshacer y rehacer guardan el documento
// entero: es pequeño, y así no hay orden que se olvide de deshacer algo.
//
// La configuración -la ruta de mcu-sim y la última carpeta- va en
// mcu-sim-build.json, en la carpeta de configuración del usuario; la
// biblioteca, con su índice y sus miniaturas, en la de datos.
// =============================================================================
#pragma once

#include <QMainWindow>

#include "documento.h"

class QAction;
class QPlainTextEdit;
class QUndoStack;

namespace mcusim::build {

class Biblioteca;
class Catalogo;
class Lienzo;
class PanelBiblioteca;
class PanelPropiedades;

struct ConfigBuild {
    QString ruta;              // el fichero
    QString mcu_sim;           // el ejecutable, para validar
    QString ultima_carpeta;
    static ConfigBuild lee(const QString& ruta = QString());
    bool guarda() const;
    static QString ruta_por_omision();
};

class VentanaBuild : public QMainWindow {
    Q_OBJECT
public:
    // `datos`: dónde guarda la biblioteca su índice (vacío: la carpeta de
    // datos del usuario). `config`: el fichero de configuración
    explicit VentanaBuild(const QString& datos = QString(), const QString& config = QString(),
                          QWidget* padre = nullptr);
    ~VentanaBuild() override;

    Lienzo* lienzo() const { return lienzo_; }
    Biblioteca* biblioteca() const { return bib_; }
    PanelBiblioteca* panel_biblioteca() const { return panel_bib_; }
    QUndoStack* deshacer() const { return pila_; }
    const Documento& documento() const;
    ConfigBuild& config() { return cfg_; }
    QString ruta() const { return ruta_; }
    QString mensajes() const;

    // Sin diálogos, para las pruebas y la línea de órdenes
    bool abre(const QString& ruta, QString* error = nullptr);
    // Valida (si hay mcu-sim) y escribe. Con `pregunta`, si mcu-sim dice
    // que no, pregunta si se escribe igual; sin ella, no se escribe
    bool exporta_a(const QString& ruta, bool pregunta = false);
    void nuevo();

protected:
    void closeEvent(QCloseEvent* e) override;

private slots:
    void cambiado(const mcusim::build::Documento& d, const QString& que);
    void abre_dialogo();
    void exporta_dialogo(bool como = false);
    void valida_ahora();
    void ajustes();
    void actualiza_titulo();
    void actualiza_acciones();

private:
    void pon(const Documento& d);
    void dice(const QString& t, bool error = false);
    void informa_redes();
    bool puede_cerrar();
    void carpetas_iniciales();

    ConfigBuild cfg_;
    Biblioteca* bib_;
    Catalogo* cat_;
    Lienzo* lienzo_;
    PanelBiblioteca* panel_bib_;
    PanelPropiedades* panel_prop_;
    QPlainTextEdit* log_;
    QUndoStack* pila_;
    QString ruta_;
    QAction *a_unir_curva_, *a_unir_tramos_, *a_girar_, *a_borrar_, *a_trazado_, *a_visible_, *a_ocultas_;
    bool poniendo_ = false;
};

} // namespace mcusim::build
