// =============================================================================
// prueba_ilustracion.cpp — cada placa con su dibujo SVG
//
// Fases de `doc/analisis-uso-ilustraciones.md`, con la plataforma `offscreen`
// como las demás pruebas de pantalla: no abre nada en ninguna pantalla.
//
//   I1  (fase 1) leer un dibujo: lo que no es XML, lo que no es SVG, lo que
//       pesa demasiado; los ids y los repetidos; las <image> de fuera, que se
//       quitan, y las incrustadas, que no; la caja de un elemento dentro de
//       grupos movidos y escalados; el fondo sin los elementos vivos.
//   I2  (fase 1) encontrar las piezas: la tabla manda, después el id; las
//       entradas rotas de la tabla, las piezas sin elemento, el informe; en un
//       sistema, solo las de esa placa y por su nombre dentro de ella.
//   I3  (fase 1) los píxeles: cada pieza viva donde estaba -también dentro de
//       un grupo escalado, y girada si su grupo la gira- y el fondo SIN ella.
//   I4  (fase 1) la ventana: la pestaña «Ilustración» junto a «Panel», abrir
//       un dibujo a mano, el informe en los avisos, el panel intacto; en un
//       sistema, un recuadro por placa; y el dibujo, recordado al volver.
//   I5  (fase 2) los efectos, por declaración: brillo, hundido o nada, y lo
//       que dice la tabla; el color del halo; los píxeles del LED apagado,
//       encendido y encendido con poca corriente; la tapa hundida; el
//       contorno de una alarma que parpadea; las etiquetas; la ayuda.
//   I6  (fase 2) en la ventana: un dibujo abierto con el modelo esperando
//       vuelve a suscribir, con lo que el panel no pinta, y las muestras
//       llegan al dibujo.
//   I7  (fase 3) los mandos con el ratón: un boton se pulsa y se suelta, y
//       con Ctrl+clic se queda; un interruptor cambia con cada clic, también
//       con doble clic; un continuo abre su deslizador y uno discreto su caja;
//       la rueda; el menú del botón derecho con todos los mandos; el cursor;
//       y nada de eso con los mandos apagados.
//   I8  (fase 3) en la ventana: con T_LISTO se encienden, un clic en el dibujo
//       sale como T_ORDENES, y con T_FIN se apagan.
//   I9  (fase 4) el dibujo lo manda el modelo: T_ILUSTRACION leído, la tabla
//       de enlaces en T_PLACA -de una placa suelta y de las de un sistema-, y
//       la ventana poniendo cada dibujo en sus placas, con su tabla.
//   I10 (fase 5) el dibujo GENERADO de una placa que no trae el suyo: el
//       glifo de cada pieza por su declaración, los conectores con su forma y
//       su numeración, en milímetros, y que funciona como uno de verdad; la
//       BANDEJA con lo que a un dibujo le falta; la escala de cada cosa.
//   I11 (fase 6) varias placas: todas en un dibujo, una al lado de otra a la
//       misma escala y centradas, con una línea por cada par de conectores
//       enchufados -en una pila, cada uno con el siguiente- y una de trazos
//       por cada hilo, de conector a conector o, si no está dibujado, desde
//       el borde de la placa. Y las líneas NACEN ESCONDIDAS (plan §33): el
//       botón de cada placa enseña las suyas -una línea se ve si se ven las
//       de cualquiera de sus dos placas-, y se recuerda al cambiar un dibujo;
//       una placa sin uniones lo tiene apagado.
//   I13 (plan §30) una PANTALLA: la imagen del catálogo, que se pide en la
//       suscripción y llega en T_IMAGEN; el efecto `pantalla`, el de omisión
//       de una pieza con imagen, que la pinta encima de su elemento -girada a
//       la izquierda si el elemento es apaisado- y con su luz; en el panel, a
//       su tamaño; un T_IMAGEN que no mide lo que dice; y en un dibujo
//       generado, su vidrio.
//   I12 (plan §29) el mando de un encoder: el efecto `giro` -el anillo gira
//       con la posición, sobre su centro, una vuelta cada max - min + 1-; sin
//       numérico que lo mueva se dice y se queda sin efecto; y con el
//       pulsador encima, el clic es para el pulsador y la RUEDA lo atraviesa
//       y gira el encoder.
//   I14 (plan §32) el aspa de un servo: el efecto `angulo`, el de omisión de
//       una pieza que sugiere un numérico en grados -gira tantos grados como
//       diga, a la derecha los positivos, sobre el centro de su caja, que un
//       círculo sin pintar pone en el eje-; sin etiqueta; y pedido por la
//       tabla en una pieza sin grados, se dice y se queda sin efecto.
//   I15 (plan §34) el MODO DE EDICIÓN: el botón «Edición», que se queda
//       pulsado y apaga los mandos; arrastrar una placa -a milímetros
//       enteros, y las demás quietas-; girarla con doble clic, con Mayús+rueda
//       y con el menú, sobre su centro; las líneas, por el lado bueno; las
//       etiquetas, derechas bajo su pieza; colocar todas como al principio; y
//       al soltar el botón, los mandos otra vez.
// =============================================================================
#include <QApplication>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsSvgItem>
#include <cstring>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QSlider>
#include <QSpinBox>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QAction>
#include <QPushButton>
#include <QToolButton>
#include <QScrollArea>
#include <QStatusBar>
#include <QTabWidget>

#include "comun.h"
#include "dibujo.h"
#include "generado.h"
#include "ilustracion.h"
#include "placa.h"
#include "ventana_principal.h"

using namespace mcusim;
using namespace mcusim::proto;
using namespace prueba;

namespace {

// Un dibujo de la placa de prueba (`PLACA_XML`: X3, LD4, B1, R35), en un
// viewBox de 200 x 100 que mide 100 x 50 mm: dos unidades por milímetro.
//
//   * LD4, por su id, un círculo rojo;
//   * B1, por su id, un cuadrado azul DENTRO de un grupo trasladado;
//   * «cristal», amarillo, dentro de un grupo trasladado y ESCALADO por 2: es
//     X3, pero solo por la tabla de enlaces;
//   * «girado», magenta, dentro de un grupo que lo gira 45 grados;
//   * dos textos con el mismo id, una <image> que apunta a un fichero de la
//     máquina y otra incrustada.
const char* DIBUJO =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n"
    "     width=\"100mm\" height=\"50mm\" viewBox=\"0 0 200 100\">\n"
    "  <rect id=\"pcb\" x=\"0\" y=\"0\" width=\"200\" height=\"100\" fill=\"#1e5a3a\"/>\n"
    "  <text id=\"serigrafia\" x=\"4\" y=\"8\" font-size=\"6\" fill=\"#ffffff\">LD4</text>\n"
    "  <text id=\"serigrafia\" x=\"4\" y=\"96\" font-size=\"6\" fill=\"#ffffff\">B1</text>\n"
    "  <circle id=\"LD4\" cx=\"40\" cy=\"30\" r=\"8\" fill=\"#ff0000\"/>\n"
    "  <g transform=\"translate(100,40)\">\n"
    "    <rect id=\"B1\" x=\"0\" y=\"0\" width=\"20\" height=\"20\" fill=\"#0000ff\"/>\n"
    "  </g>\n"
    "  <g transform=\"translate(150,10) scale(2)\">\n"
    "    <rect id=\"cristal\" x=\"0\" y=\"0\" width=\"10\" height=\"5\" fill=\"#ffff00\"/>\n"
    "  </g>\n"
    "  <g transform=\"rotate(45 60 75)\">\n"
    "    <rect id=\"girado\" x=\"50\" y=\"65\" width=\"20\" height=\"20\" fill=\"#ff00ff\"/>\n"
    "  </g>\n"
    "  <image id=\"espia\" x=\"0\" y=\"0\" width=\"10\" height=\"10\" "
    "xlink:href=\"file:///etc/passwd\"/>\n"
    "  <image id=\"incrustada\" x=\"190\" y=\"90\" width=\"4\" height=\"4\" "
    "xlink:href=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg==\"/>\n"
    "</svg>\n";

// El de la Nucleo del sistema de prueba: LD2 y CN5, y un LD_D13 que es del
// shield y aquí no debe encontrarse
const char* DIBUJO_NUCLEO =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"70mm\" height=\"80mm\" "
    "viewBox=\"0 0 70 80\">\n"
    "  <rect x=\"0\" y=\"0\" width=\"70\" height=\"80\" fill=\"#ffffff\"/>\n"
    "  <circle id=\"LD2\" cx=\"20\" cy=\"20\" r=\"3\" fill=\"#2ecc71\"/>\n"
    "  <rect id=\"CN5\" x=\"5\" y=\"40\" width=\"40\" height=\"4\" fill=\"#222222\"/>\n"
    "  <circle id=\"LD_D13\" cx=\"50\" cy=\"20\" r=\"3\" fill=\"#ff0000\"/>\n"
    "</svg>\n";

PlacaGui placa_de(const char* placa, const char* catalogo)
{
    QVector<PiezaGui> cat;
    PlacaGui p;
    QString e;
    lee_catalogo(catalogo, cat, e);
    junta_placa(placa, cat, p, e);
    return p;
}

bool parecido(QRgb a, QRgb b, int tol = 40)
{
    return std::abs(qRed(a) - qRed(b)) <= tol && std::abs(qGreen(a) - qGreen(b)) <= tol &&
           std::abs(qBlue(a) - qBlue(b)) <= tol;
}

QString hex(QRgb c) { return QColor(c).name(); }

// El color de la imagen en un punto del DIBUJO
QRgb en(const QImage& img, const VistaPlaca& v, double x, double y)
{
    const QPointF p = v.en_imagen(QPointF(x, y), img.width());
    return img.pixel(int(p.x()), int(p.y()));
}

bool hay_aviso(QListWidget* l, const QString& trozo)
{
    for (int i = 0; i < l->count(); ++i)
        if (l->item(i)->text().contains(trozo)) return true;
    return false;
}

const QRgb VERDE_PCB = qRgb(0x1e, 0x5a, 0x3a);

int luz(QRgb c) { return qRed(c) + qGreen(c) + qBlue(c); }

// Una placa con un mando de cada tipo como PRIMERO de su pieza, y un LED sin
// mandos
const char* PLACA_MANDOS =
    "<placa nombre=\"mandos\">\n"
    "  <componente tipo=\"Button\" id=\"B1\"><pin nombre=\"pin\" nodo=\"PA0\"/></componente>\n"
    "  <componente tipo=\"Interruptor\" id=\"SW\"><pin nombre=\"a\" nodo=\"PA1\"/></componente>\n"
    "  <componente tipo=\"Pot\" id=\"POT\"><pin nombre=\"c\" nodo=\"PA2\"/></componente>\n"
    "  <componente tipo=\"Selector\" id=\"SEL\"><pin nombre=\"c\" nodo=\"PA3\"/></componente>\n"
    "  <componente tipo=\"Led\" id=\"LD\"><pin nombre=\"anodo\" nodo=\"PA4\"/></componente>\n"
    "</placa>\n";
const char* CATALOGO_MANDOS =
    "<catalogo>\n"
    "  <pieza idx=\"0\" id=\"B1\" tipo=\"Button\">\n"
    "    <observable idx=\"0\" id_obs=\"0\" nombre=\"pulsado\" unidad=\"\" min=\"0\" max=\"1\" interesante=\"si\"/>\n"
    "    <mando idx=\"0\" nombre=\"pulsar\" tipo=\"boton\" min=\"0\" max=\"1\" valor=\"0\"/>\n"
    "    <mando idx=\"1\" nombre=\"rebote_ms\" tipo=\"continuo\" min=\"0\" max=\"20\" valor=\"2\"/>\n"
    "    <mando idx=\"2\" nombre=\"rebotes\" tipo=\"discreto\" min=\"1\" max=\"9\" valor=\"5\"/>\n"
    "  </pieza>\n"
    "  <pieza idx=\"1\" id=\"SW\" tipo=\"Interruptor\">\n"
    "    <mando idx=\"0\" nombre=\"conmuta\" tipo=\"interruptor\" min=\"0\" max=\"1\" valor=\"0\"/>\n"
    "  </pieza>\n"
    "  <pieza idx=\"2\" id=\"POT\" tipo=\"Pot\">\n"
    "    <mando idx=\"0\" nombre=\"giro\" tipo=\"continuo\" min=\"0\" max=\"10\" valor=\"5\"/>\n"
    "  </pieza>\n"
    "  <pieza idx=\"3\" id=\"SEL\" tipo=\"Selector\">\n"
    "    <mando idx=\"0\" nombre=\"posicion\" tipo=\"discreto\" min=\"0\" max=\"3\" valor=\"1\"/>\n"
    "  </pieza>\n"
    "  <pieza idx=\"4\" id=\"LD\" tipo=\"Led\">\n"
    "    <observable idx=\"0\" id_obs=\"1\" nombre=\"encendido\" unidad=\"\" min=\"0\" max=\"1\" interesante=\"si\"/>\n"
    "  </pieza>\n"
    "</catalogo>\n";
const char* DIBUJO_MANDOS =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 25\">\n"
    "  <rect x=\"0\" y=\"0\" width=\"100\" height=\"25\" fill=\"#dddddd\"/>\n"
    "  <rect id=\"B1\"  x=\"5\"  y=\"5\" width=\"12\" height=\"12\" fill=\"#3050c0\"/>\n"
    "  <rect id=\"SW\"  x=\"25\" y=\"5\" width=\"12\" height=\"12\" fill=\"#30a030\"/>\n"
    "  <rect id=\"POT\" x=\"45\" y=\"5\" width=\"12\" height=\"12\" fill=\"#a03030\"/>\n"
    "  <rect id=\"SEL\" x=\"65\" y=\"5\" width=\"12\" height=\"12\" fill=\"#a0a030\"/>\n"
    "  <circle id=\"LD\" cx=\"90\" cy=\"11\" r=\"5\" fill=\"#ff0000\"/>\n"
    "</svg>\n";

// El ratón, como lo mandaría Qt: a la superficie de la vista
void raton(QGraphicsView* v, QEvent::Type t, const QPoint& p,
           Qt::KeyboardModifiers mod = Qt::NoModifier, Qt::MouseButton b = Qt::LeftButton)
{
    QMouseEvent e(t, QPointF(p), QPointF(v->viewport()->mapToGlobal(p)), b,
                  t == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::MouseButtons(b), mod);
    QCoreApplication::sendEvent(v->viewport(), &e);
}
void clic(QGraphicsView* v, const QPoint& p, Qt::KeyboardModifiers mod = Qt::NoModifier)
{
    raton(v, QEvent::MouseButtonPress, p, mod);
    raton(v, QEvent::MouseButtonRelease, p, mod);
}
void rueda(QGraphicsView* v, const QPoint& p, int muescas)
{
    QWheelEvent e(QPointF(p), QPointF(v->viewport()->mapToGlobal(p)), QPoint(),
                  QPoint(0, 120 * muescas), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase,
                  false);
    QCoreApplication::sendEvent(v->viewport(), &e);
}

struct OrdenVista { int pieza, mando; float valor; };

// Una placa sin MCU con una Fuente, como la de prueba_ventana G6, y su dibujo
const char* PLACA_FUENTE =
    "<placa nombre=\"fuente-y-masa\">\n"
    "  <componente tipo=\"Fuente\" id=\"F1\" limite_ma=\"20\">\n"
    "    <pin nombre=\"pin\" nodo=\"vcc\"/>\n"
    "  </componente>\n"
    "</placa>\n";
const char* CATALOGO_FUENTE =
    "<catalogo>\n"
    "  <pieza idx=\"0\" id=\"F1\" tipo=\"Fuente\">\n"
    "    <observable idx=\"0\" id_obs=\"0\" nombre=\"corriente\" unidad=\"mA\" "
    "min=\"-20\" max=\"20\" interesante=\"si\"/>\n"
    "    <observable idx=\"1\" id_obs=\"1\" nombre=\"sobrecorriente\" unidad=\"\" "
    "min=\"0\" max=\"1\" interesante=\"si\" alarma=\"si\"/>\n"
    "  </pieza>\n"
    "</catalogo>\n";
const char* DIBUJO_FUENTE =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 100\">\n"
    "  <rect x=\"0\" y=\"0\" width=\"100\" height=\"100\" fill=\"#ffffff\"/>\n"
    "  <rect id=\"F1\" x=\"40\" y=\"30\" width=\"20\" height=\"20\" fill=\"#808080\"/>\n"
    "</svg>\n";

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    // -------------------------------------------------------------------------
    std::printf("I1 Leer un dibujo\n");
    {
        DibujoPlaca d;
        QString e;
        bool ok = d.carga("esto no es XML <", e);
        comprueba(!ok && e.startsWith("no es XML"),
                  "lo que no es XML no se carga, y se dice: \"" + e.toStdString() + "\"");
        ok = d.carga("<html><body/></html>", e);
        comprueba(!ok && e.contains("no es un SVG"),
                  "un XML que no es SVG, tampoco: \"" + e.toStdString() + "\"");
        QByteArray enorme("<svg xmlns=\"http://www.w3.org/2000/svg\">");
        enorme += QByteArray(DibujoPlaca::TAMANO_MAX, ' ');
        enorme += "</svg>";
        ok = d.carga(enorme, e);
        comprueba(!ok && e.contains("maximo es 8"),
                  "ni lo que pasa de 8 MiB, que es el techo del protocolo: \"" +
                      e.toStdString() + "\"");

        comprueba(d.carga(DIBUJO, e), "el dibujo de prueba se carga");
        comprueba(d.ids().contains("LD4") && d.ids().contains("B1") &&
                      d.ids().count("serigrafia") == 1 &&
                      d.repetidos() == QStringList{"serigrafia"},
                  "los ids, una vez cada uno; y el repetido, aparte");
        comprueba(!d.svg().contains("passwd") && !d.ids().contains("espia") &&
                      d.avisos().size() == 1 && d.avisos()[0].contains("file:///etc/passwd"),
                  "la <image> que apunta a un fichero de la maquina se quita, y se dice");
        comprueba(d.svg().contains("data:image/png") && d.ids().contains("incrustada"),
                  "la incrustada se queda: lleva el dibujo dentro");
        comprueba(d.lienzo() == QRectF(0, 0, 200, 100), "el lienzo es el viewBox");
        comprueba(d.caja("LD4") == QRectF(32, 22, 16, 16), "la caja de LD4, tal cual");
        comprueba(d.caja("B1") == QRectF(100, 40, 20, 20),
                  "la de B1, con el traslado de su grupo, que boundsOnElement no aplica");
        comprueba(d.caja("cristal") == QRectF(150, 10, 20, 10),
                  "y la del cristal, trasladada Y escalada");
        comprueba(!d.girado("B1") && d.girado("girado"), "se sabe cual esta girado");
        {
            // Lo que manda mcu-sim con `giro="90"` y `giro="180"`: un grupo
            // que gira un cuarto -o media- vuelta. Eso no es «girado»: la caja
            // es exacta
            DibujoPlaca q;
            QString eq;
            const bool cargado = q.carga(
                "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 200\">\n"
                "<g transform=\"translate(100 0) rotate(90)\">\n"
                "  <rect id=\"P\" x=\"10\" y=\"20\" width=\"40\" height=\"10\"/>\n"
                "</g>\n"
                "<g transform=\"translate(100 200) rotate(180)\">\n"
                "  <rect id=\"Q\" x=\"10\" y=\"20\" width=\"40\" height=\"10\"/>\n"
                "</g>\n</svg>\n", eq);
            const QRectF cp = q.caja("P"), cq = q.caja("Q");
            comprueba(cargado && !q.girado("P") && !q.girado("Q") &&
                          std::abs(cp.x() - 70) < 1e-6 && std::abs(cp.y() - 10) < 1e-6 &&
                          std::abs(cp.width() - 10) < 1e-6 && std::abs(cp.height() - 40) < 1e-6 &&
                          std::abs(cq.x() - 50) < 1e-6 && std::abs(cq.y() - 170) < 1e-6,
                      "un cuarto o media vuelta -una placa montada con giro=- no es girar: la "
                      "caja es exacta, y no se avisa");
        }
        comprueba(d.caja("nada").isNull() && !d.existe("nada"), "lo que no existe, no esta");
        const QByteArray fondo = d.sin({"B1", "LD4"});
        comprueba(!fondo.contains("id=\"B1\"") && !fondo.contains("id=\"LD4\"") &&
                      fondo.contains("id=\"cristal\"") && fondo.contains("translate(100,40)"),
                  "el fondo, sin esos elementos -y nada mas: su grupo se queda-");
    }

    // -------------------------------------------------------------------------
    std::printf("I2 Encontrar las piezas: la tabla, y despues el id\n");
    {
        const PlacaGui p = placa_de(PLACA_XML, CATALOGO_XML);
        DibujoPlaca d;
        QString e;
        d.carga(DIBUJO, e);

        InformeDibujo i = d.enlaza(p, QString(), {});
        comprueba(i.enlaces.size() == 2 && i.enlaces[0].pieza == 1 &&
                      i.enlaces[0].elemento == "LD4" && i.enlaces[0].por == "id" &&
                      i.enlaces[1].pieza == 2 && i.enlaces[1].caja == QRectF(100, 40, 20, 20),
                  "sin tabla, por el id: LD4 y B1, en el orden de la placa, con su caja");
        comprueba(i.sin_elemento == QStringList({"X3", "R35"}),
                  "X3 y R35 no estan en el dibujo, y se dice");
        comprueba(i.resumen() == "2 de 4 piezas en el dibujo; sin dibujar: X3, R35; 2 aviso(s)",
                  "el resumen, en una linea: \"" + i.resumen().toStdString() + "\"");
        const QStringList det = i.detalle();
        comprueba(det.size() == 3 && det[0].contains("X3, R35") &&
                      det[1].contains("repetidos") && det[1].contains("serigrafia") &&
                      det[2].contains("/etc/passwd"),
                  "y el detalle, una cosa por linea");

        i = d.enlaza(p, QString(),
                     {{"X3", "cristal", "brillo"}, {"LD9", "LD4", ""}, {"B1", "tapa", ""},
                      {"X3", "LD4", ""}});
        comprueba(i.enlaces.size() == 3 && i.enlaces[0].pieza == 0 &&
                      i.enlaces[0].elemento == "cristal" && i.enlaces[0].por == "tabla" &&
                      i.enlaces[0].efecto == "brillo" &&
                      i.enlaces[0].caja == QRectF(150, 10, 20, 10),
                  "la tabla encuentra lo que el id no: X3 es el cristal, con su efecto");
        comprueba(i.tabla_rota.size() == 3 && i.tabla_rota[0].startsWith("LD9") &&
                      i.tabla_rota[0].contains("no tiene esa pieza") &&
                      i.tabla_rota[1].startsWith("B1") &&
                      i.tabla_rota[1].contains("no tiene ese elemento") &&
                      i.tabla_rota[2].contains("ya estaba"),
                  "las entradas rotas se dicen: una pieza que no hay, un elemento que no "
                  "hay, una pieza dos veces");
        comprueba(i.enlaces[2].pieza == 2 && i.enlaces[2].por == "id",
                  "y si la tabla se equivoca de elemento, la pieza se busca por su id");
        comprueba(i.sin_elemento == QStringList{"R35"}, "solo falta R35");

        i = d.enlaza(p, QString(), {{"R35", "girado", ""}});
        comprueba(i.avisos.size() == 2 && i.avisos[1].contains("R35 está girado"),
                  "un elemento girado se dice: sus efectos iran a su caja");

        const PlacaGui s = placa_de(SISTEMA_XML, CATALOGO_SISTEMA_XML);
        DibujoPlaca dn;
        dn.carga(DIBUJO_NUCLEO, e);
        i = dn.enlaza(s, "N", {});
        comprueba(i.enlaces.size() == 2 && i.enlaces[0].pieza == 0 &&
                      i.enlaces[0].elemento == "LD2" && i.enlaces[1].pieza == 1 &&
                      i.sin_elemento.isEmpty(),
                  "en un sistema, las piezas de ESA placa, por su nombre en ella: N/LD2 es "
                  "LD2, N/CN5 es CN5");
        i = dn.enlaza(s, "S", {});
        comprueba(i.enlaces.size() == 1 && i.enlaces[0].pieza == 3 &&
                      i.sin_elemento == QStringList{"J5"},
                  "y el mismo dibujo, para S, encuentra las de S: su LD_D13 -y no el LD2 "
                  "de N, que tambien esta dibujado-");
    }

    // -------------------------------------------------------------------------
    std::printf("I3 Los pixeles: cada pieza donde estaba, y el fondo sin ella\n");
    {
        const PlacaGui p = placa_de(PLACA_XML, CATALOGO_XML);
        QString e;
        VistaPlaca* v = VistaPlaca::crea(p, QString(), DIBUJO, {{"X3", "cristal", ""},
                                                                {"R35", "girado", ""}}, e);
        comprueba(v != nullptr, "la vista se construye " + e.toStdString());
        if (!v) return resultado();
        comprueba(v->vivo(0) && v->vivo(1) && v->vivo(2) && v->vivo(3) &&
                      v->vivo(1)->objectName() == "vivo:1" &&
                      v->vivo(1)->toolTip() == QString::fromUtf8("LD4 · Led") &&
                      v->vivo(0)->toolTip().contains("cristal"),
                  "un elemento vivo por pieza encontrada, con su ayuda");
        const QImage img = v->imagen(400);
        comprueba(img.size() == QSize(400, 200), "la imagen, con el alto del dibujo");
        comprueba(parecido(en(img, *v, 40, 30), qRgb(255, 0, 0)) &&
                      parecido(en(img, *v, 110, 50), qRgb(0, 0, 255)),
                  "LD4 rojo y B1 azul, cada uno en su sitio: " + hex(en(img, *v, 40, 30)).toStdString() +
                      " y " + hex(en(img, *v, 110, 50)).toStdString());
        comprueba(parecido(en(img, *v, 168, 18), qRgb(255, 255, 0)) &&
                      parecido(en(img, *v, 172, 18), VERDE_PCB),
                  "el cristal ocupa lo que ocupa escalado -hasta x = 170- y no mas");
        comprueba(parecido(en(img, *v, 60, 75), qRgb(255, 0, 255)) &&
                      parecido(en(img, *v, 51, 66), VERDE_PCB) &&
                      parecido(en(img, *v, 60, 62), qRgb(255, 0, 255)),
                  "y el girado, GIRADO: la esquina del cuadrado sin girar es fondo, y la "
                  "punta del rombo, magenta");
        comprueba(parecido(en(img, *v, 110, 50), qRgb(0, 0, 255)) &&
                      parecido(en(img, *v, 30, 95), VERDE_PCB) &&
                      img.pixel(0, 0) != qRgb(255, 255, 255),
                  "el resto, la placa");

        // El fondo, sin B1: se aparta el vivo y donde estaba se ve la placa
        v->vivo(2)->moveBy(40, 0);
        const QImage movido = v->imagen(400);
        comprueba(parecido(en(movido, *v, 110, 50), VERDE_PCB) &&
                      parecido(en(movido, *v, 150, 50), qRgb(0, 0, 255)),
                  "el fondo NO tiene a B1: apartado el vivo, donde estaba se ve la placa "
                  "(" + hex(en(movido, *v, 110, 50)).toStdString() + "), y B1 esta donde se "
                  "ha movido");
        v->vivo(2)->moveBy(-40, 0);
        delete v;

        comprueba(VistaPlaca::crea(p, QString(), "<svg", {}, e) == nullptr &&
                      e.startsWith("no es XML"),
                  "un dibujo roto no da vista, y dice por que");
    }

    // -------------------------------------------------------------------------
    std::printf("I4 La ventana: la ilustracion, en una ventana aparte del panel\n");
    {
        VentanaPrincipal v(0);
        v.show();
        auto* avisos = v.findChild<QListWidget*>("avisos");
        auto* accion = v.findChild<QAction*>("abrir_dibujo");
        auto* ver = v.findChild<QAction*>("ver_ilustracion");
        comprueba(!v.findChild<QWidget*>("ventana_ilustracion") &&
                      !v.findChild<QTabWidget*>("vistas") &&
                      v.findChild<QScrollArea*>("centro") && accion && !accion->isEnabled() &&
                      ver && ver->isCheckable() && !ver->isEnabled() &&
                      ver->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_I),
                  "sin placa, solo el panel, sin pestanas ni ventana de ilustracion, y "
                  "Vista > Ilustracion (Ctrl+I) apagado");
        QString e;
        comprueba(!v.abre_dibujo(QString(), DIBUJO, &e) && e.contains("no hay placa"),
                  "ni un dibujo se puede abrir sin placa");

        ModeloFalso m;
        m.conecta(v.sesion().puerto());
        saluda_hasta_listo(m);
        comprueba(espera([&] { return v.findChild<QWidget*>("ventana_ilustracion"); }) &&
                      accion->isEnabled() && ver->isEnabled(),
                  "con la placa, su ventana de ilustracion");
        auto* ventana = v.findChild<QWidget*>("ventana_ilustracion");
        auto* ilus = v.findChild<VistaIlustracion*>("ilustracion");
        comprueba(ventana->isWindow() && ventana->parentWidget() == &v &&
                      ilus && ilus->window() == ventana && !ventana->isVisible() &&
                      !ver->isChecked(),
                  "una ventana de verdad, hija de la principal, con la ilustracion dentro; "
                  "sin dibujo de la placa no se abre sola");
        ver->trigger();
        comprueba(ventana->isVisible() && v.isVisible() &&
                      ventana->windowTitle() == QStringLiteral("Ilustracion — discovery"),
                  "Ctrl+I la abre, con el generado y la placa en el titulo, y la principal "
                  "sigue ahi: \"" +
                      ventana->windowTitle().toStdString() + "\"");
        ventana->close();
        comprueba(!ventana->isVisible() && !ver->isChecked() &&
                      v.findChild<QWidget*>("ventana_ilustracion") == ventana,
                  "cerrarla solo la esconde, y el menu lo sabe");
        comprueba(ilus && ilus->findChild<QWidget*>("dibujo:") &&
                      ilus->findChild<VistaPlaca*>("vista:") &&
                      ilus->origen(QString()) == "generado" &&
                      ilus->findChild<QPushButton*>("abrir:") && !ilus->hay_dibujo(),
                  "la ilustracion: un recuadro, con el dibujo GENERADO (fase 5) y su boton "
                  "para abrir otro; un generado no cuenta como dibujo de la placa");

        comprueba(v.abre_dibujo(QString(), DIBUJO, &e), "se abre el dibujo a mano");
        auto* vp = ilus->findChild<VistaPlaca*>("vista:");
        auto* inf = ilus->findChild<QLabel*>("informe:");
        comprueba(vp && ilus->origen(QString()) == "svg" && ventana->isVisible() &&
                      ver->isChecked(),
                  "y se ve: el dibujo sustituye al generado, y su ventana se abre sola");
        comprueba(inf && inf->isVisible() && inf->text().startsWith("2 de 4 piezas") &&
                      inf->toolTip().contains("serigrafia"),
                  "el recuadro dice lo que ha encontrado, y el detalle en la ayuda");
        comprueba(hay_aviso(avisos, "2 de 4 piezas") && hay_aviso(avisos, "/etc/passwd") &&
                      hay_aviso(avisos, "sin elemento"),
                  "y la lista de avisos tambien, linea a linea");
        comprueba(v.findChildren<QGroupBox*>().size() == 4 && v.findChild<QWidget*>("panel") &&
                      v.findChild<QWidget*>("panel")->window() == &v,
                  "el panel sigue entero en la principal: la ilustracion no lo sustituye");
        auto* encima = v.findChild<QAction*>("siempre_encima");
        encima->trigger();
        comprueba((ventana->windowFlags() & Qt::WindowStaysOnTopHint) && ventana->isVisible() &&
                      (v.windowFlags() & Qt::WindowStaysOnTopHint),
                  "Siempre encima vale para las dos ventanas");
        encima->trigger();
        comprueba(!(ventana->windowFlags() & Qt::WindowStaysOnTopHint) && ventana->isVisible(),
                  "y desmarcado, en las dos");

        comprueba(!v.abre_dibujo(QString(), "<svg", &e) && ilus->findChild<VistaPlaca*>("vista:") == vp,
                  "un dibujo roto no quita el que habia");
        comprueba(!v.abre_dibujo("Z", DIBUJO, &e) && e.contains("\"Z\""),
                  "ni se abre para una placa que no existe");
        m.s.disconnectFromHost();
        espera([&] { return v.sesion().estado() == Sesion::Estado::Terminada; });
    }
    {
        VentanaPrincipal v(0);
        v.show();
        auto saluda = [&](ModeloFalso& m) {
            m.conecta(v.sesion().puerto());
            m.manda(T_HOLA, "protocolo_max=2\nplaca=placas/nucleo_y_shield.xml\n");
            m.espera_leidos(1);
            m.version(2);
            m.manda(T_PLACA, SISTEMA_XML);
            m.manda(T_CATALOGO, CATALOGO_SISTEMA_XML);
            m.manda(T_LISTO);
            return espera([&] { return v.findChild<VistaIlustracion*>("ilustracion"); });
        };
        ModeloFalso m;
        saluda(m);
        auto* ilus = v.findChild<VistaIlustracion*>("ilustracion");
        comprueba(ilus && ilus->placas() == QStringList({"N", "S"}) &&
                      ilus->findChild<QWidget*>("dibujo:N") &&
                      ilus->findChild<QWidget*>("dibujo:S") &&
                      ilus->findChild<QPushButton*>("abrir:S"),
                  "en un sistema, un recuadro por placa, cada uno con su boton");
        QString e;
        comprueba(v.abre_dibujo("N", DIBUJO_NUCLEO, &e) && ilus->origen("N") == "svg" &&
                      ilus->origen("S") == "generado",
                  "el dibujo de N va a N; S sigue sin dibujo");
        comprueba(ilus->informe("N").resumen() == "2 de 2 piezas en el dibujo",
                  "y en N se encuentran sus dos piezas: \"" +
                      ilus->informe("N").resumen().toStdString() + "\"");
        m.s.disconnectFromHost();
        comprueba(espera([&] { return v.sesion().estado() == Sesion::Estado::Terminada; }),
                  "el modelo se va");

        ilus->setProperty("vieja", true);     // la de antes: no vale encontrarla
        ModeloFalso m2;
        saluda(m2);
        comprueba(espera([&] {
                      auto* i = v.findChild<VistaIlustracion*>("ilustracion");
                      return i && !i->property("vieja").isValid() && i->origen("N") == "svg";
                  }) && v.findChild<QWidget*>("ventana_ilustracion")->isVisible() &&
                      v.findChildren<QWidget*>("ventana_ilustracion").size() == 1 &&
                      v.findChild<VistaIlustracion*>("ilustracion")->window() ==
                          v.findChild<QWidget*>("ventana_ilustracion"),
                  "otro modelo con la misma Nucleo: su dibujo sale solo, que la ventana lo "
                  "recuerda por el nombre de la placa, y se ve, en la MISMA ventana aparte");
    }

    // -------------------------------------------------------------------------
    std::printf("I5 Los observables sobre el dibujo\n");
    {
        const PlacaGui p = placa_de(PLACA_XML, CATALOGO_XML);
        QString e;
        std::unique_ptr<VistaPlaca> v(
            VistaPlaca::crea(p, QString(), DIBUJO, {{"X3", "cristal", "ninguno"}}, e));
        comprueba(v && v->efecto(1) == "brillo" && v->efecto(2) == "hundido" &&
                      v->efecto(0) == "ninguno" && v->efecto(3).isEmpty(),
                  "por declaracion: el LED -un 0/1- brilla, el pulsador -un 0/1 y un mando "
                  "boton- se hunde; y el cristal, nada, porque la tabla lo dice");
        if (!v) return resultado();
        comprueba(v->halo(1) && !v->halo(2) && !v->halo(0) && !v->contorno(1),
                  "solo lo que brilla lleva halo, y sin alarmas no hay contorno");
        comprueba(parecido(v->color(1).rgb(), qRgb(255, 0, 0), 8),
                  "el halo es del color del elemento: " + v->color(1).name().toStdString());
        comprueba(v->observados() == QVector<quint16>({0, 1, 2, 3}),
                  "se necesitan todos los observables de las piezas dibujadas: tambien la "
                  "corriente del LED, que el panel no pinta");

        const QImage apagado = v->imagen(400);
        comprueba(v->halo(1)->opacity() == 0 && parecido(en(apagado, *v, 40, 30), qRgb(255, 0, 0)) &&
                      parecido(en(apagado, *v, 51, 30), VERDE_PCB),
                  "sin muestras, apagado: el LED rojo y, al lado, la placa");
        v->pon_valor(1, 1.f);
        comprueba(v->halo(1)->opacity() == 1.0,
                  "encendido sin saber la corriente: brillo entero");
        v->pon_valor(2, 25.f);
        const QImage lleno = v->imagen(400);
        v->pon_valor(2, 1.f);
        const double op_poca = v->halo(1)->opacity();
        const QImage poco = v->imagen(400);
        comprueba(std::abs(op_poca - (0.3 + 0.7 * std::sqrt(1.0 / 25.0))) < 1e-6,
                  "con 1 mA de 25, 0,3 + 0,7·raiz(1/25) = 0,44");
        const QRgb c_ap = en(apagado, *v, 40, 30), c_ll = en(lleno, *v, 40, 30);
        const QRgb f_ap = en(apagado, *v, 51, 30), f_ll = en(lleno, *v, 51, 30),
                   f_po = en(poco, *v, 51, 30);
        comprueba(luz(c_ll) > luz(c_ap) + 100,
                  "encendido, el centro del LED se aclara: " + hex(c_ap).toStdString() + " -> " +
                      hex(c_ll).toStdString());
        comprueba(qRed(f_ll) > qRed(f_po) && qRed(f_po) > qRed(f_ap) + 10 &&
                      qRed(f_ll) > qGreen(f_ll),
                  "y alrededor, el halo, mas rojo con mas corriente: " + hex(f_ap).toStdString() +
                      " / " + hex(f_po).toStdString() + " / " + hex(f_ll).toStdString());
        v->pon_valor(1, 0.f);
        comprueba(v->halo(1)->opacity() == 0, "y apagado otra vez, sin halo");
        comprueba(v->vivo(1)->toolTip() ==
                      QString::fromUtf8("LD4 · Led\nencendido: ○\ncorriente: 1 mA"),
                  "la ayuda del LED dice todos sus valores: \"" +
                      v->vivo(1)->toolTip().toStdString() + "\"");

        comprueba(parecido(en(apagado, *v, 101, 50), qRgb(0, 0, 255)),
                  "B1 suelto llega hasta su borde");
        v->pon_valor(3, 1.f);
        const QImage hundido = v->imagen(400);
        comprueba(v->vivo(2)->scale() < 1 && !parecido(en(hundido, *v, 101, 50), qRgb(0, 0, 255)) &&
                      qBlue(en(hundido, *v, 110, 50)) < 235 &&
                      qBlue(en(hundido, *v, 110, 50)) > 150,
                  "pulsado, la tapa se hunde: mas pequena -el borde ya no es azul- y mas "
                  "apagada -el centro, menos azul-: " + hex(en(hundido, *v, 110, 50)).toStdString());
        v->pon_valor(3, 0.f);
        comprueba(v->vivo(2)->scale() == 1 && v->vivo(2)->opacity() == 1, "y vuelve");
        v->pon_valor(999, 1.f);
        comprueba(true, "un id_obs que no es de nadie, se ignora");

        std::unique_ptr<VistaPlaca> r(
            VistaPlaca::crea(p, QString(), DIBUJO, {{"X3", "cristal", "raro"}}, e));
        comprueba(r && r->efecto(0) == "brillo" &&
                      r->informe().avisos.last().contains("«raro» no existe"),
                  "un efecto que no existe se dice, y se usa el de la declaracion");
    }
    {
        const PlacaGui p = placa_de(PLACA_FUENTE, CATALOGO_FUENTE);
        QString e;
        std::unique_ptr<VistaPlaca> v(VistaPlaca::crea(p, QString(), DIBUJO_FUENTE, {}, e));
        comprueba(v && v->efecto(0) == "ninguno" && v->contorno(0) &&
                      !v->contorno(0)->isVisible() && v->etiqueta(0) && !v->etiqueta(1),
                  "una Fuente: sin brillo, con contorno de alarma -escondido- y una etiqueta "
                  "para la corriente, que es lo que sugiere; la alarma no lleva etiqueta");
        if (!v) return resultado();
        v->pon_valor(0, -12.5f);
        comprueba(v->etiqueta(0)->text() == "-12.5 mA" &&
                      v->etiqueta(0)->sceneBoundingRect().top() >= 50 &&
                      std::abs(v->etiqueta(0)->sceneBoundingRect().center().x() - 50) < 1,
                  "la etiqueta dice el valor con su unidad, centrada debajo del elemento");
        v->pon_valor(1, 1.f);
        const QImage alarma = v->imagen(400);
        const QRectF borde = v->contorno(0)->rect();
        comprueba(v->contorno(0)->isVisible() &&
                      parecido(en(alarma, *v, borde.left(), 40), qRgb(0xc6, 0x28, 0x28)),
                  "la sobrecorriente: un contorno rojo, al momento");
        comprueba(espera([&] { return !v->contorno(0)->isVisible(); }, 1000) &&
                      espera([&] { return v->contorno(0)->isVisible(); }, 1000),
                  "que parpadea");
        v->pon_valor(1, 0.f);
        comprueba(!v->contorno(0)->isVisible() &&
                      !espera([&] { return v->contorno(0)->isVisible(); }, 700),
                  "y se apaga, y deja de parpadear, al volver");
    }

    // -------------------------------------------------------------------------
    std::printf("I6 La ventana: la suscripcion y las muestras llegan al dibujo\n");
    {
        VentanaPrincipal v(0);
        v.show();
        ModeloFalso m;
        m.conecta(v.sesion().puerto());
        saluda_hasta_listo(m);
        comprueba(m.espera_leidos(2) && m.leido[1].tipo == T_SUSCRIBE,
                  "con T_LISTO, la suscripcion del panel");
        v.abre_dibujo(QString(), DIBUJO);
        CabSuscribe cs{};
        comprueba(m.espera_leidos(3) && m.leido[2].tipo == T_SUSCRIBE &&
                      (std::memcpy(&cs, m.leido[2].cuerpo.constData(), sizeof cs), true) &&
                      cs.n == 4 &&
                      m.leido[2].cuerpo.mid(int(sizeof cs)) ==
                          QByteArray::fromStdString(bytes(uint16_t(0)) + bytes(uint16_t(1)) +
                                                    bytes(uint16_t(3)) + bytes(uint16_t(2))),
                  "con el dibujo abierto, otra: lo del panel y, detras, la corriente del "
                  "LED, que solo necesita el dibujo");
        auto* ilus = v.findChild<VistaIlustracion*>("ilustracion");
        VistaPlaca* vp = ilus ? ilus->vista(QString()) : nullptr;
        std::string inst = bytes(CabInstantanea{5000000ull, 3, 0});
        inst += bytes(Muestra{1, 0, 1.f}) + bytes(Muestra{2, 0, 20.f}) + bytes(Muestra{3, 0, 1.f});
        m.manda(T_INSTANTANEA, inst);
        comprueba(vp && espera([&] { return vp->halo(1)->opacity() > 0.9; }) &&
                      vp->vivo(2)->scale() < 1,
                  "y una instantanea enciende el LED del dibujo y hunde su boton");
        auto* enc = v.findChild<QLabel*>("obs:1");
        comprueba(enc && enc->text() == QString::fromUtf8("●"), "a la vez que el panel");
    }

    // -------------------------------------------------------------------------
    std::printf("I7 Los mandos sobre el dibujo, con el raton\n");
    {
        const PlacaGui p = placa_de(PLACA_MANDOS, CATALOGO_MANDOS);
        QString e;
        std::unique_ptr<VistaPlaca> v(VistaPlaca::crea(p, QString(), DIBUJO_MANDOS, {}, e));
        comprueba(v != nullptr, "la vista se construye " + e.toStdString());
        if (!v) return resultado();
        v->resize(800, 200);
        v->show();
        espera([] { return false; }, 50);
        std::vector<OrdenVista> o;
        QObject::connect(v.get(), &VistaPlaca::orden,
                         [&](quint16 pz, quint16 m, float x) { o.push_back({pz, m, x}); });
        auto ultima = [&](int pz, int m, float x) {
            return !o.empty() && o.back().pieza == pz && o.back().mando == m &&
                   o.back().valor == x;
        };
        const QPoint b1 = v->donde(0), sw = v->donde(1), pot = v->donde(2), sel = v->donde(3),
                     ld = v->donde(4);
        comprueba(b1.x() > 0 && sw.x() > b1.x() && ld.x() > sel.x(),
                  "cada pieza tiene donde pinchar");
        comprueba(v->vivo(0)->toolTip().contains("Ctrl+clic") &&
                      v->vivo(2)->toolTip().contains("clic o rueda: giro") &&
                      !v->vivo(4)->toolTip().contains("clic"),
                  "la ayuda dice que hace el raton en cada una, y nada en un LED");

        clic(v.get(), b1);
        comprueba(o.empty() && !v->mandos_activos(), "con los mandos apagados, un clic no ordena");

        v->activa_mandos(true);
        raton(v.get(), QEvent::MouseButtonPress, b1);
        comprueba(o.size() == 1 && ultima(0, 0, 1.f) && v->vivo(0)->scale() < 1,
                  "pulsar B1 ordena pulsar = 1, y la tapa se hunde al momento, sin esperar "
                  "la muestra");
        raton(v.get(), QEvent::MouseButtonRelease, b1);
        comprueba(o.size() == 2 && ultima(0, 0, 0.f) && v->vivo(0)->scale() == 1,
                  "soltarlo, pulsar = 0, y vuelve");
        clic(v.get(), b1, Qt::ControlModifier);
        comprueba(o.size() == 3 && ultima(0, 0, 1.f) && v->vivo(0)->scale() < 1,
                  "Ctrl+clic lo deja hundido: una orden, y al soltar el raton, ninguna");
        clic(v.get(), b1);
        comprueba(o.size() == 3 && v->vivo(0)->scale() < 1,
                  "con el «switch» puesto, un clic normal no ordena nada: ni al bajar ni, "
                  "sobre todo, al subir");
        clic(v.get(), b1, Qt::ControlModifier);
        comprueba(o.size() == 4 && ultima(0, 0, 0.f) && v->vivo(0)->scale() == 1,
                  "y otro Ctrl+clic lo suelta");

        clic(v.get(), sw);
        comprueba(o.size() == 5 && ultima(1, 0, 1.f), "un interruptor: un clic, a 1");
        clic(v.get(), sw);
        comprueba(o.size() == 6 && ultima(1, 0, 0.f), "otro, a 0");
        raton(v.get(), QEvent::MouseButtonPress, sw);
        raton(v.get(), QEvent::MouseButtonRelease, sw);
        raton(v.get(), QEvent::MouseButtonDblClick, sw);
        raton(v.get(), QEvent::MouseButtonRelease, sw);
        comprueba(o.size() == 8 && ultima(1, 0, 0.f),
                  "y un doble clic son dos cambios, no uno");

        clic(v.get(), pot);
        QMenu* menu = v->menu_abierto();
        auto* des = menu ? menu->findChild<QSlider*>("mando:2:0") : nullptr;
        comprueba(menu && menu->objectName() == "emergente:2" && des && des->value() == 500 &&
                      des->isEnabled() && o.size() == 8,
                  "un continuo: el clic abre su deslizador, donde esta el modelo -5 de 10-, "
                  "sin ordenar nada");
        if (des) des->setValue(800);
        comprueba(o.size() == 9 && ultima(2, 0, 8.f), "moverlo ordena: 8");
        // Con un menú abierto, Qt no deja llegar la rueda a otra ventana: se cierra
        if (v->menu_abierto()) v->menu_abierto()->close();
        rueda(v.get(), pot, 1);
        comprueba(o.size() == 10 && ultima(2, 0, 8.5f),
                  "la rueda, un veinteavo del rango por muesca: 8,5");
        rueda(v.get(), pot, 10);
        comprueba(o.size() == 11 && ultima(2, 0, 10.f), "sin pasar del maximo");
        clic(v.get(), sel);
        auto* caja = v->menu_abierto() ? v->menu_abierto()->findChild<QSpinBox*>("mando:3:0")
                                       : nullptr;
        comprueba(caja && caja->value() == 1 && caja->maximum() == 3 && (!menu || menu != v->menu_abierto()),
                  "uno discreto, su caja numerica, en su valor; y el menu de antes se cierra");
        if (caja) caja->setValue(3);
        comprueba(o.size() == 12 && ultima(3, 0, 3.f), "elegir el 3 lo ordena");
        if (v->menu_abierto()) v->menu_abierto()->close();
        rueda(v.get(), sel, -2);
        comprueba(o.size() == 13 && ultima(3, 0, 1.f), "la rueda, de uno en uno: 3 - 2 = 1");
        rueda(v.get(), sel, -5);
        rueda(v.get(), sel, -1);
        comprueba(o.size() == 14 && ultima(3, 0, 0.f), "hasta el minimo, y de ahi no baja");

        clic(v.get(), ld);
        rueda(v.get(), ld, 1);
        comprueba(o.size() == 14, "una pieza sin mandos no hace nada");

        {
            QContextMenuEvent ce(QContextMenuEvent::Mouse, b1, v->viewport()->mapToGlobal(b1));
            QCoreApplication::sendEvent(v->viewport(), &ce);
        }
        QMenu* m0 = v->menu_abierto();
        QAction* fija = nullptr;
        if (m0)
            for (QAction* a : m0->actions())
                if (a->objectName() == "mando:0:0") fija = a;
        auto* rebote = m0 ? m0->findChild<QSlider*>("mando:0:1") : nullptr;
        auto* rebotes = m0 ? m0->findChild<QSpinBox*>("mando:0:2") : nullptr;
        comprueba(m0 && m0->objectName() == "menu:0" && fija && fija->isCheckable() &&
                      !fija->isChecked() && rebote && rebote->value() == 100 && rebotes &&
                      rebotes->value() == 5,
                  "el boton derecho: todos los mandos de B1 -dejarlo hundido, el rebote en 2 "
                  "de 20 y los rebotes en 5-");
        if (fija) fija->setChecked(true);
        comprueba(o.size() == 15 && ultima(0, 0, 1.f) && v->vivo(0)->scale() < 1,
                  "dejarlo hundido desde el menu es el Ctrl+clic");
        if (rebotes) rebotes->setValue(7);
        comprueba(o.size() == 16 && ultima(0, 2, 7.f), "y los rebotes, 7");
        clic(v.get(), b1, Qt::ControlModifier);
        comprueba(o.size() == 17 && ultima(0, 0, 0.f), "Ctrl+clic suelta lo que puso el menu");

        {
            QMouseEvent mv(QEvent::MouseMove, QPointF(b1), QPointF(v->viewport()->mapToGlobal(b1)),
                           Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(v->viewport(), &mv);
        }
        const bool mano = v->viewport()->cursor().shape() == Qt::PointingHandCursor;
        {
            QMouseEvent mv(QEvent::MouseMove, QPointF(ld), QPointF(v->viewport()->mapToGlobal(ld)),
                           Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(v->viewport(), &mv);
        }
        comprueba(mano && v->viewport()->cursor().shape() == Qt::ArrowCursor,
                  "sobre una pieza con mandos el cursor es una mano; sobre un LED, no");

        clic(v.get(), pot);
        raton(v.get(), QEvent::MouseButtonPress, b1);
        v->activa_mandos(false);
        comprueba(!v->menu_abierto() || !v->menu_abierto()->isVisible(),
                  "apagar los mandos cierra el menu abierto");
        raton(v.get(), QEvent::MouseButtonRelease, b1);
        const std::size_t n = o.size();
        clic(v.get(), sw);
        rueda(v.get(), sel, 1);
        comprueba(o.size() == n && v->vivo(0)->scale() == 1,
                  "y, apagados, ni el clic, ni la rueda, ni el boton que estaba abajo");
    }

    // -------------------------------------------------------------------------
    std::printf("I8 La ventana: las ordenes del dibujo llegan al modelo\n");
    {
        VentanaPrincipal v(0);
        v.resize(1000, 700);
        v.show();
        ModeloFalso m;
        m.conecta(v.sesion().puerto());
        m.manda(T_HOLA, "protocolo_max=1\nplaca=placas/mandos.xml\n");
        m.espera_leidos(1);
        m.version(1);
        m.manda(T_PLACA, PLACA_MANDOS);
        m.manda(T_CATALOGO, CATALOGO_MANDOS);
        espera([&] { return v.findChild<VistaIlustracion*>("ilustracion"); });
        v.abre_dibujo(QString(), DIBUJO_MANDOS);
        auto* ilus = v.findChild<VistaIlustracion*>("ilustracion");
        VistaPlaca* vp = ilus ? ilus->vista(QString()) : nullptr;
        espera([] { return false; }, 100);
        comprueba(vp && !vp->mandos_activos(), "antes de T_LISTO, los mandos del dibujo, apagados");
        if (!vp) return resultado();
        m.manda(T_LISTO);
        comprueba(espera([&] { return vp->mandos_activos(); }),
                  "con T_LISTO se encienden, con los del panel");
        m.espera_leidos(2);
        const std::size_t antes = m.leido.size();
        clic(vp, vp->donde(1));
        comprueba(m.espera_leidos(antes + 1) && m.leido.back().tipo == T_ORDENES &&
                      m.leido.back().cuerpo == QByteArray::fromStdString(bytes(Orden{0, 1, 0, 1.f})),
                  "un clic en el interruptor del dibujo sale como T_ORDENES: pieza 1, "
                  "mando 0, 1");
        m.manda(T_FIN, bytes(Fin{M_VENTANA, 0, 1000000ull}));
        comprueba(espera([&] { return !vp->mandos_activos(); }), "y con T_FIN se apagan");
    }

    // -------------------------------------------------------------------------
    std::printf("I9 El dibujo lo manda el modelo: T_ILUSTRACION y la tabla de T_PLACA\n");
    {
        Sesion::Ilustracion il;
        comprueba(Sesion::lee_ilustracion("placas=L1 L2\nfichero=pc104_leds.svg\n\n<svg/>", il) &&
                      il.placas == QStringList({"L1", "L2"}) && il.fichero == "pc104_leds.svg" &&
                      il.svg == "<svg/>",
                  "T_ILUSTRACION: las placas, el fichero y, tras la linea en blanco, el SVG "
                  "tal cual");
        comprueba(Sesion::lee_ilustracion("placas=\nfichero=a.svg\n\n<svg>\n\n</svg>", il) &&
                      il.placas == QStringList({QString()}) && il.svg == "<svg>\n\n</svg>",
                  "placas= vacio es la placa suelta; y el SVG puede llevar lineas en blanco");
        comprueba(!Sesion::lee_ilustracion("placas=N\n<svg/>", il),
                  "sin la linea en blanco no es un T_ILUSTRACION");

        QByteArray xml(PLACA_XML);
        xml.replace("<placa nombre=\"discovery\">",
                    "<placa nombre=\"discovery\" ilustracion=\"discovery.svg\">\n"
                    "  <ilustracion>\n"
                    "    <enlace pieza=\"X3\" elemento=\"cristal\" efecto=\"ninguno\"/>\n"
                    "  </ilustracion>");
        const PlacaGui p = placa_de(xml.constData(), CATALOGO_XML);
        comprueba(p.ilustracion == "discovery.svg" && p.tabla.size() == 1 &&
                      p.tabla[0].pieza == "X3" && p.tabla[0].elemento == "cristal" &&
                      p.tabla[0].efecto == "ninguno" && p.tabla_de(QString()).size() == 1,
                  "T_PLACA trae el dibujo declarado de la placa y su tabla de enlaces");
        QByteArray sx(SISTEMA_XML);
        sx.replace("<placa id=\"S\" nombre=\"shield-leds\" fichero=\"shield_leds.xml\"/>",
                   "<placa id=\"S\" nombre=\"shield-leds\" fichero=\"shield_leds.xml\" "
                   "ilustracion=\"shield.svg\">\n"
                   "    <ilustracion><enlace pieza=\"LD_D13\" elemento=\"rojo\"/></ilustracion>\n"
                   "  </placa>");
        const PlacaGui s = placa_de(sx.constData(), CATALOGO_SISTEMA_XML);
        comprueba(s.subplaca("S") && s.subplaca("S")->ilustracion == "shield.svg" &&
                      s.tabla_de("S").size() == 1 && s.tabla_de("S")[0].pieza == "LD_D13" &&
                      s.tabla_de("N").isEmpty() && s.tabla.isEmpty(),
                  "y en un sistema, la de cada placa en la suya");

        VentanaPrincipal v(0);
        v.show();
        auto* avisos = v.findChild<QListWidget*>("avisos");
        ModeloFalso m;
        m.conecta(v.sesion().puerto());
        m.manda(T_HOLA, "protocolo_max=1\nplaca=placas/discovery.xml\n");
        m.espera_leidos(1);
        m.version(1);
        m.manda(T_PLACA, xml.toStdString());
        m.manda(T_CATALOGO, CATALOGO_XML);
        m.manda(T_ILUSTRACION, std::string("placas=\nfichero=discovery.svg\n\n") + DIBUJO);
        m.manda(T_LISTO);
        auto* arr = v.findChild<QPushButton*>("arrancar");
        comprueba(espera([&] { return arr->isEnabled(); }), "saludo con un T_ILUSTRACION");
        auto* ilus = v.findChild<VistaIlustracion*>("ilustracion");
        VistaPlaca* vp = ilus ? ilus->vista(QString()) : nullptr;
        comprueba(vp && v.findChild<QWidget*>("ventana_ilustracion")->isVisible() &&
                      ilus->informe(QString()).resumen().startsWith("3 de 4 piezas") &&
                      vp->efecto(0) == "ninguno",
                  "la ventana lo pone sin que nadie abra nada, y se ve la ilustracion; con "
                  "la tabla de la placa, X3 es el cristal -3 de 4- y sin efecto");
        comprueba(hay_aviso(avisos, "dibujo (discovery.svg)") && hay_aviso(avisos, "3 de 4"),
                  "los avisos dicen de que fichero es lo encontrado");
        m.s.disconnectFromHost();
        espera([&] { return v.sesion().estado() == Sesion::Estado::Terminada; });

        ModeloFalso m2;
        m2.conecta(v.sesion().puerto());
        m2.manda(T_HOLA, "protocolo_max=2\nplaca=placas/nucleo_y_shield.xml\n");
        m2.espera_leidos(1);
        m2.version(2);
        m2.manda(T_PLACA, SISTEMA_XML);
        m2.manda(T_CATALOGO, CATALOGO_SISTEMA_XML);
        m2.manda(T_ILUSTRACION, std::string("placas=N Z\nfichero=nucleo.svg\n\n") + DIBUJO_NUCLEO);
        m2.manda(T_LISTO);
        comprueba(espera([&] { return arr->isEnabled(); }), "y otro, con un sistema");
        ilus = v.findChild<VistaIlustracion*>("ilustracion");
        comprueba(ilus && ilus->origen("N") == "svg" &&
                      ilus->informe("N").resumen() == "2 de 2 piezas en el dibujo",
                  "el dibujo va a la placa N");
        comprueba(hay_aviso(avisos, "es de la placa \"Z\""),
                  "y una placa que no existe se dice, sin mas");
    }

    // -------------------------------------------------------------------------
    std::printf("I10 El dibujo generado, y la bandeja\n");
    {
        const PlacaGui d = placa_de(PLACA_XML, CATALOGO_XML);
        const PlacaGui mds = placa_de(PLACA_MANDOS, CATALOGO_MANDOS);
        const PlacaGui f = placa_de(PLACA_FUENTE, CATALOGO_FUENTE);
        comprueba(glifo_de(d.piezas[1]) == "piloto" && glifo_de(d.piezas[2]) == "boton" &&
                      glifo_de(d.piezas[3]) == "pieza" && glifo_de(f.piezas[0]) == "medida" &&
                      glifo_de(mds.piezas[1]) == "interruptor" &&
                      glifo_de(mds.piezas[2]) == "mando" && glifo_de(mds.piezas[3]) == "mando",
                  "el glifo de cada pieza sale de lo que declara: un 0/1 es un piloto, un "
                  "boton un boton, un continuo o un discreto un mando giratorio, una medida "
                  "un recuadro, y lo que no declara nada una pieza gris");

        DibujoPlaca g;
        QString e;
        comprueba(g.carga(dibujo_generado(mds, QString()), e) && g.mm_por_unidad() == 1.0,
                  "el generado es un SVG que se lee como cualquier otro, en milimetros");
        const InformeDibujo ig = g.enlaza(mds, QString(), {});
        comprueba(ig.enlaces.size() == 5 && ig.sin_elemento.isEmpty() &&
                      ig.repetidos.isEmpty(),
                  "con todas las piezas de la placa, por su id: " + ig.resumen().toStdString());
        std::unique_ptr<VistaPlaca> vg(
            VistaPlaca::crea(mds, QString(), dibujo_generado(mds, QString()), {}, e));
        vg->resize(600, 400);
        vg->show();
        espera([] { return false; }, 50);
        std::vector<OrdenVista> o;
        QObject::connect(vg.get(), &VistaPlaca::orden,
                         [&](quint16 pz, quint16 m, float x) { o.push_back({pz, m, x}); });
        vg->activa_mandos(true);
        clic(vg.get(), vg->donde(1));
        comprueba(o.size() == 1 && o[0].pieza == 1 && o[0].valor == 1.f &&
                      vg->efecto(4) == "brillo" && vg->efecto(0) == "hundido",
                  "y se usa igual: el interruptor generado se cambia con un clic, el piloto "
                  "brilla y el boton se hunde");

        // Un sistema: cada placa con sus chips y sus conectores de verdad
        const char* CAT_PILA =
            "<catalogo>\n"
            "  <pieza idx=\"0\" id=\"CPU/J1\" tipo=\"Conector\"/>\n"
            "  <pieza idx=\"1\" id=\"L1/J1\" tipo=\"Conector\"/>\n"
            "  <pieza idx=\"2\" id=\"L2/J1\" tipo=\"Conector\"/>\n"
            "  <pieza idx=\"3\" id=\"L1/LD1\" tipo=\"Led\">\n"
            "    <observable idx=\"0\" id_obs=\"0\" nombre=\"encendido\" unidad=\"\" min=\"0\" max=\"1\" interesante=\"si\"/>\n"
            "  </pieza>\n"
            "  <pieza idx=\"4\" id=\"L2/J9\" tipo=\"Conector\"/>\n"
            "</catalogo>\n";
        const PlacaGui pila = placa_de(PILA_XML, CAT_PILA);
        const QByteArray cpu = dibujo_generado(pila, "CPU");
        DibujoPlaca gc;
        comprueba(gc.carga(cpu, e) && gc.existe("J1") && gc.existe("J1.1") && gc.existe("J1.64") &&
                      !gc.existe("J1.65") && cpu.contains(">u0<") && cpu.contains("STM32F407VG") &&
                      cpu.contains("CPU · pc104-cpu"),
                  "la CPU de la pila: su nombre, su chip con su tipo y su J1 de 64 pines, "
                  "cada uno con su id");
        const QRectF p1 = gc.caja("J1.1"), p2 = gc.caja("J1.2"), p3 = gc.caja("J1.3");
        comprueba(std::abs(p2.x() - p1.x()) < 0.01 && p2.y() > p1.y() + 2 &&
                      std::abs(p3.x() - p1.x() - 2.54) < 0.01 && std::abs(p3.y() - p1.y()) < 0.01 &&
                      std::abs(gc.caja("J1").width() - (32 * 2.54 + 1)) < 0.25,
                  "en zigzag, como dice T_PLACA: el 2 debajo del 1, el 3 al lado, a 2,54 mm; "
                  "32 columnas (con el trazo, " + std::to_string(gc.caja("J1").width()) + " mm)");
        const QByteArray l2 = dibujo_generado(pila, "L2");
        DibujoPlaca gl;
        comprueba(gl.carga(l2, e) && gl.existe("J9.8") &&
                      std::abs(gl.caja("J9.2").y() - gl.caja("J9.1").y()) < 0.01 &&
                      gl.caja("J9.2").x() > gl.caja("J9.1").x() && !l2.contains(">u0<"),
                  "y la J9 de L2, de una fila: los pines uno al lado de otro; L2 no lleva chip");
        const InformeDibujo il = gl.enlaza(pila, "L2", {});
        comprueba(il.enlaces.size() == 2 && il.sin_elemento.isEmpty(),
                  "sus conectores son sus piezas: J1 y J9, encontrados por id");

        // La bandeja
        VentanaPrincipal v(0);
        v.show();
        ModeloFalso m;
        m.conecta(v.sesion().puerto());
        saluda_hasta_listo(m);
        auto* arr = v.findChild<QPushButton*>("arrancar");
        espera([&] { return arr->isEnabled(); });
        auto* ilus = v.findChild<VistaIlustracion*>("ilustracion");
        VistaPlaca* vp = ilus ? ilus->vista(QString()) : nullptr;
        comprueba(vp && vp->origen(QString()) == "generado" && !vp->tiene(QString(), true) &&
                      vp->vivo(0) && vp->vivo(1) && vp->vivo(2) && vp->vivo(3),
                  "sin dibujo, el generado, con las cuatro piezas, y sin bandeja");
        comprueba(!v.abre_dibujo(QString(), "<svg", &e) && vp->origen(QString()) == "generado" &&
                      v.findChild<QLabel*>("informe:")->text().contains("no sirve"),
                  "un dibujo que no se puede leer: se dice, y se queda el generado");
        v.abre_dibujo(QString(), DIBUJO);
        auto* inf = v.findChild<QLabel*>("informe:");
        const QRectF placa = vp->en_escena(QString()), bandeja = vp->en_escena(QString(), true);
        comprueba(vp->origen(QString()) == "svg" && vp->tiene(QString(), true) &&
                      inf->text().endsWith("en la bandeja: X3"),
                  "con el dibujo de prueba, a X3 -que deja ver algo- le toca la bandeja; a R35, "
                  "que no deja ver ni tocar nada, no: \"" + inf->text().toStdString() + "\"");
        comprueba(bandeja.left() > placa.right() && std::abs(bandeja.top() - placa.top()) < 0.01 &&
                      vp->vivo(0) && vp->en_escena(QString(), true)
                                         .contains(vp->vivo(0)->sceneBoundingRect().center()),
                  "la bandeja, a la derecha del dibujo y arriba; X3 esta en ella");
        DibujoPlaca db;
        OpcionesGenerado ob;
        ob.solo = {0};
        db.carga(dibujo_generado(d, QString(), ob), e);
        comprueba(std::abs(bandeja.height() - db.lienzo().height() * 2) < 0.01,
                  "a la escala del dibujo: este dice medir 100 mm en 200 unidades, asi que un "
                  "milimetro de la bandeja son dos unidades de la escena");
        vp->pon_valor(0, 1.f);
        comprueba(vp->halo(0) && vp->halo(0)->opacity() == 1.0 &&
                      vp->halo(0)->parentItem() == vp->vivo(0)->parentItem(),
                  "y en la bandeja X3 brilla como en cualquier otro dibujo");
        v.abre_dibujo(QString(), dibujo_generado(d, QString()));
        comprueba(!vp->tiene(QString(), true), "un dibujo con todas las piezas no lleva bandeja");

        // Un dibujo que no dice su tamano: se le suponen 80 mm de alto
        std::unique_ptr<VistaPlaca> vm(VistaPlaca::crea(mds, QString(), DIBUJO_MANDOS, {}, e));
        OpcionesGenerado om;
        om.solo = {4};
        vm->pon_capa(QString(), true, dibujo_generado(mds, QString(), om), {}, "generado", e);
        DibujoPlaca dm;
        dm.carga(dibujo_generado(mds, QString(), om), e);
        comprueba(std::abs(vm->en_escena(QString(), true).height() -
                           dm.lienzo().height() * 25.0 / 80.0) < 0.01,
                  "un dibujo sin milimetros mide 80 mm de alto: 25 unidades son 80 mm, y la "
                  "bandeja se escala a eso");
    }

    // -------------------------------------------------------------------------
    std::printf("I11 Varias placas: una al lado de otra, con lineas entre conectores\n");
    {
        // El sistema de prueba, con sus conectores descritos como los manda el
        // mcu-sim de ahora
        QByteArray sx(SISTEMA_XML);
        sx.replace("<placa id=\"N\" nombre=\"nucleo-f446re\" fichero=\"nucleo_f446re.xml\"/>",
                   "<placa id=\"N\" nombre=\"nucleo-f446re\" fichero=\"nucleo_f446re.xml\">\n"
                   "    <conector ref=\"N/CN5\" filas=\"1\" columnas=\"10\" numeracion=\"zigzag\" acople=\"0\"/>\n"
                   "  </placa>");
        sx.replace("<placa id=\"S\" nombre=\"shield-leds\" fichero=\"shield_leds.xml\"/>",
                   "<placa id=\"S\" nombre=\"shield-leds\" fichero=\"shield_leds.xml\">\n"
                   "    <conector ref=\"S/J5\" filas=\"1\" columnas=\"10\" numeracion=\"zigzag\" acople=\"0\"/>\n"
                   "  </placa>");
        const PlacaGui s = placa_de(sx.constData(), CATALOGO_SISTEMA_XML);
        VistaIlustracion il(s);
        il.resize(900, 500);
        il.show();
        espera([] { return false; }, 50);
        VistaPlaca* v = il.vista("N");
        comprueba(v && v == il.vista("S") && !il.vista("Z") && il.findChild<QWidget*>("dibujo:N") &&
                      il.findChild<QWidget*>("dibujo:S") && il.findChild<QPushButton*>("abrir:S") &&
                      il.findChildren<VistaPlaca*>().size() == 1,
                  "un solo dibujo para las dos placas, y encima una fila por placa con su boton");
        if (!v) return resultado();
        QRectF n = v->en_escena("N"), sr = v->en_escena("S");
        comprueba(std::abs(sr.left() - n.right() - 30) < 0.01 &&
                      std::abs(sr.center().y() - n.center().y()) < 0.01,
                  "una al lado de otra, en el orden del sistema, a 30 mm y centradas: los dos "
                  "generados estan en milimetros, y la escena tambien");
        comprueba(v->lineas().size() == 2 && !v->lineas()[0].hilo && v->lineas()[1].hilo &&
                      v->lineas()[0].a == "N/CN5" && v->lineas()[0].b == "S/J5" &&
                      v->lineas()[0].item->toolTip() == QString::fromUtf8("N/CN5 ⇄ S/J5"),
                  "dos lineas: la del acople CN5-J5 y la del hilo, que va de trazos");
        const QRectF cn5 = v->caja_de("N/CN5"), j5 = v->caja_de("S/J5");
        const VistaPlaca::Linea& l0 = v->lineas()[0];
        comprueba(!cn5.isNull() && !j5.isNull() && std::abs(l0.pa.x() - cn5.right()) < 0.01 &&
                      std::abs(l0.pa.y() - cn5.center().y()) < 0.01 &&
                      std::abs(l0.pb.x() - j5.left()) < 0.01,
                  "de conector a conector: sale por el lado de CN5 que mira al shield y entra "
                  "por el de J5 que mira a la Nucleo");
        const VistaPlaca::Linea& l1 = v->lineas()[1];
        const QRectF p7 = v->caja_de("N/CN5.7"), p8 = v->caja_de("S/J5.8");
        comprueba(l1.a == "N/CN5.7" && std::abs(l1.pa.x() - p7.right()) < 0.01 &&
                      std::abs(l1.pb.x() - p8.left()) < 0.01 &&
                      l1.item->pen().style() == Qt::DashLine,
                  "el hilo, de pin a pin: del 7 de CN5 al 8 de J5");
        comprueba(l0.item->childItems().size() == 2 && l1.item->childItems().isEmpty(),
                  "la del acople lleva un punto en cada conector; la del hilo, ninguno: llega "
                  "al borde de cada pin y no lo tapa, que se vea como sus vecinos");
        const QPointF medio = l0.item->path().pointAtPercent(0.5);
        const QImage oculta = v->imagen(1200);
        QToolButton* bn = il.boton_conexiones("N");
        QToolButton* bs = il.boton_conexiones("S");
        comprueba(!l0.item->isVisible() && !l1.item->isVisible() && bn && bs &&
                      bn == il.findChild<QToolButton*>("conexiones:N") && bn->isCheckable() &&
                      !bn->isChecked() && !bs->isChecked() && bn->isEnabled() &&
                      !bn->icon().isNull() &&
                      !parecido(en(oculta, *v, medio.x(), medio.y()), qRgb(0xd9, 0x48, 0x1c), 70),
                  "las LINEAS NACEN ESCONDIDAS: cada placa tiene su boton con icono, apagado, "
                  "y a mitad de camino no hay linea");
        bn->click();
        comprueba(bn->isChecked() && v->lineas_visibles("N") && !v->lineas_visibles("S") &&
                      l0.item->isVisible() && l1.item->isVisible(),
                  "el boton de N las enseña: las dos son suyas");
        l1.item->setVisible(false);          // va casi por el mismo sitio
        const QImage img = v->imagen(1200);
        l1.item->setVisible(true);
        comprueba(parecido(en(img, *v, medio.x(), medio.y()), qRgb(0xd9, 0x48, 0x1c), 70),
                  "y se ve: a mitad de camino, el color de la linea, " +
                      hex(en(img, *v, medio.x(), medio.y())).toStdString());
        bs->click();
        bn->click();
        comprueba(!v->lineas_visibles("N") && v->lineas_visibles("S") && l0.item->isVisible() &&
                      l1.item->isVisible(),
                  "con el de S y sin el de N, se siguen viendo: tambien son de S");
        bs->click();
        comprueba(!l0.item->isVisible() && !l1.item->isVisible(),
                  "y sin ninguno de los dos, escondidas otra vez");
        bn->click();

        // Un dibujo de verdad en N: a su escala -70 mm en 70 unidades- y las
        // lineas, a su CN5
        QString e;
        comprueba(il.pon_dibujo("N", DIBUJO_NUCLEO, {}, e) && v->origen("N") == "svg",
                  "el dibujo de la Nucleo, en N");
        n = v->en_escena("N");
        sr = v->en_escena("S");
        const QRectF cn5b = v->caja_de("N/CN5");
        comprueba(std::abs(n.width() - 70) < 0.01 && std::abs(sr.left() - n.right() - 30) < 0.01 &&
                      v->lineas().size() == 2 && std::abs(v->lineas()[0].pa.x() - cn5b.right()) < 0.01 &&
                      std::abs(v->lineas()[0].pa.y() - cn5b.center().y()) < 0.01,
                  "lo pone a su tamano, recoloca el shield, y la linea sale ahora del CN5 del "
                  "dibujo");
        comprueba(v->lineas()[1].a == "N/CN5.7" &&
                      std::abs(v->lineas()[1].pa.x() - cn5b.right()) < 0.01 &&
                      v->lineas()[1].item->childItems().size() == 1,
                  "el hilo, sin un CN5.7 dibujado, sale de CN5, y ahi si lleva su punto -en "
                  "el pin 8 de J5, que esta dibujado, no-");
        comprueba(v->lineas()[0].item->isVisible() && v->lineas()[1].item->isVisible(),
                  "las lineas trazadas de nuevo con el dibujo nuevo se siguen viendo: el boton "
                  "de N sigue encendido");
        comprueba(!il.informe("N").avisos.join(" ").contains("no dice su tamano"),
                  "un dibujo que si dice sus milimetros no avisa de su tamano");
        const QByteArray sin_mm = QByteArray(DIBUJO_NUCLEO).replace(" width=\"70mm\" height=\"80mm\"", "");
        il.pon_dibujo("N", sin_mm, {}, e);
        n = v->en_escena("N");
        sr = v->en_escena("S");
        comprueba(il.informe("N").avisos.join(" ").contains("no dice su tamano") &&
                      std::abs(n.height() - sr.height()) < 0.01,
                  "y uno que no lo dice se iguala en altura a la placa que si lo dice, y se "
                  "avisa");

        // Una pila: cada conector con el siguiente, y el hilo de un pin del chip
        const char* CAT_PILA =
            "<catalogo>\n"
            "  <pieza idx=\"0\" id=\"CPU/J1\" tipo=\"Conector\"/>\n"
            "  <pieza idx=\"1\" id=\"L1/J1\" tipo=\"Conector\"/>\n"
            "  <pieza idx=\"2\" id=\"L2/J1\" tipo=\"Conector\"/>\n"
            "  <pieza idx=\"3\" id=\"L2/J9\" tipo=\"Conector\"/>\n"
            "</catalogo>\n";
        const PlacaGui pila = placa_de(PILA_XML, CAT_PILA);
        VistaIlustracion ip(pila);
        VistaPlaca* vp = ip.vista("CPU");
        const QRectF cpu = vp->en_escena("CPU"), rl1 = vp->en_escena("L1"),
                     rl2 = vp->en_escena("L2");
        comprueba(rl1.left() > cpu.right() && rl2.left() > rl1.right(),
                  "la pila, CPU, L1 y L2 de izquierda a derecha");
        QStringList pares;
        for (const VistaPlaca::Linea& l : vp->lineas())
            pares << l.a + (l.hilo ? " ~ " : " - ") + l.b;
        comprueba(pares == QStringList({"CPU/J1 - L1/J1", "L1/J1 - L2/J1",
                                        "CPU/u0.PA2 ~ L2/J9.1"}) &&
                      vp->lineas()[0].item->toolTip().endsWith("(en pila)"),
                  "tres lineas: cada conector de la pila con el siguiente, y el hilo: \"" +
                      pares.join(", ").toStdString() + "\"");
        const VistaPlaca::Linea& h = vp->lineas()[2];
        comprueba(std::abs(h.pa.x() - cpu.right()) < 0.01 &&
                      std::abs(h.pa.y() - cpu.center().y()) < 0.01 &&
                      std::abs(h.pb.x() - vp->caja_de("L2/J9.1").left()) < 0.01,
                  "un pin de chip no esta dibujado: el hilo sale del borde de la CPU, y llega "
                  "al pin 1 de J9 de L2");
        ip.boton_conexiones("CPU")->click();
        comprueba(vp->lineas()[0].item->isVisible() && !vp->lineas()[1].item->isVisible() &&
                      vp->lineas()[2].item->isVisible(),
                  "el boton de CPU enseña sus dos lineas -su acople con L1 y su hilo a L2- y no "
                  "la de L1 con L2");
        {
            // Una tercera placa, Z, sin nada que la una a las otras
            QByteArray sz(SISTEMA_XML);
            sz.replace("  <mcu tipo=", "  <placa id=\"Z\" nombre=\"suelta\" fichero=\"z.xml\"/>\n  <mcu tipo=");
            const PlacaGui pz = placa_de(sz.constData(), CATALOGO_SISTEMA_XML);
            VistaIlustracion iz(pz);
            QToolButton* bz = iz.boton_conexiones("Z");
            comprueba(pz.subplaca("Z") && bz && !bz->isEnabled() &&
                          bz->toolTip().contains("no esta unida") &&
                          iz.boton_conexiones("N")->isEnabled(),
                      "una placa que no esta unida a ninguna otra tiene el boton apagado");
            VistaIlustracion suelta(placa_de(PLACA_MANDOS, CATALOGO_MANDOS));
            comprueba(!suelta.boton_conexiones(QString()) &&
                          !suelta.findChild<QToolButton*>("conexiones:"),
                      "y una placa suelta no lo lleva: no tiene con quien unirse");
        }
        ip.boton_conexiones("CPU")->click();
        ip.boton_conexiones("L1")->click();
        comprueba(vp->lineas()[0].item->isVisible() && vp->lineas()[1].item->isVisible() &&
                      !vp->lineas()[2].item->isVisible(),
                  "el de L1, sus dos acoples, y no el hilo, que no es suyo");
    }

    // -------------------------------------------------------------------------
    std::printf("I12 El mando de un encoder: el giro, y la rueda que atraviesa\n");
    {
        const char* PLACA_ENC =
            "<placa nombre=\"ky\">\n"
            "  <componente tipo=\"Encoder\" id=\"ENC\"><pin nombre=\"a\" nodo=\"PA0\"/></componente>\n"
            "  <componente tipo=\"Button\" id=\"SW1\"><pin nombre=\"pin\" nodo=\"PA1\"/></componente>\n"
            "  <componente tipo=\"Encoder\" id=\"MUDO\"><pin nombre=\"a\" nodo=\"PA2\"/></componente>\n"
            "  <ilustracion>\n"
            "    <enlace pieza=\"ENC\" elemento=\"ENC\" efecto=\"giro\"/>\n"
            "    <enlace pieza=\"MUDO\" elemento=\"MUDO\" efecto=\"giro\"/>\n"
            "  </ilustracion>\n"
            "</placa>\n";
        const char* CAT_ENC =
            "<catalogo>\n"
            "  <pieza idx=\"0\" id=\"ENC\" tipo=\"Encoder\">\n"
            "    <observable idx=\"0\" id_obs=\"0\" nombre=\"posicion\" unidad=\"\" min=\"0\" max=\"29\" interesante=\"si\"/>\n"
            "    <observable idx=\"1\" id_obs=\"1\" nombre=\"contacto_a\" unidad=\"\" min=\"0\" max=\"1\" interesante=\"no\"/>\n"
            "    <mando idx=\"0\" nombre=\"girar\" tipo=\"discreto\" min=\"-30000\" max=\"30000\" valor=\"0\"/>\n"
            "  </pieza>\n"
            "  <pieza idx=\"1\" id=\"SW1\" tipo=\"Button\">\n"
            "    <observable idx=\"0\" id_obs=\"2\" nombre=\"pulsado\" unidad=\"\" min=\"0\" max=\"1\" interesante=\"si\"/>\n"
            "    <mando idx=\"0\" nombre=\"pulsar\" tipo=\"boton\" min=\"0\" max=\"1\" valor=\"0\"/>\n"
            "  </pieza>\n"
            "  <pieza idx=\"2\" id=\"MUDO\" tipo=\"Encoder\">\n"
            "    <observable idx=\"0\" id_obs=\"3\" nombre=\"contacto_a\" unidad=\"\" min=\"0\" max=\"1\" interesante=\"si\"/>\n"
            "  </pieza>\n"
            "</catalogo>\n";
        // El anillo, un circulo con su marca arriba, y la tapa encima, en el
        // centro; MUDO, a un lado
        const char* DIBUJO_ENC =
            "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 140 100\">\n"
            "  <rect x=\"0\" y=\"0\" width=\"140\" height=\"100\" fill=\"#0d2350\"/>\n"
            "  <g id=\"ENC\">\n"
            "    <circle cx=\"50\" cy=\"50\" r=\"40\" fill=\"#2b2b2b\"/>\n"
            "    <rect x=\"47\" y=\"12\" width=\"6\" height=\"16\" fill=\"#ffffff\"/>\n"
            "  </g>\n"
            "  <circle id=\"SW1\" cx=\"50\" cy=\"50\" r=\"18\" fill=\"#b4bbc2\"/>\n"
            "  <rect id=\"MUDO\" x=\"110\" y=\"40\" width=\"20\" height=\"20\" fill=\"#808080\"/>\n"
            "</svg>\n";
        const PlacaGui p = placa_de(PLACA_ENC, CAT_ENC);
        QString e;
        std::unique_ptr<VistaPlaca> v(VistaPlaca::crea(p, QString(), DIBUJO_ENC, p.tabla_de(QString()), e));
        comprueba(v != nullptr, "la vista se construye " + e.toStdString());
        if (!v) return resultado();
        v->resize(700, 500);
        v->show();
        espera([] { return false; }, 50);
        comprueba(v->efecto(0) == "giro" && v->efecto(1) == "hundido" &&
                      v->efecto(2) == "ninguno" &&
                      v->informe().avisos.join(" ").contains("MUDO: el efecto «giro» necesita"),
                  "ENC gira y SW1 se hunde; MUDO, sin numerico que lo mueva, sin efecto, y "
                  "se dice: " + v->informe().avisos.join(" | ").toStdString());
        const QPointF centro = v->vivo(0)->transformOriginPoint() -
                               v->vivo(0)->boundingRect().center();
        comprueba(std::abs(centro.x()) < 1e-9 && std::abs(centro.y()) < 1e-9 &&
                      v->vivo(0)->rotation() == 0,
                  "el anillo gira sobre el centro de su caja, que es el eje, y nace sin girar");
        auto marca = [&](const QImage& img, double x, double y) {
            return luz(en(img, *v, x, y)) > 600;
        };
        v->pon_valor(0, 0.f);
        const QImage cero = v->imagen(560);
        v->pon_valor(0, 15.f);
        const QImage quince = v->imagen(560);
        v->pon_valor(0, 7.5f);
        const double r75 = v->vivo(0)->rotation();
        v->pon_valor(0, 29.f);
        const double r29 = v->vivo(0)->rotation();
        comprueba(marca(cero, 50, 20) && !marca(cero, 50, 80) && !marca(quince, 50, 20) &&
                      marca(quince, 50, 80),
                  "la marca, arriba en la posicion 0 y abajo en la 15: media vuelta de 30");
        comprueba(std::abs(r75 - 90) < 1e-9 && std::abs(r29 - 348) < 1e-9,
                  "12 grados por posicion, a la derecha: 7.5 son 90 y 29 son 348 (" +
                      std::to_string(r75) + ", " + std::to_string(r29) + ")");
        comprueba(!v->etiqueta(0) && v->vivo(0)->toolTip().contains("posicion: 29"),
                  "la posicion no lleva etiqueta -ya la dice el giro-, y la ayuda la dice");

        std::vector<OrdenVista> o;
        QObject::connect(v.get(), &VistaPlaca::orden,
                         [&](quint16 pz, quint16 m, float x) { o.push_back({pz, m, x}); });
        v->activa_mandos(true);
        const QPoint tapa = v->donde(1);
        const QPoint anillo = v->mapFromScene(v->vivo(0)->mapToScene(QPointF(50, 15)));
        clic(v.get(), tapa);
        comprueba(o.size() == 2 && o[0].pieza == 1 && o[0].valor == 1.f && o[1].valor == 0.f,
                  "un clic en la tapa es para el pulsador: pulsar y soltar");
        o.clear();
        rueda(v.get(), tapa, 1);
        rueda(v.get(), tapa, 1);
        comprueba(o.size() == 2 && o[0].pieza == 0 && o[0].mando == 0 && o[0].valor == 1.f &&
                      o[1].valor == 2.f,
                  "la rueda sobre la tapa ATRAVIESA el pulsador -no tiene mando que mover- "
                  "y gira el encoder, de clic en clic");
        rueda(v.get(), anillo, -3);
        comprueba(o.size() == 3 && o[2].pieza == 0 && o[2].valor == -1.f,
                  "y sobre el anillo, igual: tres atras, a -1");
    }

    // -------------------------------------------------------------------------
    std::printf("I13 Una pantalla: la imagen por T_IMAGEN, encima de su elemento y en el panel\n");
    {
        const char* PLACA_TFT =
            "<placa nombre=\"tft\">\n"
            "  <componente tipo=\"Tft128x160\" id=\"TFT\"><pin nombre=\"sda\" nodo=\"PA7\"/></componente>\n"
            "  <componente tipo=\"Led\" id=\"LD\"><pin nombre=\"anodo\" nodo=\"PA5\"/></componente>\n"
            "</placa>\n";
        const char* CAT_TFT =
            "<catalogo>\n"
            "  <pieza idx=\"0\" id=\"TFT\" tipo=\"Tft128x160\">\n"
            "    <observable idx=\"0\" id_obs=\"0\" nombre=\"encendida\" unidad=\"\" min=\"0\" max=\"1\" interesante=\"si\"/>\n"
            "    <observable idx=\"1\" id_obs=\"1\" nombre=\"luz\" unidad=\"mA\" min=\"0\" max=\"40\" interesante=\"no\"/>\n"
            "    <imagen idx=\"0\" id_obs=\"3\" nombre=\"pantalla\" ancho=\"128\" alto=\"160\" formato=\"rgb888\"/>\n"
            "  </pieza>\n"
            "  <pieza idx=\"1\" id=\"LD\" tipo=\"Led\">\n"
            "    <observable idx=\"0\" id_obs=\"2\" nombre=\"encendido\" unidad=\"\" min=\"0\" max=\"1\" interesante=\"si\"/>\n"
            "  </pieza>\n"
            "</catalogo>\n";
        // El vidrio, APAISADO: 160 de ancho y 128 de alto
        const char* DIBUJO_TFT =
            "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 200 160\">\n"
            "  <rect x=\"0\" y=\"0\" width=\"200\" height=\"160\" fill=\"#c8161d\"/>\n"
            "  <rect id=\"TFT\" x=\"20\" y=\"16\" width=\"160\" height=\"128\" fill=\"#2a3036\"/>\n"
            "  <circle id=\"LD\" cx=\"190\" cy=\"10\" r=\"4\" fill=\"#00ff00\"/>\n"
            "</svg>\n";
        // La imagen: la fila de arriba roja, la columna izquierda verde, y el
        // resto azul
        QByteArray rgb(128 * 160 * 3, '\0');
        for (int y = 0; y < 160; ++y)
            for (int x = 0; x < 128; ++x) {
                char* p = rgb.data() + (y * 128 + x) * 3;
                if (y < 8)      { p[0] = char(255); }
                else if (x < 8) { p[1] = char(255); }
                else            { p[2] = char(255); }
            }
        const PlacaGui p = placa_de(PLACA_TFT, CAT_TFT);
        comprueba(p.piezas.size() == 2 && p.piezas[0].imagenes.size() == 1 &&
                      p.piezas[0].imagenes[0].id_obs == 3 && p.piezas[0].imagenes[0].ancho == 128 &&
                      p.piezas[0].imagenes[0].alto == 160 && p.piezas[1].imagenes.isEmpty(),
                  "el catalogo trae la imagen de la pantalla, con su id_obs y su tamano");
        comprueba(glifo_de(p.piezas[0]) == "pantalla" &&
                      para_bandeja(p, QString(), {"TFT"}) == QVector<int>{0} &&
                      dibujo_generado(p, QString()).contains("fill=\"#1b1e22\""),
                  "en un dibujo generado, su vidrio oscuro; y va a la bandeja si al dibujo le falta");
        QString e;
        std::unique_ptr<VistaPlaca> v(VistaPlaca::crea(p, QString(), DIBUJO_TFT, {}, e));
        comprueba(v != nullptr, "la vista se construye " + e.toStdString());
        if (!v) return resultado();
        v->resize(600, 500);
        v->show();
        espera([] { return false; }, 50);
        comprueba(v->efecto(0) == "pantalla" && v->pantalla(0) && !v->pantalla(0)->isVisible() &&
                      v->observados().contains(3) && v->efecto(1) == "brillo" && !v->pantalla(1),
                  "una pieza con imagen lleva el efecto `pantalla` sin que lo diga la tabla; se "
                  "suscribe a su imagen, y hasta la primera se ve el elemento");
        v->pon_imagen(3, 128, 160, rgb, 1.f);
        const QImage con_luz = v->imagen(600);
        comprueba(v->pantalla(0)->isVisible() &&
                      parecido(en(con_luz, *v, 22, 80), qRgb(255, 0, 0), 10) &&
                      parecido(en(con_luz, *v, 100, 141), qRgb(0, 255, 0), 10) &&
                      parecido(en(con_luz, *v, 100, 60), qRgb(0, 0, 255), 10),
                  "la imagen, sobre el vidrio apaisado, girada a la izquierda: su fila de "
                  "arriba -roja- a la izquierda, su columna izquierda -verde- abajo, el resto "
                  "azul: " + hex(en(con_luz, *v, 22, 80)).toStdString() + " " +
                      hex(en(con_luz, *v, 100, 141)).toStdString());
        comprueba(parecido(en(con_luz, *v, 10, 80), qRgb(0xc8, 0x16, 0x1d), 10),
                  "y fuera del vidrio, la placa, sin tocar");
        v->pon_imagen(3, 128, 160, rgb, 0.f);
        const QImage sin_luz = v->imagen(600);
        v->pon_imagen(3, 128, 160, rgb, 0.5f);
        const QImage media = v->imagen(600);
        comprueba(parecido(en(sin_luz, *v, 100, 60), qRgb(0, 0, 0), 4) &&
                      parecido(en(media, *v, 100, 60), qRgb(0, 0, 128), 6),
                  "sin luz, negra; con la mitad, a la mitad");
        v->pon_imagen(3, 128, 160, rgb.left(1000), 1.f);
        comprueba(parecido(en(v->imagen(600), *v, 100, 60), qRgb(0, 0, 128), 6),
                  "una imagen con menos pixeles de los que dice no se pinta: se queda la de antes");

        // La placa montada con giro="90": mcu-sim manda el dibujo dentro de un
        // grupo que lo gira. La imagen va con el vidrio: su fila de arriba,
        // que estaba a la izquierda, queda ARRIBA
        {
            QString dib = QString::fromUtf8(DIBUJO_TFT);
            dib.replace("viewBox=\"0 0 200 160\">", "viewBox=\"0 0 160 200\">\n"
                        "<g transform=\"translate(160 0) rotate(90)\">");
            dib.replace("</svg>", "</g>\n</svg>");
            std::unique_ptr<VistaPlaca> g(
                VistaPlaca::crea(p, QString(), dib.toUtf8(), {}, e));
            comprueba(g != nullptr, "con el dibujo girado, la vista se construye");
            if (!g) return resultado();
            g->resize(500, 600);
            g->show();
            espera([] { return false; }, 50);
            g->pon_imagen(3, 128, 160, rgb, 1.f);
            const QImage gi = g->imagen(500);
            comprueba(parecido(en(gi, *g, 80, 22), qRgb(255, 0, 0), 10) &&
                          parecido(en(gi, *g, 19, 100), qRgb(0, 255, 0), 10) &&
                          parecido(en(gi, *g, 80, 100), qRgb(0, 0, 255), 10),
                      "con la placa girada 90 grados la imagen gira con ella: la fila de "
                      "arriba, arriba; la columna izquierda, a la izquierda: " +
                          hex(en(gi, *g, 80, 22)).toStdString() + " " +
                          hex(en(gi, *g, 19, 100)).toStdString());
        }

        // En la ventana: la suscripción, el panel y el dibujo
        VentanaPrincipal w(0);
        w.show();
        ModeloFalso m;
        m.conecta(w.sesion().puerto());
        m.manda(T_HOLA, "protocolo_max=1\nplaca=tft.xml\nmcu=\nfirmware=\n");
        comprueba(m.espera_leidos(1) && m.leido[0].tipo == T_VERSION, "el saludo");
        m.version(1);
        m.manda(T_PLACA, PLACA_TFT);
        m.manda(T_CATALOGO, CAT_TFT);
        m.manda(T_LISTO);
        CabSuscribe cs{};
        comprueba(m.espera_leidos(2) && m.leido[1].tipo == T_SUSCRIBE &&
                      (std::memcpy(&cs, m.leido[1].cuerpo.constData(), sizeof cs), true) &&
                      m.leido[1].cuerpo.mid(int(sizeof cs)).contains(
                          QByteArray::fromStdString(bytes(uint16_t(3)))),
                  "la suscripcion pide la imagen, con su id_obs, como un observable");
        auto* lab = w.findChild<QLabel*>("img:3");
        comprueba(lab && lab->size() == QSize(128, 160) && lab->pixmap().isNull(),
                  "el panel tiene sitio para la pantalla, a su tamano, negra hasta la primera");
        w.abre_dibujo(QString(), DIBUJO_TFT);
        auto* ilus = w.findChild<VistaIlustracion*>("ilustracion");
        VistaPlaca* vp = ilus ? ilus->vista(QString()) : nullptr;
        std::string im = bytes(CabImagen{7000000ull, 3, FMT_RGB888, 128, 160, 1.f, 0});
        im += rgb.toStdString();
        m.manda(T_IMAGEN, im);
        comprueba(lab && espera([&] { return !lab->pixmap().isNull(); }) &&
                      lab->pixmap().toImage().pixel(64, 2) == qRgb(255, 0, 0) &&
                      lab->pixmap().toImage().pixel(2, 80) == qRgb(0, 255, 0),
                  "un T_IMAGEN llega al panel, tal cual: la fila de arriba roja");
        comprueba(vp && vp->pantalla(0) && vp->pantalla(0)->isVisible(),
                  "y al dibujo, encima del vidrio");
        std::string mala = bytes(CabImagen{8000000ull, 3, FMT_RGB888, 128, 160, 0.f, 0}) + "corto";
        m.manda(T_IMAGEN, mala);
        espera([] { return false; }, 200);
        comprueba(lab && lab->pixmap().toImage().pixel(64, 2) == qRgb(255, 0, 0) &&
                      w.statusBar()->currentMessage().contains("no mide lo que dice"),
                  "un T_IMAGEN que no mide lo que dice se ignora, y se dice");
    }

    // -------------------------------------------------------------------------
    std::printf("I14 El aspa de un servo: el efecto angulo\n");
    {
        const char* PLACA_SERVO =
            "<placa nombre=\"servo\">\n"
            "  <componente tipo=\"Servo\" id=\"SERVO\"><pin nombre=\"pwm\" nodo=\"PA0\"/></componente>\n"
            "  <componente tipo=\"Led\" id=\"MUDO\"><pin nombre=\"anodo\" nodo=\"PA1\"/></componente>\n"
            "  <ilustracion>\n"
            "    <enlace pieza=\"MUDO\" elemento=\"MUDO\" efecto=\"angulo\"/>\n"
            "  </ilustracion>\n"
            "</placa>\n";
        const char* CAT_SERVO =
            "<catalogo>\n"
            "  <pieza idx=\"0\" id=\"SERVO\" tipo=\"Servo\">\n"
            "    <observable idx=\"0\" id_obs=\"0\" nombre=\"angulo\" unidad=\"°\" min=\"-90\" max=\"90\" interesante=\"si\"/>\n"
            "    <observable idx=\"1\" id_obs=\"1\" nombre=\"pulso\" unidad=\"us\" min=\"0\" max=\"3000\" interesante=\"no\"/>\n"
            "    <mando idx=\"0\" nombre=\"bloquear\" tipo=\"interruptor\" min=\"0\" max=\"1\" valor=\"0\"/>\n"
            "  </pieza>\n"
            "  <pieza idx=\"1\" id=\"MUDO\" tipo=\"Led\">\n"
            "    <observable idx=\"0\" id_obs=\"2\" nombre=\"corriente\" unidad=\"mA\" min=\"0\" max=\"25\" interesante=\"si\"/>\n"
            "  </pieza>\n"
            "</catalogo>\n";
        // El aspa: una sola pala, hacia arriba desde el eje (50, 50), y un
        // circulo SIN PINTAR centrado en el eje que pone ahi el centro de la caja
        const char* DIBUJO_SERVO =
            "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 140 100\">\n"
            "  <rect x=\"0\" y=\"0\" width=\"140\" height=\"100\" fill=\"#1e5631\"/>\n"
            "  <g id=\"SERVO\">\n"
            "    <circle cx=\"50\" cy=\"50\" r=\"42\" fill=\"none\" stroke=\"none\"/>\n"
            "    <rect x=\"46\" y=\"12\" width=\"8\" height=\"40\" fill=\"#ffffff\"/>\n"
            "  </g>\n"
            "  <rect id=\"MUDO\" x=\"110\" y=\"40\" width=\"20\" height=\"20\" fill=\"#808080\"/>\n"
            "</svg>\n";
        const PlacaGui p = placa_de(PLACA_SERVO, CAT_SERVO);
        QString e;
        std::unique_ptr<VistaPlaca> v(VistaPlaca::crea(p, QString(), DIBUJO_SERVO, p.tabla_de(QString()), e));
        comprueba(v != nullptr, "la vista se construye " + e.toStdString());
        if (!v) return resultado();
        v->resize(700, 500);
        v->show();
        espera([] { return false; }, 50);
        comprueba(v->efecto(0) == "angulo" && v->efecto(1) == "ninguno" &&
                      v->informe().avisos.join(" ").contains("MUDO: el efecto «angulo» necesita"),
                  "el servo, que sugiere un angulo en grados, gira sin que nadie lo diga; el LED, "
                  "con la tabla pidiendolo y sin grados, sin efecto, y se dice: " +
                      v->informe().avisos.join(" | ").toStdString());
        const QRectF caja = v->vivo(0)->boundingRect();
        const QPointF origen = v->vivo(0)->transformOriginPoint();
        comprueba(std::abs(caja.width() - 84) < 0.5 && std::abs(caja.height() - 84) < 0.5 &&
                      origen == caja.center() && v->vivo(0)->rotation() == 0,
                  "la caja es la del circulo sin pintar, 84 x 84 -sin el, la de la pala, 8 x 40-, "
                  "y gira sobre su centro, que es el eje: " + std::to_string(caja.width()) + " x " +
                      std::to_string(caja.height()));
        auto pala = [&](const QImage& img, double x, double y) {
            return luz(en(img, *v, x, y)) > 600;
        };
        v->pon_valor(0, 0.f);
        const QImage cero = v->imagen(560);
        v->pon_valor(0, 90.f);
        const QImage noventa = v->imagen(560);
        const double r90 = v->vivo(0)->rotation();
        v->pon_valor(0, -45.f);
        const QImage menos45 = v->imagen(560);
        const double rm45 = v->vivo(0)->rotation();
        comprueba(pala(cero, 50, 20) && !pala(cero, 80, 50) && pala(noventa, 80, 50) &&
                      !pala(noventa, 50, 20) && pala(menos45, 30, 30) && !pala(menos45, 70, 30),
                  "la pala, arriba en el 0, a la derecha en +90 y arriba a la izquierda en -45");
        comprueba(r90 == 90 && rm45 == -45,
                  "los grados, tal cual: +90 son 90 y -45 son -45 (" + std::to_string(r90) +
                      ", " + std::to_string(rm45) + ")");
        comprueba(!v->etiqueta(0) && v->vivo(0)->toolTip().contains("angulo: -45"),
                  "el angulo no lleva etiqueta -ya lo dice el aspa-, y la ayuda lo dice: " +
                      v->vivo(0)->toolTip().toStdString());
    }

    // -------------------------------------------------------------------------
    std::printf("I15 El modo de edicion: mover y girar las placas con el raton\n");
    {
        QByteArray sx(SISTEMA_XML);
        sx.replace("<placa id=\"N\" nombre=\"nucleo-f446re\" fichero=\"nucleo_f446re.xml\"/>",
                   "<placa id=\"N\" nombre=\"nucleo-f446re\" fichero=\"nucleo_f446re.xml\">\n"
                   "    <conector ref=\"N/CN5\" filas=\"1\" columnas=\"10\" numeracion=\"zigzag\" acople=\"0\"/>\n"
                   "  </placa>");
        sx.replace("<placa id=\"S\" nombre=\"shield-leds\" fichero=\"shield_leds.xml\"/>",
                   "<placa id=\"S\" nombre=\"shield-leds\" fichero=\"shield_leds.xml\">\n"
                   "    <conector ref=\"S/J5\" filas=\"1\" columnas=\"10\" numeracion=\"zigzag\" acople=\"0\"/>\n"
                   "  </placa>");
        const PlacaGui s = placa_de(sx.constData(), CATALOGO_SISTEMA_XML);
        VistaIlustracion il(s);
        il.resize(900, 500);
        il.show();
        espera([] { return false; }, 50);
        VistaPlaca* v = il.vista("N");
        auto* boton = il.findChild<QToolButton*>("edicion");
        auto* ayuda = il.findChild<QLabel*>("ayuda_edicion");
        comprueba(v && boton && ayuda && boton->text() == QString::fromUtf8("Edición") &&
                      boton->isCheckable() && !boton->isChecked() && !ayuda->isVisible() &&
                      !v->edicion(),
                  "arriba, el boton Edicion, sin pulsar");
        if (!v || !boton) return resultado();
        il.activa_mandos(true);
        const QRectF n0 = v->en_escena("N"), s0 = v->en_escena("S");
        const double mm = v->mm_escena();
        int cambios = 0;
        QObject::connect(v, &VistaPlaca::disposicion_cambiada, [&] { ++cambios; });
        boton->click();
        comprueba(boton->isChecked() && il.edicion() && v->edicion() && ayuda->isVisible() &&
                      !v->mandos_activos(),
                  "pulsado, se queda pulsado: el modo de edicion, su ayuda, y los mandos "
                  "apagados aunque el modelo corra");

        // Arrastrar S
        const QPoint ini = v->mapFromScene(s0.center());
        const QPoint fin = ini + QPoint(37, 23);
        const QPointF d = (v->mapToScene(fin) - v->mapToScene(ini)) * mm;
        const QPointF esperada(std::round(s0.left() * mm + d.x()), std::round(s0.top() * mm + d.y()));
        raton(v, QEvent::MouseButtonPress, ini);
        raton(v, QEvent::MouseMove, ini + QPoint(10, 5));
        raton(v, QEvent::MouseMove, fin);
        raton(v, QEvent::MouseButtonRelease, fin);
        const QRectF s1 = v->en_escena("S");
        comprueba(std::abs(s1.left() * mm - esperada.x()) < 1e-6 &&
                      std::abs(s1.top() * mm - esperada.y()) < 1e-6 &&
                      std::abs(s1.width() - s0.width()) < 1e-9 && v->en_escena("N") == n0 &&
                      v->ajuste("S").fija && v->ajuste("N").fija && cambios == 1,
                  "arrastrar S la mueve, y se queda en milimetros enteros (" +
                      std::to_string(s1.left() * mm) + ", " + std::to_string(s1.top() * mm) +
                      "); N, quieta y fija desde ahora");
        il.boton_conexiones("N")->click();
        const QRectF j5 = v->caja_de("S/J5");
        const VistaPlaca::Linea& l0 = v->lineas()[0];
        comprueba(l0.item->isVisible() && std::abs(l0.pb.x() - j5.left()) < 1e-6 &&
                      std::abs(l0.pb.y() - j5.center().y()) < 1e-6,
                  "la linea del acople sigue a J5, donde ha quedado");

        // Girar: doble clic, Mayus+rueda y el menu
        const QPointF c1 = s1.center();
        raton(v, QEvent::MouseButtonDblClick, v->mapFromScene(c1));
        const QRectF s2 = v->en_escena("S");
        comprueba(v->ajuste("S").giro == 90 && std::abs(s2.width() - s1.height()) < 1e-6 &&
                      std::abs(s2.height() - s1.width()) < 1e-6 &&
                      std::abs(s2.center().x() - c1.x()) < 1e-6 &&
                      std::abs(s2.center().y() - c1.y()) < 1e-6 && cambios == 2,
                  "un doble clic la gira 90 grados a la derecha, sobre su centro");
        const QRectF j5g = v->caja_de("S/J5");
        const VistaPlaca::Linea& l1 = v->lineas()[0];
        const bool en_borde = std::abs(l1.pb.x() - j5g.left()) < 1e-6 ||
                              std::abs(l1.pb.x() - j5g.right()) < 1e-6 ||
                              std::abs(l1.pb.y() - j5g.top()) < 1e-6 ||
                              std::abs(l1.pb.y() - j5g.bottom()) < 1e-6;
        comprueba(j5g.height() > j5g.width() && en_borde,
                  "J5 queda de pie, y la linea sigue llegando a su borde");
        {
            QWheelEvent e(QPointF(v->mapFromScene(c1)),
                          QPointF(v->viewport()->mapToGlobal(v->mapFromScene(c1))), QPoint(),
                          QPoint(0, 120), Qt::NoButton, Qt::ShiftModifier, Qt::NoScrollPhase,
                          false);
            QCoreApplication::sendEvent(v->viewport(), &e);
        }
        comprueba(v->ajuste("S").giro == 0 && v->en_escena("S").size() == s1.size(),
                  "Mayus+rueda hacia arriba, a la izquierda: otra vez derecha");
        {
            const QPoint p = v->mapFromScene(c1);
            QContextMenuEvent e(QContextMenuEvent::Mouse, p, v->viewport()->mapToGlobal(p));
            QCoreApplication::sendEvent(v->viewport(), &e);
        }
        QMenu* menu = v->menu_abierto();
        QAction* izq = menu ? menu->findChild<QAction*>("gira_izquierda") : nullptr;
        comprueba(menu && menu->objectName() == "placa:S" && izq &&
                      menu->findChild<QAction*>("gira_derecha") &&
                      menu->findChild<QAction*>("restablece_todas"),
                  "el boton derecho, el menu de la placa: los dos giros y colocarlas");
        if (izq) izq->trigger();
        comprueba(v->ajuste("S").giro == 270, "girar a la izquierda: 270");

        // Una rueda sin Mayus y un clic no hacen nada mas: no hay mandos
        rueda(v, v->mapFromScene(c1), 2);
        comprueba(v->ajuste("S").giro == 270, "la rueda sola no gira la placa");

        if (menu) menu->close();
        v->restablece_todas();
        comprueba(v->en_escena("N") == n0 && v->en_escena("S") == s0 &&
                      !v->ajuste("S").fija && v->ajuste("S").giro == 0,
                  "colocar todas como al principio: donde estaban");
        boton->click();
        comprueba(!boton->isChecked() && !il.edicion() && !v->edicion() && !ayuda->isVisible() &&
                      v->mandos_activos(),
                  "y al volver a pulsarlo, sin pulsar: fuera del modo, y los mandos otra vez");
    }
    {
        // Los mandos, apagados en la edicion y de vuelta al salir
        const PlacaGui p = placa_de(PLACA_MANDOS, CATALOGO_MANDOS);
        QString e;
        std::unique_ptr<VistaPlaca> v(VistaPlaca::crea(p, QString(), DIBUJO_MANDOS, {}, e));
        v->resize(800, 200);
        v->show();
        espera([] { return false; }, 50);
        std::vector<OrdenVista> o;
        QObject::connect(v.get(), &VistaPlaca::orden,
                         [&](quint16 pz, quint16 m, float x) { o.push_back({pz, m, x}); });
        v->activa_mandos(true);
        v->pon_edicion(true);
        const QPoint sw = v->donde(1);
        clic(v.get(), sw);
        rueda(v.get(), v->donde(2), 1);
        comprueba(o.empty(), "en la edicion, un clic en un interruptor o la rueda en un "
                             "mando no ordenan nada");
        v->pon_edicion(false);
        clic(v.get(), sw);
        comprueba(o.size() == 1 && o[0].pieza == 1, "fuera de ella, si");
    }
    {
        // Una placa suelta girada: la etiqueta, derecha y bajo su pieza
        const char* PLACA_ET =
            "<placa nombre=\"et\">\n"
            "  <componente tipo=\"Fuente\" id=\"F\"><pin nombre=\"pin\" nodo=\"PA0\"/></componente>\n"
            "</placa>\n";
        const char* CAT_ET =
            "<catalogo>\n"
            "  <pieza idx=\"0\" id=\"F\" tipo=\"Fuente\">\n"
            "    <observable idx=\"0\" id_obs=\"0\" nombre=\"corriente\" unidad=\"mA\" min=\"0\" max=\"100\" interesante=\"si\"/>\n"
            "  </pieza>\n"
            "</catalogo>\n";
        const char* DIBUJO_ET =
            "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 140 60\">\n"
            "  <rect x=\"0\" y=\"0\" width=\"140\" height=\"60\" fill=\"#1e5631\"/>\n"
            "  <rect id=\"F\" x=\"20\" y=\"10\" width=\"30\" height=\"20\" fill=\"#c0c0c0\"/>\n"
            "</svg>\n";
        const PlacaGui p = placa_de(PLACA_ET, CAT_ET);
        QString e;
        std::unique_ptr<VistaPlaca> v(VistaPlaca::crea(p, QString(), DIBUJO_ET, {}, e));
        v->resize(600, 400);
        v->show();
        espera([] { return false; }, 50);
        v->pon_valor(0, 12.5f);
        QGraphicsSimpleTextItem* t = v->etiqueta(0);
        const QRectF antes = t ? t->sceneBoundingRect() : QRectF();
        v->gira_placa(QString(), 90);
        const QRectF pieza = v->vivo(0)->sceneBoundingRect();
        const QRectF et = t ? t->sceneBoundingRect() : QRectF();
        const QTransform st = t ? t->sceneTransform() : QTransform();
        comprueba(t && pieza.height() > pieza.width() && std::abs(st.m12()) < 1e-9 &&
                      st.m11() > 0 && et.top() >= pieza.bottom() - 1e-6 &&
                      std::abs(et.center().x() - pieza.center().x()) < 1e-6 &&
                      std::abs(et.width() - antes.width()) < 1e-6,
                  "una placa suelta tambien se gira: la pieza queda de pie, y su etiqueta, "
                  "derecha, del mismo tamano y centrada bajo ella");
    }

    return resultado();
}
