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
// =============================================================================
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
                  "un mando de tipo boton es un boton, desactivado hasta la fase 5");
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
        auto* parar = v.findChild<QPushButton*>("parar");
        arrancar->click();
        comprueba(m.espera_leidos(3) && m.leido[2].tipo == T_ARRANCA && !arrancar->isEnabled() &&
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
        m.manda(T_FIN, bytes(Fin{M_VENTANA, 0, 50110000ull}));
        comprueba(espera([&] { return v.statusBar()->currentMessage().contains("50.110"); }),
                  "T_FIN se ve en la barra de estado, con el instante: \"" +
                  v.statusBar()->currentMessage().toStdString() + "\"");
    }

    return resultado();
}
