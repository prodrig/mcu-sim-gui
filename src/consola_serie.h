// =============================================================================
// consola_serie.h — La pestaña de un puente serie en memoria: un terminal
//
// Un PuenteSerie cuyo destino es `memoria` no abre ningún puerto, y el
// catálogo lo anuncia con `<serie destino="memoria">` (doc/protocolo.md §3).
// La ventana le da una pestaña, abajo, junto a «Avisos» y «mcu-sim», con:
//
//   * LO QUE PASA POR LA LÍNEA: lo que llega del MCU (T_SERIE) y lo que se
//     manda (T_SERIE_ENVIA), cada uno de su color. Los dos iconos de arriba
//     los eligen; por omisión, lo enviado en verde oliva y lo recibido en
//     azul. Lo imprimible sale tal cual; CR, LF o CR+LF, un salto de línea; un
//     byte que no se imprime, \xNN, y varios seguidos, juntos en un \x{...}
//     (`trama::legible`), también si llegan partidos en dos mensajes. Cuando cambia el sentido a media línea,
//     lo nuevo empieza en otra: es solo la vista, los bytes son los mismos;
//   * LA ENTRADA: ASCII con \xNN y \x{NNMM...} intercalados donde se quiera
//     (`trama_serie.h`), y debajo los bytes que saldrían, o por qué no se
//     puede. «Enviar» o Intro mandan; lo escrito se queda seleccionado, para
//     repetirlo o escribir encima;
//   * «Enviar CR», «Enviar LF» y «Enviar CRC», con la lista de los CRC
//     conocidos: el CRC se calcula sobre lo escrito y va delante de CR y LF,
//     que no entran nunca en él.
//
// Solo se puede mandar con la simulación en marcha (`activa`): antes de
// T_ARRANCA el modelo no lo lee. La pestaña no sabe de sockets: pide mandar
// con `envia()` y, si salió, la ventana se lo dice con `enviado()`, que es lo
// que se pinta.
// =============================================================================
#ifndef MCU_SIM_GUI_CONSOLA_SERIE_H
#define MCU_SIM_GUI_CONSOLA_SERIE_H

#include <QColor>
#include <QIcon>
#include <QWidget>

#include "trama_serie.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QToolButton;

namespace mcusim {

class ConsolaSerie : public QWidget {
    Q_OBJECT
public:
    // Los de por omisión: verde oliva y azul
    static QColor color_enviado_omision() { return QColor(0x80, 0x80, 0x00); }
    static QColor color_recibido_omision() { return QColor(0x00, 0x00, 0xFF); }

    ConsolaSerie(quint16 pieza, const QString& nombre, const QString& formato,
                 QWidget* padre = nullptr);

    quint16 pieza() const { return pieza_; }
    const QString& nombre() const { return nombre_; }

    // Lo que ha llegado del MCU, y lo que ha salido hacia él
    void recibido(const QByteArray& b);
    void enviado(const QByteArray& b);
    // Con la simulación en marcha se puede mandar; si no, no
    void activa(bool si);
    bool activa() const { return activa_; }

    void pon_colores(const QColor& enviado, const QColor& recibido);
    QColor color_enviado() const { return col_tx_; }
    QColor color_recibido() const { return col_rx_; }

    // La trama que saldría con lo escrito y las casillas; false, con
    // `error`, si no se puede
    bool trama(QByteArray& out, QString* error = nullptr) const;

    // Para las pruebas y para quien quiera mirar
    QString texto() const;
    quint64 bytes_recibidos() const { return n_rx_; }
    quint64 bytes_enviados() const { return n_tx_; }

    // El icono de los dos botones de color: un cuadrado del color con una
    // flecha, hacia la derecha lo enviado y hacia la izquierda lo recibido
    static QIcon icono(const QColor& c, bool enviado);

signals:
    // Se ha pulsado Enviar (o Intro) con una trama válida
    void envia(quint16 pieza, const QByteArray& bytes);
    // Se ha elegido otro color en uno de los dos iconos
    void colores(const QColor& enviado, const QColor& recibido);

private:
    void pon(const QByteArray& b, bool tx);
    void revisa();
    void manda();
    void elige_color(bool tx);
    void repinta_colores();

    quint16         pieza_;
    QString         nombre_;
    QColor          col_tx_ = color_enviado_omision();
    QColor          col_rx_ = color_recibido_omision();
    bool            activa_ = false;
    int             ultimo_ = -1;          // -1 nada, 0 recibido, 1 enviado
    bool            cr_rx_ = false, cr_tx_ = false;
    // Los bytes que no se imprimen al final de lo último pintado, y dónde
    // empieza su \x en el documento: si lo siguiente, del mismo sentido,
    // empieza igual, se repintan juntos en un solo \x{...}
    QByteArray      cola_;
    int             cola_pos_ = -1;
    quint64         n_rx_ = 0, n_tx_ = 0;
    QPlainTextEdit* vista_     = nullptr;
    QToolButton*    b_tx_      = nullptr;
    QToolButton*    b_rx_      = nullptr;
    QLabel*         cuenta_    = nullptr;
    QLineEdit*      entrada_   = nullptr;
    QPushButton*    enviar_    = nullptr;
    QCheckBox*      cr_        = nullptr;
    QCheckBox*      lf_        = nullptr;
    QCheckBox*      crc_       = nullptr;
    QComboBox*      tipo_crc_  = nullptr;
    QLabel*         previa_    = nullptr;
};

} // namespace mcusim

#endif // MCU_SIM_GUI_CONSOLA_SERIE_H
