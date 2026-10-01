// =============================================================================
// prueba_ventana.cpp — la pantalla construida a partir de los dos XML
//
// Fase 3 del plan: «un panel por pieza, un indicador por observable
// interesante, un control por mando. Sin conocer un tipo». Con widgets de
// verdad, pero con la plataforma `offscreen`: no abre nada en ninguna
// pantalla, así que corre en el CI.
//
//   G1  `construye_panel` sobre la placa de prueba: cuántos recuadros,
//       indicadores y controles, y lo que dice cada uno;
//   G2  la VentanaPrincipal entera contra un modelo falso: espera, se
//       construye al llegar la placa, «Arrancar» se habilita con T_LISTO y,
//       pulsado, el modelo recibe T_ARRANCA; T_FIN lo deshabilita todo.
//   G3  (fase 4) cómo se escribe un valor sin conocer un tipo, y la ventana en
//       marcha: se suscribe al recibir T_LISTO a lo que pinta, y pone en su
//       sitio las muestras, los relojes, las pérdidas y los avisos.
//   G4  (fase 5) los mandos: qué ordena cada tipo de control, que se activan
//       con T_LISTO y se apagan con T_FIN, que antes de arrancar y en marcha
//       cada toque sale como T_ORDENES, y que los ecos que no son RES_OK -salvo
//       el de rango, que ya avisa el modelo- van a la lista de avisos.
// =============================================================================
#include <cmath>

#include <QApplication>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QStatusBar>

#include "comun.h"
#include "panel.h"
#include "placa.h"
#include "ventana_principal.h"

using namespace mcusim;
using namespace mcusim::proto;
using namespace prueba;

namespace {

int cuantos_con_prefijo(QWidget* w, const QString& prefijo)
{
    int n = 0;
    for (QWidget* h : w->findChildren<QWidget*>())
        if (h->objectName().startsWith(prefijo)) ++n;
    return n;
}

} // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    // -------------------------------------------------------------------------
    std::printf("G1 El panel, a partir de los dos XML\n");
    {
        QVector<PiezaGui> cat;
        PlacaGui p;
        QString e;
        lee_catalogo(CATALOGO_XML, cat, e);
        junta_placa(PLACA_XML, cat, p, e);
        QWidget* w = construye_panel(p);

        const auto cajas = w->findChildren<QGroupBox*>();
        comprueba(cajas.size() == 4, "un recuadro por pieza: cuatro, la resistencia incluida");
        comprueba(cuantos_con_prefijo(w, "obs:") == p.n_interesantes() &&
                  p.n_interesantes() == 3,
                  "un indicador por observable interesante: tres");
        comprueba(w->findChild<QLabel*>("obs:2") == nullptr,
                  "la corriente del LED, que no se sugiere, no se pinta...");
        QLabel* ocultos = w->findChild<QLabel*>("ocultos:1");
        comprueba(ocultos && ocultos->text().contains("1"),
                  "...pero se dice que esta ahi");
        QLabel* enc = w->findChild<QLabel*>("obs:1");
        comprueba(enc && enc->text() == QString::fromUtf8("—"),
                  "los indicadores dicen '—' hasta que haya muestras (fase 4)");
        QPushButton* pulsar = w->findChild<QPushButton*>("mando:2:0");
        comprueba(pulsar && pulsar->text() == "pulsar" && !pulsar->isEnabled(),
                  "un mando de tipo boton es un boton, desactivado hasta que haya un "
                  "modelo esperando");
        comprueba(cuantos_con_prefijo(w, "mando:") == p.n_mandos(),
                  "un control por mando, ni uno mas");
        QGroupBox* x3 = w->findChild<QGroupBox*>("pieza:0");
        comprueba(x3 && x3->title().contains("X3") && x3->title().contains("Crystal") &&
                  x3->title().contains("desoldada"),
                  "el titulo es el id y el tipo, y dice si esta desoldada");
        QLabel* pd12 = w->findChild<QLabel*>("patilla:1:anodo");
        comprueba(pd12 && pd12->text() == "PD12", "y cada patilla dice a que nodo va");

        // Una pieza que esta ventana no ha visto nunca: sale igual
        PlacaGui raro;
        PiezaGui s;
        s.idx = 0; s.id = "S1"; s.tipo = "Servo";
        s.observables.push_back({0, 0, "angulo", "grados", 0, 180, true});
        s.mandos.push_back({0, "consigna", "continuo", 0, 180});
        s.mandos.push_back({1, "habilita", "interruptor", 0, 1});
        raro.piezas.push_back(s);
        QWidget* w2 = construye_panel(raro);
        comprueba(w2->findChild<QLabel*>("obs:0") &&
                  w2->findChild<QSlider*>("mando:0:0") &&
                  w2->findChild<QCheckBox*>("mando:0:1"),
                  "un tipo que esta ventana no conoce -un servo- sale igual: un "
                  "deslizador para lo continuo y una casilla para el interruptor");

        // Fase 5: que ordena cada control, sin conocer un tipo
        auto* panel = qobject_cast<Panel*>(w2);
        struct Ord { quint16 p, m; float v; };
        std::vector<Ord> ords;
        QObject::connect(panel, &Panel::orden,
                         [&](quint16 p, quint16 m, float v) { ords.push_back({p, m, v}); });
        auto* consigna = w2->findChild<QSlider*>("mando:0:0");
        auto* habilita = w2->findChild<QCheckBox*>("mando:0:1");
        comprueba(!panel->mandos_activos() && !consigna->isEnabled() && !habilita->isEnabled(),
                  "los controles nacen apagados");
        panel->activa_mandos(true);
        comprueba(panel->mandos_activos() && consigna->isEnabled() && habilita->isEnabled(),
                  "activa_mandos() los enciende todos");
        consigna->setValue(500);
        habilita->setChecked(true);
        habilita->setChecked(false);
        comprueba(ords.size() == 3 && ords[0].p == 0 && ords[0].m == 0 &&
                      std::fabs(ords[0].v - 90.f) < 1e-4f &&
                      ords[1].m == 1 && ords[1].v == 1.f && ords[2].m == 1 && ords[2].v == 0.f,
                  "el deslizador a la mitad ordena la mitad de su rango -90 grados- y la "
                  "casilla, su maximo marcada y su minimo desmarcada");
        delete w;
        delete w2;
    }

    {
        ObservableGui si{0, 0, "encendido", "", 0, 1, true};
        ObservableGui ma{1, 1, "corriente", "mA", 0, 25, false};
        ObservableGui gr{0, 2, "angulo", "grados", 0, 180, true};
        comprueba(Panel::texto_de(si, 1.f) == QString::fromUtf8("●") &&
                  Panel::texto_de(si, 0.f) == QString::fromUtf8("○") &&
                  Panel::texto_de(ma, 1.7734f) == "1.773 mA" &&
                  Panel::texto_de(gr, 90.f) == "90 grados",
                  "un valor se escribe segun su DECLARACION: de 0 a 1 sin unidad, ● u ○; "
                  "lo demas, el numero con su unidad");
    }

    // -------------------------------------------------------------------------
    std::printf("G2 La ventana entera, contra un modelo falso\n");
    {
        VentanaPrincipal v(0);              // puerto 0: el que dé el sistema
        v.show();
        const quint16 puerto = v.sesion().puerto();
        auto* arrancar = v.findChild<QPushButton*>("arrancar");
        auto* resumen  = v.findChild<QLabel*>("resumen");
        comprueba(puerto != 0 && arrancar && !arrancar->isEnabled() && resumen &&
                  resumen->text().contains("Esperando"),
                  "al abrirse escucha, dice que espera, y Arrancar esta desactivado");

        ModeloFalso m;
        m.conecta(puerto);
        comprueba(saluda_hasta_listo(m) &&
                  espera([&] { return arrancar->isEnabled(); }),
                  "tras el saludo y T_LISTO, Arrancar se habilita");
        comprueba(v.findChildren<QGroupBox*>().size() == 4 &&
                  resumen->text().contains("discovery") &&
                  resumen->text().contains("STM32F407VG") &&
                  resumen->text().contains("blinky.bin"),
                  "y la ventana tiene su panel: cuatro piezas, y arriba la placa, el MCU "
                  "y el firmware");
        // Fase 4: con T_LISTO, la ventana se suscribe a lo que pinta
        CabSuscribe cs{};
        comprueba(m.espera_leidos(2) && m.leido[1].tipo == T_SUSCRIBE &&
                  m.leido[1].cuerpo.size() == int(sizeof cs + 3 * 2) &&
                  (std::memcpy(&cs, m.leido[1].cuerpo.constData(), sizeof cs), true) &&
                  cs.periodo_ns_lo == VentanaPrincipal::PERIODO_NS && cs.n == 3 &&
                  m.leido[1].cuerpo.mid(int(sizeof cs)) ==
                      QByteArray::fromStdString(bytes(uint16_t(0)) + bytes(uint16_t(1)) +
                                                bytes(uint16_t(3))),
                  "con T_LISTO, y ANTES de arrancar, se suscribe a los tres observables "
                  "que pinta, a 60 Hz simulados");
        // Fase 5: con T_LISTO los mandos se activan, y lo que se toque antes de
        // arrancar sale ya: el modelo lo aplicara en t = 0
        auto* pulsar = v.findChild<QPushButton*>("mando:2:0");
        comprueba(pulsar && pulsar->isEnabled(),
                  "con T_LISTO los mandos se activan, antes de arrancar");
        pulsar->animateClick();
        comprueba(m.espera_leidos(4) && m.leido[2].tipo == T_ORDENES &&
                  m.leido[2].cuerpo == QByteArray::fromStdString(bytes(Orden{0, 2, 0, 1.f})) &&
                  m.leido[3].tipo == T_ORDENES &&
                  m.leido[3].cuerpo == QByteArray::fromStdString(bytes(Orden{0, 2, 0, 0.f})),
                  "hundir el boton manda pulsar = 1 y soltarlo pulsar = 0: un T_ORDENES "
                  "cada vez, con delta 0");
        auto* parar = v.findChild<QPushButton*>("parar");
        arrancar->click();
        comprueba(m.espera_leidos(5) && m.leido[4].tipo == T_ARRANCA && !arrancar->isEnabled() &&
                  !parar->isEnabled(),
                  "pulsar Arrancar manda T_ARRANCA; Arrancar se desactiva, y Parar tambien "
                  "(en marcha es la fase 6)");

        std::printf("G3 La ventana en marcha\n");
        auto* led = v.findChild<QLabel*>("obs:1");
        auto* btn = v.findChild<QLabel*>("obs:3");
        std::string inst = bytes(CabInstantanea{5000000ull, 2, 3});
        inst += bytes(Muestra{1, 0, 1.f}) + bytes(Muestra{3, 0, 0.f}) ;
        m.manda(T_INSTANTANEA, inst);
        comprueba(espera([&] { return led->text() == QString::fromUtf8("●"); }) &&
                  btn->text() == QString::fromUtf8("○"),
                  "una instantanea pone cada muestra en su indicador: el LED encendido, "
                  "el pulsador suelto");
        m.manda(T_ESTADO, bytes(Estado{F_CORRIENDO, 0, 5000000ull, 0.75, 1234ull}));
        auto* relojes = v.findChild<QLabel*>("relojes");
        comprueba(espera([&] { return relojes->text().contains("5.000 ms"); }) &&
                  relojes->text().contains("0.75 s") && relojes->text().contains("1234") &&
                  relojes->text().contains("3 instantaneas perdidas"),
                  "T_ESTADO pone los dos relojes y los deltas, y como la instantanea decia "
                  "3 perdidas, que va por detras: \"" + relojes->text().toStdString() + "\"");
        m.manda(T_AVISO, bytes(CabAviso{N_ERROR, 7, 6000000ull}) + "/stm32/" + "un error");
        auto* avisos = v.findChild<QListWidget*>("avisos");
        comprueba(espera([&] { return avisos->count() == 1; }) &&
                  avisos->item(0)->text().contains("error") &&
                  avisos->item(0)->text().contains("6.000 ms") &&
                  avisos->item(0)->text().contains("/stm32/: un error"),
                  "un T_AVISO va a la lista de avisos: nivel, instante, origen y texto");

        std::printf("G4 Los mandos en marcha, y los ecos\n");
        pulsar->animateClick();
        comprueba(pulsar->isEnabled() && m.espera_leidos(7) && m.leido[5].tipo == T_ORDENES &&
                  m.leido[6].tipo == T_ORDENES &&
                  m.leido[6].cuerpo == QByteArray::fromStdString(bytes(Orden{0, 2, 0, 0.f})),
                  "en marcha siguen activos, y cada toque sale igual");
        m.manda(T_ORDEN_HECHA, bytes(OrdenHecha{7000000ull, 2, 0, 1.f, RES_OK, 0}));
        m.manda(T_ORDEN_HECHA, bytes(OrdenHecha{7000000ull, 2, 0, 1.f, RES_RANGO, 0}));
        m.manda(T_ORDEN_HECHA, bytes(OrdenHecha{8000000ull, 9, 0, 1.f, RES_PIEZA, 0}));
        m.manda(T_ORDEN_HECHA, bytes(OrdenHecha{9000000ull, 1, 0, 1.f, RES_MANDO, 0}));
        m.manda(T_ORDEN_HECHA, bytes(OrdenHecha{9500000ull, 2, 0, 0.f, RES_TARDE, 0}));
        comprueba(espera([&] { return v.sesion().ecos() == 5 && avisos->count() == 4; }),
                  "de cinco ecos, tres van a la lista de avisos: RES_OK se ve en los "
                  "indicadores, y RES_RANGO ya lo avisa el modelo");
        if (avisos->count() == 4) {
            comprueba(avisos->item(1)->text().contains("pieza 9") &&
                          avisos->item(1)->text().contains("no existe esa pieza") &&
                          avisos->item(1)->text().contains("8.000 ms"),
                      "una pieza que no existe, con su instante: \"" +
                          avisos->item(1)->text().toStdString() + "\"");
            comprueba(avisos->item(2)->text().contains("LD4, mando 0") &&
                          avisos->item(2)->text().contains("no tiene ese mando"),
                      "un mando que no existe, nombrando la pieza por su id: \"" +
                          avisos->item(2)->text().toStdString() + "\"");
            comprueba(avisos->item(3)->text().startsWith("[info]") &&
                          avisos->item(3)->text().contains("B1.pulsar") &&
                          avisos->item(3)->text().contains("tarde"),
                      "y una que llego tarde, como informacion: \"" +
                          avisos->item(3)->text().toStdString() + "\"");
        }
        m.manda(T_FIN, bytes(Fin{M_VENTANA, 0, 50110000ull}));
        comprueba(espera([&] { return v.statusBar()->currentMessage().contains("50.110"); }),
                  "T_FIN se ve en la barra de estado, con el instante: \"" +
                  v.statusBar()->currentMessage().toStdString() + "\"");
        comprueba(!pulsar->isEnabled(), "y con T_FIN los mandos se apagan");
    }

    return resultado();
}
