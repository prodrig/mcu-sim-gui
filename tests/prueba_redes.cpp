// =============================================================================
// prueba_redes.cpp — De las conexiones a los nodos (G10 de
// doc/analisis_build.md, §3.1), con la biblioteca de mcu-sim (MCU_SIM_SRC)
// =============================================================================
#include <QCoreApplication>
#include <QFileInfo>
#include <QTemporaryDir>

#include "biblioteca.h"
#include "catalogo.h"
#include "comun.h"
#include "exporta_placa.h"
#include "redes.h"

using namespace mcusim::build;
using prueba::comprueba;

namespace {
Instancia pieza(const Catalogo& c, const QString& tipo, const QString& id)
{
    Instancia i = c.nueva(*c.biblioteca()->busca(Entrada::Pieza, tipo));
    i.id = id;
    return i;
}
Conexion con(const QString& a, const QString& b)
{
    Conexion x;
    x.a = a;
    x.b = b;
    return x;
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QString src = qEnvironmentVariable("MCU_SIM_SRC");
    if (src.isEmpty() || !QFileInfo(src + "/biblioteca/piezas/Led.pieza.xml").exists()) {
        std::printf("  [SALTA] sin MCU_SIM_SRC\n");
        return 77;
    }
    QTemporaryDir tmp;
    Biblioteca bi(tmp.path());
    bi.anade_carpeta(src + "/biblioteca");
    bi.abre();
    Catalogo c(&bi);

    std::printf("== los ids\n");
    Documento d;
    comprueba(motivo_id_invalido(d, "LD2").isEmpty(), "LD2 vale");
    comprueba(!motivo_id_invalido(d, "P1").isEmpty(), "P1 no: P1.1 sería PB1");
    comprueba(!motivo_id_invalido(d, "PA").isEmpty(), "PA tampoco");
    comprueba(!motivo_id_invalido(d, "A/B").isEmpty() && !motivo_id_invalido(d, "A.B").isEmpty(),
              "ni con / ni con .");
    comprueba(!motivo_id_invalido(d, "2R").isEmpty(), "ni empezando por cifra");

    std::printf("== un pad, una masa, un nodo externo y un conector\n");
    Instancia u = c.nueva(*bi.busca(Entrada::Mcu, "STM32F446RE"));
    u.id = "u0";
    d.instancias << u << pieza(c, "Led", "LD2") << pieza(c, "Led", "LD3") << pieza(c, "Resistencia", "R1");
    Instancia cn = c.nueva(*bi.busca(Entrada::Pieza, "Conector"));
    cn.id = "CN9";
    cn.atributos["columnas"] = "8";
    d.instancias << cn;
    d.conexiones << con("LD2.anodo", "u0.21")           // PA5
                 << con("LD2.catodo", "u0.31")          // VSS
                 << con("R1.a", "LD3.anodo") << con("LD3.catodo", "CN9.8")
                 << con("R1.b", "CN9.3") << con("CN9.4", "u0.22");   // PA6
    const Redes r = calcula_redes(d, c);
    comprueba(r.nodo_de.value("LD2.anodo") == "PA5", "LD2.anodo va al pad: PA5");
    comprueba(r.nodo_de.value("LD2.catodo") == c.nodo_mcu(u, "31"),
              "LD2.catodo, a la masa del chip: " + c.nodo_mcu(u, "31").toStdString());
    const QString ext = r.nodo_de.value("R1.a");
    comprueba(ext.startsWith("n_") && r.nodo_de.value("LD3.anodo") == ext, "R1.a y LD3.anodo: un nodo externo");
    bool declarado = false;
    for (const NodoDeclarado& nd : r.nodos) declarado = declarado || (nd.id == ext && nd.externo);
    comprueba(declarado, "que se declara externo=\"si\"");
    comprueba(r.nodo_de.value("LD3.catodo") == "CN9.8" && !r.nodo_de.contains("CN9.8"),
              "LD3.catodo va al pin CN9.8, y el conector no escribe ese pin");
    comprueba(r.nodo_de.value("CN9.4") == "PA6", "CN9.4 va a PA6");
    comprueba(r.errores.isEmpty(), "sin errores");

    std::printf("== un puente, un corto, y lo que falta\n");
    Documento p = d;
    p.conexiones << con("u0.21", "u0.22");               // PA5 con PA6
    Redes rp = calcula_redes(p, c);
    bool puente = false;
    for (const NodoDeclarado& nd : rp.nodos) puente = puente || nd.une.size() == 2;
    comprueba(puente && !rp.avisos.filter("puente").isEmpty(), "dos pads: <nodo une> y un aviso de puente");
    Documento q = d;
    q.conexiones << con("u0.21", "u0.31");               // PA5 con VSS
    comprueba(!calcula_redes(q, c).errores.isEmpty(), "un pad con la masa: error");
    Documento s;
    s.instancias << pieza(c, "Servo", "SV");
    s.conexiones << con("SV.pwm", "SV.gnd");
    const Redes rs = calcula_redes(s, c);
    comprueba(rs.sin_conectar.contains("SV.vcc"), "un Servo sin vcc: se marca");

    std::printf("== bus\n");
    Documento b = d;
    b.bus << "R1.a";
    bool es_bus = false;
    for (const NodoDeclarado& nd : calcula_redes(b, c).nodos) es_bus = es_bus || (nd.id == ext && nd.bus);
    comprueba(es_bus, "una red marcada como bus: bus=\"si\"");

    std::printf("== el XML\n");
    const QString x = xml_placa(d, c, r);
    comprueba(x.contains("<mcu tipo=\"STM32F446RE\" id=\"u0\"/>"), "el MCU");
    comprueba(x.contains("<pin nombre=\"anodo\" nodo=\"PA5\"/>"), "el pin del LED al pad");
    comprueba(x.contains("<build version=\"1\"") && x.contains("<instancia id=\"LD2\" fuente=\"piezas/Led\""),
              "el <build> con sus instancias");
    comprueba(x.count("<conexion ") == d.conexiones.size(), "y sus conexiones");
    const QString sv = svg_placa(d, c);
    comprueba(sv.contains("<g id=\"LD2\"") && sv.contains("id=\"LD2.anodo\"") && !sv.contains("id=\"$"),
              "el SVG: cada pieza con su id, sin «$»");
    comprueba(sv.contains("id=\"CN9.8\""), "y los pines del conector, CN9.8");
    return prueba::resultado();
}
