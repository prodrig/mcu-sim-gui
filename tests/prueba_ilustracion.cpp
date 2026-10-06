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
// =============================================================================
#include <QApplication>
#include <QGraphicsSvgItem>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QAction>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>

#include "comun.h"
#include "dibujo.h"
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
    std::printf("I4 La ventana: la pestana Ilustracion, junto al panel\n");
    {
        VentanaPrincipal v(0);
        v.show();
        auto* vistas = v.findChild<QTabWidget*>("vistas");
        auto* avisos = v.findChild<QListWidget*>("avisos");
        auto* accion = v.findChild<QAction*>("abrir_dibujo");
        comprueba(vistas && vistas->count() == 1 && vistas->tabText(0) == "Panel" &&
                      accion && !accion->isEnabled(),
                  "sin placa, solo el panel, y nada que dibujar");
        QString e;
        comprueba(!v.abre_dibujo(QString(), DIBUJO, &e) && e.contains("no hay placa"),
                  "ni un dibujo se puede abrir sin placa");

        ModeloFalso m;
        m.conecta(v.sesion().puerto());
        saluda_hasta_listo(m);
        comprueba(espera([&] { return vistas->count() == 2; }) &&
                      vistas->tabText(0) == "Ilustracion" && vistas->tabText(1) == "Panel" &&
                      vistas->currentIndex() == 1 && accion->isEnabled(),
                  "con la placa, las dos pestanas; sin dibujo se ve el panel");
        auto* ilus = v.findChild<VistaIlustracion*>("ilustracion");
        comprueba(ilus && ilus->findChild<QWidget*>("dibujo:") &&
                      ilus->findChild<QLabel*>("hueco:") &&
                      ilus->findChild<QPushButton*>("abrir:") && !ilus->hay_dibujo(),
                  "la ilustracion: un recuadro, con un hueco y su boton para abrir el dibujo");

        comprueba(v.abre_dibujo(QString(), DIBUJO, &e), "se abre el dibujo a mano");
        auto* vp = ilus->findChild<VistaPlaca*>("vista:");
        auto* inf = ilus->findChild<QLabel*>("informe:");
        comprueba(vp && !ilus->findChild<QLabel*>("hueco:") &&
                      vistas->currentWidget() == ilus,
                  "y se ve: el hueco deja sitio al dibujo, y la pestana pasa a la ilustracion");
        comprueba(inf && inf->isVisible() && inf->text().startsWith("2 de 4 piezas") &&
                      inf->toolTip().contains("serigrafia"),
                  "el recuadro dice lo que ha encontrado, y el detalle en la ayuda");
        comprueba(hay_aviso(avisos, "2 de 4 piezas") && hay_aviso(avisos, "/etc/passwd") &&
                      hay_aviso(avisos, "sin elemento"),
                  "y la lista de avisos tambien, linea a linea");
        comprueba(v.findChildren<QGroupBox*>().size() == 4 &&
                      v.findChild<QWidget*>("panel") && vistas->indexOf(
                          v.findChild<QScrollArea*>()) >= 0,
                  "el panel sigue entero en su pestana: la ilustracion no lo sustituye");

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
        auto* vistas = v.findChild<QTabWidget*>("vistas");
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
        comprueba(v.abre_dibujo("N", DIBUJO_NUCLEO, &e) && ilus->vista("N") &&
                      !ilus->vista("S") && ilus->findChild<QLabel*>("hueco:S"),
                  "el dibujo de N va a N; S sigue sin dibujo");
        comprueba(ilus->vista("N")->informe().resumen() == "2 de 2 piezas en el dibujo",
                  "y en N se encuentran sus dos piezas: \"" +
                      ilus->vista("N")->informe().resumen().toStdString() + "\"");
        m.s.disconnectFromHost();
        comprueba(espera([&] { return v.sesion().estado() == Sesion::Estado::Terminada; }),
                  "el modelo se va");

        ModeloFalso m2;
        saluda(m2);
        comprueba(espera([&] {
                      auto* i = v.findChild<VistaIlustracion*>("ilustracion");
                      return i && i != ilus && i->vista("N");
                  }) && vistas->currentWidget() == v.findChild<VistaIlustracion*>("ilustracion"),
                  "otro modelo con la misma Nucleo: su dibujo sale solo, que la ventana lo "
                  "recuerda por el nombre de la placa, y se ve la ilustracion");
    }

    return resultado();
}
