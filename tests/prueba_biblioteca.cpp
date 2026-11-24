// =============================================================================
// prueba_biblioteca.cpp — La biblioteca persistente de mcu-sim-build (G9 de
// doc/analisis_build.md, D6)
//
// Sobre una copia de unos ficheros de la biblioteca de mcu-sim (MCU_SIM_SRC):
// importar, que el índice se guarde y se reutilice sin releer, que un fichero
// cambiado se relea y solo él, que uno borrado se marque y que el refresco
// completo lo quite; y las miniaturas, de la caché la segunda vez.
// =============================================================================
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QThread>

#include "biblioteca.h"
#include "catalogo.h"
#include "comun.h"

using namespace mcusim::build;
using prueba::comprueba;

namespace {
bool copia(const QString& de, const QString& a)
{
    QDir().mkpath(QFileInfo(a).path());
    QFile::remove(a);
    return QFile::copy(de, a);
}
} // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    const QString src = qEnvironmentVariable("MCU_SIM_SRC");
    if (src.isEmpty() || !QFileInfo(src + "/biblioteca/piezas/Led.pieza.xml").exists()) {
        std::printf("  [SALTA] sin MCU_SIM_SRC no hay biblioteca de mcu-sim que importar\n");
        return 77;
    }
    QTemporaryDir tmp;
    const QString lib = tmp.path() + "/lib", datos = tmp.path() + "/datos";
    const QString b = src + "/biblioteca/";
    for (const char* f : {"piezas/Led.pieza.xml", "piezas/Led.forma.xml", "piezas/Led.svg",
                          "piezas/Resistencia.pieza.xml", "piezas/Resistencia.forma.xml",
                          "piezas/Resistencia.svg", "piezas/Conector.pieza.xml",
                          "piezas/Conector.forma.xml", "mcus/STM32F446RE.mcu.xml",
                          "mcus/STM32F446RE.simple.svg", "mcus/STM32F446RE.detalle.svg",
                          "conectores/arduino_uno_r3.digital_alto.conector.xml",
                          "conectores/arduino_uno_r3.digital_alto.svg"})
        copia(b + f, lib + "/" + f);
    copia(src + "/placas/servo_sg90.xml", lib + "/placas/servo_sg90.xml");
    copia(src + "/placas/servo_sg90.svg", lib + "/placas/servo_sg90.svg");

    std::printf("== importar\n");
    {
        Biblioteca bi(datos);
        bi.anade_carpeta(lib);
        const int n = bi.abre();
        comprueba(n == 6, "importa 6 ficheros: 3 fichas, un MCU, un perfil y una placa (" +
                              std::to_string(n) + ")");
        comprueba(bi.piezas().size() == 5 && bi.tarjetas().size() == 1, "5 piezas y 1 tarjeta");
        const Entrada* led = bi.busca(Entrada::Pieza, "Led");
        comprueba(led && led->fuente == "piezas/Led", "el Led, con su fuente piezas/Led");
        comprueba(led && led->resumen.startsWith("Diodo"), "con el resumen de su ficha");
        const Entrada* m = bi.busca(Entrada::Mcu, "STM32F446RE");
        comprueba(m && m->resumen.contains("LQFP64"), "el MCU, con su encapsulado en el resumen");
        comprueba(QFileInfo::exists(bi.ruta_indice()), "el índice se guarda");
        // El catálogo: ficha, dibujo y terminales
        Catalogo c(&bi);
        Instancia i = c.nueva(*led);
        i.id = "LD1";
        const Dibujo d = c.dibujo(i);
        comprueba(d.valido() && d.ancla("anodo") && d.ancla("catodo"), "el dibujo del Led y sus dos anclas");
        Instancia u = c.nueva(*m);
        comprueba(u.clase == Instancia::Mcu && c.terminales(u).size() == 64, "el MCU: 64 terminales");
        const Dibujo du = c.dibujo(u);
        comprueba(du.anclas.size() == 64, "y 64 anclas en su dibujo simple (" +
                                              std::to_string(du.anclas.size()) + ")");
        u.detalle = true;
        comprueba(c.dibujo(u).anclas.size() == 64, "y en el detallado");
        comprueba(c.nodo_mcu(u, "21") == "PA5" && c.posicion_mcu(u, "PA5") == "21",
                  "la patilla 21 es PA5, y al revés");
        const Entrada* pf = bi.busca(Entrada::Perfil, "arduino_uno_r3.digital_alto");
        Instancia cn = c.nueva(*pf);
        comprueba(cn.tipo == "Conector" && cn.atributos.value("columnas") == "10" &&
                      c.terminales(cn).size() == 10 && c.dibujo(cn).anclas.size() == 10,
                  "un perfil es un Conector de 1x10, dibujado con su generador");
        comprueba(c.rotulo(cn, "6") == "6 D13", "y su pin 6 se rotula con su uso: D13");
        // Las miniaturas
        const QImage a = bi.miniatura(*led, 64);
        comprueba(!a.isNull() && std::max(a.width(), a.height()) == 64, "la miniatura del Led");
        const int png = QDir(datos + "/miniaturas").entryList({"*.png"}).size();
        bi.miniatura(*led, 64);
        comprueba(png == 1 && QDir(datos + "/miniaturas").entryList({"*.png"}).size() == 1,
                  "y la segunda vez sale de la caché");
    }

    std::printf("== reabrir: del índice, sin releer\n");
    {
        Biblioteca bi(datos);
        const int n = bi.abre();
        comprueba(bi.carpetas().size() == 1, "las carpetas vienen del índice");
        comprueba(n == 0 && bi.entradas().size() == 6, "ningún fichero leído, 6 entradas (" +
                                                           std::to_string(n) + ")");
        // Un fichero cambia: solo ese se relee
        QThread::msleep(20);
        QFile f(lib + "/piezas/Led.pieza.xml");
        f.open(QIODevice::Append);
        f.write("\n");
        f.close();
        comprueba(bi.refresca() == 1, "cambia un fichero: se relee solo ese");
        // Uno desaparece: se marca
        QFile::remove(lib + "/piezas/Resistencia.pieza.xml");
        bi.refresca();
        const Entrada* r = nullptr;
        for (const Entrada& e : bi.entradas())
            if (e.nombre == "Resistencia") r = &e;
        comprueba(r && r->desaparecida, "uno borrado queda marcado");
        comprueba(!bi.busca(Entrada::Pieza, "Resistencia"), "y no se ofrece");
        bi.refresca(true);
        bool sigue = false;
        for (const Entrada& e : bi.entradas()) sigue = sigue || e.nombre == "Resistencia";
        comprueba(!sigue && bi.leidos() == 5, "el refresco completo lo quita, y lo relee todo");
    }
    {
        Biblioteca bi(datos);
        bi.abre();
        bi.quita_carpeta(lib);
        bi.refresca(true);
        comprueba(bi.entradas().isEmpty(), "quitar la carpeta y refrescar: vacía");
    }
    return prueba::resultado();
}
