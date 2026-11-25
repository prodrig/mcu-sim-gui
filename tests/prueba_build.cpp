// =============================================================================
// prueba_build.cpp — La ventana de mcu-sim-build, sin pantalla (G9 y G10 de
// doc/analisis_build.md)
//
// Una placa nueva de principio a fin, como la haría el usuario: la
// biblioteca importada de MCU_SIM_SRC, un MCU y un LED colocados, unidos
// con Ctrl+clic (de verdad, con eventos de ratón) y con las acciones,
// movidos, girados, deshacer y rehacer, el marco, ocultar, curva y tramos,
// y exportarla -validada por mcu-sim si hay MCU_SIM- y volverla a abrir.
// =============================================================================
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QListWidget>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QUndoStack>

#include "biblioteca.h"
#include "comun.h"
#include "lienzo.h"
#include "panel_biblioteca.h"
#include "ventana_build.h"

using namespace mcusim::build;
using prueba::comprueba;

namespace {

QGraphicsItem* item_con(Lienzo* l, int rol, const QVariant& v)
{
    for (QGraphicsItem* it : l->scene()->items())
        if (it->data(rol) == v) return it;
    return nullptr;
}

// Un clic de verdad, con Ctrl, en el terminal `ref`
bool ctrl_clic(Lienzo* l, const QString& ref)
{
    QGraphicsItem* t = item_con(l, 1, ref);
    if (!t) return false;
    l->centerOn(t);
    const QPoint p = l->mapFromScene(t->sceneBoundingRect().center());
    QMouseEvent pulsa(QEvent::MouseButtonPress, p, l->viewport()->mapToGlobal(p), Qt::LeftButton,
                      Qt::LeftButton, Qt::ControlModifier);
    QApplication::sendEvent(l->viewport(), &pulsa);
    QMouseEvent suelta(QEvent::MouseButtonRelease, p, l->viewport()->mapToGlobal(p), Qt::LeftButton,
                       Qt::NoButton, Qt::ControlModifier);
    QApplication::sendEvent(l->viewport(), &suelta);
    return true;
}

const Entrada* entrada(VentanaBuild& v, Entrada::Clase c, const QString& n)
{
    return v.biblioteca()->busca(c, n);
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    const QString src = qEnvironmentVariable("MCU_SIM_SRC");
    if (src.isEmpty() || !QFileInfo(src + "/biblioteca/piezas/Led.pieza.xml").exists()) {
        std::printf("  [SALTA] sin MCU_SIM_SRC no hay biblioteca\n");
        return 77;
    }
    QTemporaryDir tmp;
    VentanaBuild v(tmp.path() + "/datos", tmp.path() + "/config.json");
    v.resize(1200, 800);
    v.show();
    Lienzo* l = v.lienzo();

    std::printf("== la biblioteca (G9)\n");
    comprueba(v.biblioteca()->carpetas().size() == 2, "sin carpetas, importa la de mcu-sim: biblioteca y placas");
    comprueba(v.panel_biblioteca()->cuantas_piezas() > 100, "más de cien piezas en la pestaña Piezas (" +
                                                                std::to_string(v.panel_biblioteca()->cuantas_piezas()) + ")");
    comprueba(v.panel_biblioteca()->cuantas_tarjetas() > 20, "y las placas en Tarjetas");
    for (int i = 0; i < 20; ++i) QApplication::processEvents();
    QListWidgetItem* primera = v.panel_biblioteca()->lista_piezas()->item(0);
    comprueba(primera && !primera->icon().isNull() && !primera->toolTip().isEmpty(),
              "con su miniatura y su resumen");

    std::printf("== colocar\n");
    const Entrada* mcu = entrada(v, Entrada::Mcu, "STM32F446RE");
    const Entrada* led = entrada(v, Entrada::Pieza, "Led");
    comprueba(l->coloca(*mcu, {40, 40}, "u0") == "u0", "el MCU, u0");
    comprueba(l->coloca(*led, {80, 30}, "LD1") == "LD1", "un LED, LD1");
    comprueba(l->coloca(*led, {80, 50}, "P1").isEmpty(), "con id P1, no: P1.1 sería PB1");
    comprueba(l->coloca(*led, {80, 50}, "LD1").isEmpty(), "ni repetido");
    comprueba(v.documento().instancias.size() == 2 && v.deshacer()->count() == 2, "dos piezas, dos pasos de deshacer");
    comprueba(prefijo_id(*v.documento().instancia("LD1")) == "LD" &&
                  prefijo_id(*v.documento().instancia("u0")) == "u",
              "los prefijos: LD y u");

    std::printf("== unir\n");
    comprueba(ctrl_clic(l, "u0.21") && ctrl_clic(l, "LD1.anodo"), "Ctrl+clic en u0.21 y en LD1.anodo");
    comprueba(l->terminales_elegidos() == QStringList({"u0.21", "LD1.anodo"}), "elegidos, en orden");
    comprueba(l->une(false) && v.documento().conexiones.size() == 1, "unidos con una curva");
    comprueba(l->terminales_elegidos().isEmpty(), "y se olvidan");
    l->elige_terminal("LD1.catodo");
    l->elige_terminal("u0.31");
    l->elige_terminal("u0.47");
    comprueba(l->une(true) && v.documento().conexiones.size() == 3, "tres terminales: dos conexiones en estrella");
    const Conexion c1 = v.documento().conexiones[1];
    comprueba(c1.a == "LD1.catodo" && c1.b == "u0.31" && c1.tramos && c1.codos.size() == 1,
              "desde el primero, en tramos y con un codo");
    comprueba(l->redes().nodo_de.value("LD1.anodo") == "PA5", "LD1.anodo va a PA5");
    comprueba(l->redes().errores.isEmpty(), "sin errores (" + l->redes().errores.join("; ").toStdString() + ")");
    QGraphicsItem* t21 = item_con(l, 1, "u0.21");
    comprueba(t21 && t21->toolTip().contains("PA5") && t21->toolTip().contains("TIM2_CH1"),
              "el terminal dice su patilla y sus funciones");

    std::printf("== deshacer\n");
    v.deshacer()->undo();
    comprueba(v.documento().conexiones.size() == 1, "deshacer: una conexión");
    v.deshacer()->redo();
    comprueba(v.documento().conexiones.size() == 3, "rehacer: tres");

    std::printf("== mover, girar, el marco\n");
    QGraphicsItem* pl = item_con(l, 0, "LD1");
    pl->setPos(90.04, 32);
    l->suelta_piezas();
    comprueba(v.documento().instancia("LD1")->pos == QPointF(90, 32), "mover: a 0,1 mm");
    l->elige_instancia("LD1");
    comprueba(l->gira() && v.documento().instancia("LD1")->giro == 90, "girar 90°");
    comprueba(l->marco_a_mano(true) && v.documento().marco.isValid(), "el marco a mano");
    v.deshacer()->undo();
    comprueba(!v.documento().marco.isValid(), "y otra vez el automático");

    std::printf("== una conexión\n");
    l->scene()->clearSelection();
    item_con(l, 2, 0)->setSelected(true);
    comprueba(l->conexion_elegida() == 0, "elegida la primera");
    comprueba(l->alterna_trazado() && v.documento().conexiones[0].tramos, "curva → tramos");
    item_con(l, 2, 0)->setSelected(true);
    comprueba(l->alterna_visible() && !v.documento().conexiones[0].visible, "oculta");
    comprueba(l->redes().nodo_de.value("LD1.anodo") == "PA5", "y une igual");

    std::printf("== exportar y reabrir\n");
    const QString ruta = tmp.path() + "/mi_placa.xml";
    if (!v.config().mcu_sim.isEmpty()) std::printf("  (validando con %s)\n", qPrintable(v.config().mcu_sim));
    comprueba(v.exporta_a(ruta), "se exporta");
    comprueba(QFileInfo::exists(ruta) && QFileInfo::exists(tmp.path() + "/mi_placa.svg"), "el XML y el SVG");
    comprueba(v.deshacer()->isClean() && v.ruta() == ruta, "sin cambios pendientes");
    if (!qEnvironmentVariableIsEmpty("PRUEBA_CAPTURA")) {     // para mirarla
        l->ajusta_vista();
        l->elige_terminal("u0.5");
        l->elige_terminal("LD1.catodo");
        for (int i = 0; i < 50; ++i) QApplication::processEvents();
        v.grab().save(qEnvironmentVariable("PRUEBA_CAPTURA"));
        l->olvida_terminales();
    }
    const Documento antes = v.documento();
    v.nuevo();
    comprueba(v.documento().instancias.isEmpty(), "nueva: vacía");
    QString e;
    comprueba(v.abre(ruta, &e), "se reabre " + e.toStdString());
    const Documento d = v.documento();
    comprueba(d.instancias.size() == 2 && d.instancia("LD1")->giro == 90 &&
                  d.instancia("LD1")->pos == QPointF(90, 32),
              "con las piezas donde estaban");
    comprueba(d.conexiones == antes.conexiones, "y las mismas conexiones");

    std::printf("== una placa del repositorio, sin <build>\n");
    comprueba(v.abre(src + "/placas/servo_sg90.xml", &e), "servo_sg90 se reconstruye " + e.toStdString());
    comprueba(!v.deshacer()->isClean() && v.ruta().isEmpty(), "y queda por exportar");

    std::printf("== borrar\n");
    v.abre(ruta, &e);
    l->elige_instancia("LD1");
    comprueba(l->borra() && v.documento().instancias.size() == 1 && v.documento().conexiones.isEmpty(),
              "borrar LD1 se lleva sus conexiones");
    return prueba::resultado();
}
