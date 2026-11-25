// =============================================================================
// prueba_exporta.cpp — La prueba de ida y vuelta de G10
// (doc/analisis_build.md, §9)
//
// Cada placa del repositorio (MCU_SIM_SRC/placas) que mcu-sim-build sabe
// dibujar se RECONSTRUYE con piezas de la biblioteca -cada red, una estrella
// de conexiones-, se exporta, y el XML que sale tiene que dar EL MISMO
// NETLIST que el original: las mismas redes con los mismos terminales, se
// llamen como se llamen sus nodos. Con MCU_SIM, además, mcu-sim tiene que
// validar lo exportado. Las placas que llevan algo que todavía no se dibuja
// (<ref>, <transmision>, <ilustracion>, varios MCU...) se dicen y se saltan;
// unas cuantas tienen que salir siempre: servo_sg90, l298n, pololu_a4988 y
// la NUCLEO-F446RE, con su MCU y sus seis conectores.
// =============================================================================
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QXmlStreamReader>

#include "biblioteca.h"
#include "catalogo.h"
#include "comun.h"
#include "exporta_placa.h"
#include "abre.h"
#include "importa.h"
#include "redes.h"
#include "valida.h"

using namespace mcusim::build;
using prueba::comprueba;

namespace {
QString lee(const QString& r)
{
    QFile f(r);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}
bool xml_bien_formado(const QString& t)
{
    QXmlStreamReader x(t);
    while (!x.atEnd()) x.readNext();
    return !x.hasError();
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QString src = qEnvironmentVariable("MCU_SIM_SRC");
    const QString sim = qEnvironmentVariable("MCU_SIM");
    if (src.isEmpty() || !QFileInfo(src + "/biblioteca/piezas/Led.pieza.xml").exists()) {
        std::printf("  [SALTA] sin MCU_SIM_SRC\n");
        return 77;
    }
    QTemporaryDir tmp;
    tmp.setAutoRemove(qEnvironmentVariableIsEmpty("PRUEBA_CONSERVA"));   // para mirar lo exportado
    if (!tmp.autoRemove()) std::printf("  (lo exportado queda en %s)\n", qPrintable(tmp.path()));
    Biblioteca bi(tmp.path() + "/datos");
    bi.anade_carpeta(src + "/biblioteca");
    bi.abre();
    Catalogo c(&bi);
    if (sim.isEmpty()) std::printf("  (sin MCU_SIM: no se valida con mcu-sim)\n");

    QStringList hechas, saltadas;
    const QStringList obligatorias = {"servo_sg90", "l298n", "pololu_a4988", "nucleo_f446re"};
    const QStringList placas = QDir(src + "/placas").entryList({"*.xml"}, QDir::Files, QDir::Name);
    for (const QString& fn : placas) {
        const QString nombre = QFileInfo(fn).completeBaseName();
        const QString texto = lee(src + "/placas/" + fn);
        QString error;
        const PlacaLeida pl = lee_placa(texto, &error);
        if (!error.isEmpty() || pl.componentes.isEmpty() && pl.mcus.isEmpty()) continue;  // un sistema
        QString motivo;
        Documento d = reconstruye(pl, c, &motivo);
        if (d.instancias.isEmpty()) {
            saltadas << QStringLiteral("%1 (%2)").arg(nombre, motivo);
            comprueba(!obligatorias.contains(nombre), nombre.toStdString() + " se reconstruye");
            continue;
        }
        d.nombre = nombre;
        const QString ruta = tmp.path() + "/" + nombre + ".xml";
        const QString e = exporta(d, c, ruta);
        if (!comprueba(e.isEmpty(), nombre.toStdString() + ": se exporta")) continue;
        const QString nuevo = lee(ruta);
        const QString svg = lee(tmp.path() + "/" + nombre + ".svg");
        const PlacaLeida pn = lee_placa(nuevo, &error);
        const Particion a = particion(pl, c), b = particion(pn, c);
        if (!comprueba(a == b, nombre.toStdString() + ": el mismo netlist (" +
                                   std::to_string(a.size()) + " redes)")) {
            for (const QStringList& r : a)
                if (!b.contains(r)) std::printf("       solo en el original: %s\n", qPrintable(r.join(' ')));
            for (const QStringList& r : b)
                if (!a.contains(r)) std::printf("       solo en el exportado: %s\n", qPrintable(r.join(' ')));
        }
        bool mismos = pn.componentes.size() == pl.componentes.size();
        for (int i = 0; mismos && i < pl.componentes.size(); ++i)
            mismos = pl.componentes[i].tipo == pn.componentes[i].tipo &&
                     pl.componentes[i].id == pn.componentes[i].id;
        comprueba(mismos, nombre.toStdString() + ": las mismas piezas");
        comprueba(xml_bien_formado(svg) && svg.count("<g id=\"") >= d.instancias.size() &&
                      !svg.contains("id=\"$"),
                  nombre.toStdString() + ": el SVG, bien formado y con cada pieza");
        comprueba(nuevo.contains("<build version=\"1\"") &&
                      nuevo.count("<instancia ") == d.instancias.size(),
                  nombre.toStdString() + ": con su <build>");
        // Y se reabre: lo mismo -el mismo XML al exportarla otra vez-
        QString ea;
        const Documento r = abre_build(nuevo, c, &ea);
        const QString otra = xml_placa(r, c, calcula_redes(r, c));
        if (!comprueba(!r.instancias.isEmpty() && otra == nuevo,
                       nombre.toStdString() + ": se reabre y sale igual " + ea.toStdString())) {
            const QStringList l1 = nuevo.split('\n'), l2 = otra.split('\n');
            for (int k = 0; k < std::max(l1.size(), l2.size()); ++k)
                if (l1.value(k) != l2.value(k)) {
                    std::printf("       %s\n    -> %s\n", qPrintable(l1.value(k)), qPrintable(l2.value(k)));
                    break;
                }
        }
        if (!sim.isEmpty()) {
            const Validacion v = valida_fichero(sim, ruta);
            if (!comprueba(v.valida, nombre.toStdString() + ": mcu-sim la valida"))
                std::printf("%s\n", qPrintable(v.texto()));
        }
        hechas << nombre;
    }
    for (const QString& o : obligatorias)
        comprueba(hechas.contains(o), o.toStdString() + " está entre las hechas");
    std::printf("\n  %d placas de ida y vuelta: %s\n", int(hechas.size()), qPrintable(hechas.join(", ")));
    std::printf("  %d todavía no:\n", int(saltadas.size()));
    for (const QString& s : saltadas) std::printf("    %s\n", qPrintable(s));
    return prueba::resultado();
}
