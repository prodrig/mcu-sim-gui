// =============================================================================
// panel_biblioteca.h — La biblioteca a la izquierda del lienzo (G9 de
// doc/analisis_build.md, D6)
//
// Dos pestañas, Piezas (piezas, MCU y perfiles de conector) y Tarjetas
// (placas y sistemas), con un buscador; cada entrada, con su miniatura -de la
// caché de la biblioteca, cargadas poco a poco para no parar la ventana- y su
// resumen al pasar por encima. Se arrastran al lienzo. «Carpetas…» dice de
// dónde se importa; «Refrescar» vuelve a mirar qué ha cambiado.
// =============================================================================
#pragma once

#include <QWidget>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QTabWidget;
class QLabel;
class QTimer;

namespace mcusim::build {

class Biblioteca;
struct Entrada;

class PanelBiblioteca : public QWidget {
    Q_OBJECT
public:
    explicit PanelBiblioteca(Biblioteca* b, QWidget* padre = nullptr);
    // Vuelve a llenar las listas desde la biblioteca
    void rellena();
    int cuantas_piezas() const;
    int cuantas_tarjetas() const;
    // La entrada de una fila (para las pruebas y el doble clic)
    const Entrada* entrada(QListWidgetItem* it) const;
    QListWidget* lista_piezas() const { return piezas_; }

signals:
    void refrescada(int leidos);
    void pedida(const mcusim::build::Entrada& e);     // doble clic: colócala
    void mensaje(const QString& t);

public slots:
    void refresca(bool completo = false);
    void carpetas();

private:
    void filtra();
    void carga_miniaturas();

    Biblioteca* b_;
    QTabWidget* tabs_;
    QListWidget* piezas_;
    QListWidget* tarjetas_;
    QLineEdit* busca_;
    QLabel* estado_;
    QTimer* reloj_;
    QList<QListWidgetItem*> pendientes_;
};

} // namespace mcusim::build
