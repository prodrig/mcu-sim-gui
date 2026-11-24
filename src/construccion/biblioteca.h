#pragma once
// =============================================================================
// biblioteca.h — Las dos bibliotecas de mcu-sim-build, piezas y tarjetas, y su
// caché (G9 de doc/analisis_build.md, D6)
//
// Se le dan una o más CARPETAS -la biblioteca/ de mcu-sim, sus placas/, una
// propia- y las recorre buscando lo que sabe usar:
//
//   PIEZAS     TIPO.pieza.xml (con su .forma.xml y su .svg), TIPO.mcu.xml,
//              PERFIL.conector.xml y CONJUNTO.conjunto.xml
//   TARJETAS   las placas: un .xml cuya raíz es <placa>; y los sistemas, cuya
//              raíz es <sistema>, aparte
//
// PERSISTE entre ejecuciones: el índice -por fichero, su clase, su fecha en
// ms, su tamaño, su huella y su resumen- va en un JSON, y las miniaturas en
// una carpeta de PNG. Al abrir, `refresca()` solo relee lo que cambió de
// fecha o de tamaño; `refresca(true)` lo relee todo. Un fichero que ha
// desaparecido se MARCA y no se quita hasta un refresco completo, para que
// una carpeta en un disco que no está no vacíe la biblioteca.
//
// Sin widgets: QtCore, y QtGui y QtSvg para las miniaturas.
// =============================================================================
#include <QDateTime>
#include <QImage>
#include <QString>
#include <QStringList>
#include <QVector>

namespace mcusim::build {

struct Entrada {
    enum Clase { Pieza, Mcu, Perfil, Conjunto, Tarjeta, Sistema };
    Clase   clase = Pieza;
    QString nombre;          // Led, STM32F446RE, arduino_uno_r3.digital_alto, nucleo_f446re
    QString ruta;            // el fichero principal, absoluto
    QString carpeta;         // la carpeta importada de la que sale
    QString fuente;          // relativa a `carpeta`, sin extensión: piezas/Led
    QString resumen;
    qint64  mtime = 0;       // ms
    qint64  tamano = 0;
    QString huella;          // SHA-1 del contenido
    bool    desaparecida = false;
    bool de_piezas() const { return clase <= Conjunto; }
    QString clave() const { return carpeta + QLatin1Char('|') + fuente; }
};

class Biblioteca {
public:
    // `datos`: la carpeta del índice y de las miniaturas. Vacía, la de la
    // aplicación (QStandardPaths::AppDataLocation)
    explicit Biblioteca(const QString& datos = QString());

    QString datos() const { return datos_; }
    QString ruta_indice() const;

    // Las carpetas importadas. Cambiarlas no relee: eso es refresca()
    QStringList carpetas() const { return carpetas_; }
    void pon_carpetas(const QStringList& c);
    void anade_carpeta(const QString& c);
    void quita_carpeta(const QString& c);

    // Lee el índice guardado (si lo hay) y relee lo que haya cambiado.
    // Devuelve cuántos ficheros ha tenido que leer
    int abre();
    // Recorre las carpetas: relee lo nuevo o cambiado (o todo, con
    // `completo`), marca lo desaparecido (o lo quita, con `completo`) y
    // guarda el índice. Devuelve cuántos ficheros ha leído
    int refresca(bool completo = false);
    bool guarda() const;

    const QVector<Entrada>& entradas() const { return entradas_; }
    QVector<Entrada> piezas() const;
    QVector<Entrada> tarjetas() const;
    const Entrada* busca(Entrada::Clase c, const QString& nombre) const;
    const Entrada* por_fuente(const QString& fuente) const;

    // La miniatura de una entrada, de `px` de lado como mucho, de la caché o
    // pintada y guardada en ella. Nula si no tiene dibujo
    QImage miniatura(const Entrada& e, int px = 96) const;
    // El dibujo de una entrada: el .svg de la pieza, el simple de un MCU...
    QString svg_de(const Entrada& e) const;

    // Lo que costó el último abre()/refresca(), para las pruebas
    int leidos() const { return leidos_; }
    QStringList errores() const { return errores_; }

private:
    bool lee(const QString& ruta, const QString& carpeta, Entrada& e);
    bool carga_indice();

    QString datos_;
    QStringList carpetas_;
    QVector<Entrada> entradas_;
    int leidos_ = 0;
    QStringList errores_;
};

} // namespace mcusim::build
