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
//   G5  (fase 6) el control: el ritmo elegido antes de arrancar, «Pausa» /
//       «Sigue» según diga el modelo, «Paso» a demanda y «Parar» en marcha.
//   G6  una placa sin MCU, con una Fuente: el resumen dice «sin MCU», la
//       corriente se lee en mA y la sobrecorriente -un observable `alarma`-
//       se pinta en rojo, con el título de su recuadro, y se apaga al volver.
// =============================================================================
#include <cmath>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QAction>
#include <QKeySequence>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
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
        QPushButton* fija = w->findChild<QPushButton*>("fija:2:0");
        comprueba(fija && fija->text() == "switch" && fija->isCheckable() &&
                      !fija->isChecked() && !fija->isEnabled() &&
                      cuantos_con_prefijo(w, "fija:") == 1,
                  "y el mando de tipo boton lleva al lado su «switch»: marcable, suelto "
                  "y desactivado como el otro. Uno, porque solo hay un boton");
        auto* rebote = w->findChild<QSlider*>("mando:2:1");
        auto* rebote_v = w->findChild<QLabel*>("valor:2:1");
        comprueba(rebote && rebote_v && !rebote->isEnabled() && rebote->value() == 100 &&
                      rebote_v->text() == "2",
                  "un mando continuo nace donde esta el modelo -el rebote de B1, en 2 de "
                  "0 a 20: el deslizador en 100 de 1000- y dice su valor al lado");
        auto* cuantos = w->findChild<QComboBox*>("mando:2:2");
        comprueba(cuantos && !cuantos->isEnabled() && cuantos->count() == 9 &&
                      cuantos->itemText(0) == "1" && cuantos->itemText(8) == "9" &&
                      cuantos->currentText() == "5",
                  "y uno discreto es un desplegable con los enteros del rango -de 1 a 9- "
                  "en el valor del modelo, 5");
        {
            auto* panel = qobject_cast<Panel*>(w);
            std::vector<std::pair<int, float>> o;
            QObject::connect(panel, &Panel::orden,
                             [&](quint16, quint16 m, float v) { o.push_back({m, v}); });
            panel->activa_mandos(true);
            cuantos->setCurrentIndex(2);
            comprueba(o.size() == 1 && o[0].first == 2 && o[0].second == 3.f,
                      "elegir el 3 ordena rebotes = 3, y nada mas");
            panel->activa_mandos(false);
        }
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
        comprueba(w2->findChild<QLabel*>("valor:0:0") &&
                      w2->findChild<QLabel*>("valor:0:0")->text() == "90",
                  "y el numero de al lado sigue al deslizador: 90");
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
    std::printf("G1b El «switch»: el boton, puesto hasta que se vuelva a tocar\n");
    {
        PlacaGui placa;
        PiezaGui b1;
        b1.idx = 0; b1.id = "B1"; b1.tipo = "Button";
        b1.observables.push_back({0, 0, "pulsado", "", 0, 1, true});
        b1.mandos.push_back({0, "pulsar", "boton", 0, 1});
        placa.piezas.push_back(b1);
        auto* panel = construye_panel(placa);
        std::vector<float> ords;
        QObject::connect(panel, &Panel::orden,
                         [&](quint16, quint16, float v) { ords.push_back(v); });
        auto* dedo = panel->findChild<QPushButton*>("mando:0:0");
        auto* sw   = panel->findChild<QPushButton*>("fija:0:0");
        panel->activa_mandos(true);
        comprueba(dedo && sw && dedo->isEnabled() && sw->isEnabled(),
                  "activa_mandos() enciende los dos");
        auto ordenes = [&] {
            std::string t;
            for (float v : ords) t += v >= 0.5f ? '1' : '0';
            return t;
        };
        sw->setChecked(true);
        comprueba(ordenes() == "1", "el «switch» puesto ordena pulsar = 1, y se queda");
        emit dedo->pressed();
        emit dedo->released();
        comprueba(ordenes() == "1",
                  "con el «switch» puesto, el boton de siempre no ordena nada: ni al "
                  "hundirlo ni, sobre todo, al soltarlo, que soltaria lo que el "
                  "«switch» sujeta");
        sw->setChecked(false);
        comprueba(ordenes() == "10", "quitar el «switch» ordena pulsar = 0");
        emit dedo->pressed();
        sw->setChecked(true);
        sw->setChecked(false);
        comprueba(ordenes() == "101",
                  "con el dedo abajo, poner y quitar el «switch» no ordena nada: el "
                  "boton sigue hundido todo el rato");
        emit dedo->released();
        comprueba(ordenes() == "1010", "y al soltar el dedo, por fin, 0");
        emit dedo->pressed();
        sw->setChecked(true);
        emit dedo->released();
        comprueba(ordenes() == "10101" && sw->isChecked(),
                  "hundir, poner el «switch» y soltar deja el boton hundido: el "
                  "«switch» lo sujeta");
        sw->setChecked(false);
        comprueba(ordenes() == "101010", "hasta que se quita");
        delete panel;
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
        auto* encima = v.findChild<QAction*>("siempre_encima");
        comprueba(encima && encima->isCheckable() && !encima->isChecked() &&
                      encima->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_T) &&
                      !(v.windowFlags() & Qt::WindowStaysOnTopHint),
                  "Vista > Siempre encima existe, con Ctrl+T, y empieza sin marcar");
        encima->trigger();
        comprueba(encima->isChecked() && (v.windowFlags() & Qt::WindowStaysOnTopHint) &&
                      v.isVisible() && v.configuracion().siempre_encima,
                  "marcada, la ventana se queda por encima de las demas -y sigue a la "
                  "vista: cambiar las banderas la esconde-");
        encima->trigger();
        comprueba(!(v.windowFlags() & Qt::WindowStaysOnTopHint) && v.isVisible() &&
                      !v.configuracion().siempre_encima,
                  "y desmarcada, vuelve a ser una ventana como las demas");

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
        Arranca ar{};
        comprueba(m.espera_leidos(5) && m.leido[4].tipo == T_ARRANCA && !arrancar->isEnabled() &&
                  parar->isEnabled() &&
                  (std::memcpy(&ar, m.leido[4].cuerpo.constData(), sizeof ar), true) &&
                  ar.ritmo == RIT_REAL && ar.factor == 1.f,
                  "pulsar Arrancar manda T_ARRANCA, a tiempo real, que es el ritmo por "
                  "omision; Arrancar se desactiva, y Parar sigue (fase 6)");

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

    // -------------------------------------------------------------------------
    std::printf("G5 El control\n");
    {
        VentanaPrincipal v(0);
        v.show();
        auto* ritmo  = v.findChild<QComboBox*>("ritmo");
        auto* arr    = v.findChild<QPushButton*>("arrancar");
        auto* pausa  = v.findChild<QPushButton*>("pausa");
        auto* paso   = v.findChild<QPushButton*>("paso");
        auto* paso_ms= v.findChild<QSpinBox*>("paso_ms");
        auto* parar  = v.findChild<QPushButton*>("parar");
        auto* relojes= v.findChild<QLabel*>("relojes");
        comprueba(ritmo && pausa && paso && paso_ms && parar && !ritmo->isEnabled() &&
                  !pausa->isEnabled() && !paso->isEnabled() && !parar->isEnabled(),
                  "sin modelo, el control entero esta apagado");
        ModeloFalso m;
        m.conecta(v.sesion().puerto());
        saluda_hasta_listo(m);
        comprueba(espera([&] { return arr->isEnabled(); }) && ritmo->isEnabled() &&
                  ritmo->currentText() == "tiempo real" && parar->isEnabled() &&
                  !pausa->isEnabled() && !paso->isEnabled(),
                  "con T_LISTO: el ritmo se elige ahora -tiempo real por omision-, y "
                  "Parar; pausa y paso, no");
        ritmo->setCurrentIndex(ritmo->findText("a demanda"));
        arr->click();
        Arranca a{};
        comprueba(m.espera_leidos(3) && m.leido[2].tipo == T_ARRANCA &&
                      (std::memcpy(&a, m.leido[2].cuerpo.constData(), sizeof a), true) &&
                      a.ritmo == RIT_DEMANDA && !ritmo->isEnabled() && !pausa->isEnabled() &&
                      !paso->isEnabled() && parar->isEnabled(),
                  "a demanda: T_ARRANCA con RIT_DEMANDA; el ritmo ya no se cambia, y Paso "
                  "espera a que el modelo diga que esta en pausa");
        m.manda(T_ESTADO, bytes(Estado{F_PAUSADA, 0, 0, 0.05, 3}));
        comprueba(espera([&] { return paso->isEnabled(); }) && paso_ms->isEnabled() &&
                      relojes->text().contains("en pausa"),
                  "T_ESTADO PAUSADA: Paso se habilita, y los relojes dicen 'en pausa'");
        paso_ms->setValue(250);
        paso->click();
        Paso p{};
        comprueba(m.espera_leidos(4) && m.leido[3].tipo == T_PASO &&
                      (std::memcpy(&p, m.leido[3].cuerpo.constData(), sizeof p), true) &&
                      p.ns == 250000000ull && !paso->isEnabled(),
                  "Paso con 250 ms: T_PASO de 250 000 000 ns, y se apaga mientras avanza");
        m.manda(T_ESTADO, bytes(Estado{F_CORRIENDO, 0, 100000000ull, 0.1, 9}));
        m.manda(T_ESTADO, bytes(Estado{F_PAUSADA, 0, 250000000ull, 0.2, 19}));
        comprueba(espera([&] { return paso->isEnabled(); }),
                  "y vuelve cuando el modelo dice que esta otra vez en pausa");
        parar->click();
        comprueba(m.espera_leidos(5) && m.leido[4].tipo == T_PARA,
                  "Parar en marcha: T_PARA");
        m.manda(T_FIN, bytes(Fin{M_PARA, 0, 250000000ull}));
        comprueba(espera([&] {
                      return v.statusBar()->currentMessage().contains("se pidio parar");
                  }) && !parar->isEnabled() && !paso->isEnabled(),
                  "y con su T_FIN, todo apagado: \"" +
                      v.statusBar()->currentMessage().toStdString() + "\"");
    }
    {
        VentanaPrincipal v(0);
        v.show();
        auto* arr   = v.findChild<QPushButton*>("arrancar");
        auto* pausa = v.findChild<QPushButton*>("pausa");
        auto* paso  = v.findChild<QPushButton*>("paso");
        ModeloFalso m;
        m.conecta(v.sesion().puerto());
        saluda_hasta_listo(m);
        espera([&] { return arr->isEnabled(); });
        arr->click();
        comprueba(espera([&] { return pausa->isEnabled(); }) && pausa->text() == "Pausa" &&
                      !paso->isEnabled(),
                  "a tiempo real, en marcha: Pausa habilitado, Paso no");
        pausa->click();
        comprueba(m.espera_leidos(4) && m.leido[3].tipo == T_PAUSA && !pausa->isEnabled(),
                  "Pausa manda T_PAUSA, y el boton espera a que el modelo conteste");
        m.manda(T_ESTADO, bytes(Estado{F_PAUSADA, 0, 7000000ull, 0.3, 50}));
        comprueba(espera([&] { return pausa->isEnabled() && pausa->text() == "Sigue"; }),
                  "T_ESTADO PAUSADA: el boton dice 'Sigue'");
        pausa->click();
        comprueba(m.espera_leidos(5) && m.leido[4].tipo == T_SIGUE, "y manda T_SIGUE");
        m.manda(T_ESTADO, bytes(Estado{F_CORRIENDO, 0, 7000000ull, 0.9, 51}));
        comprueba(espera([&] { return pausa->isEnabled() && pausa->text() == "Pausa"; }),
                  "T_ESTADO CORRIENDO: otra vez 'Pausa'");
    }

    // -------------------------------------------------------------------------
    std::printf("G6 Una placa sin MCU, y una alarma que se nota\n");
    {
        ObservableGui so{1, 1, "sobrecorriente", "", 0, 1, true, true};
        comprueba(Panel::texto_de(so, 0.f) == QString::fromUtf8("○") &&
                      Panel::texto_de(so, 1.f) == QString::fromUtf8("⚠ SI") &&
                      !Panel::en_alarma(so, 0.f) && Panel::en_alarma(so, 1.f) &&
                      !Panel::en_alarma(ObservableGui{0, 0, "encendido", "", 0, 1, true}, 1.f),
                  "un observable `alarma` se escribe ○ en reposo y ⚠ SI disparado; uno que no "
                  "lo es, aunque valga 1, no esta en alarma");

        VentanaPrincipal v(0);
        v.show();
        auto* resumen = v.findChild<QLabel*>("resumen");
        ModeloFalso m;
        m.conecta(v.sesion().puerto());
        m.manda(T_HOLA, "protocolo_max=1\nplaca=placas/fuente_y_masa.xml\nmcu=\nfirmware=\n");
        comprueba(m.espera_leidos(1) && m.leido[0].tipo == T_VERSION, "saludo con mcu= vacio");
        m.version(1);
        m.manda(T_PLACA,
                "<placa nombre=\"fuente-y-masa\">\n"
                "  <componente tipo=\"Fuente\" id=\"F1\" limite_ma=\"20\">\n"
                "    <pin nombre=\"pin\" nodo=\"vcc\"/>\n"
                "  </componente>\n"
                "</placa>\n");
        m.manda(T_CATALOGO,
                "<catalogo>\n"
                "  <pieza idx=\"0\" id=\"F1\" tipo=\"Fuente\">\n"
                "    <observable idx=\"0\" id_obs=\"0\" nombre=\"corriente\" unidad=\"mA\" "
                "min=\"-20\" max=\"20\" interesante=\"si\"/>\n"
                "    <observable idx=\"1\" id_obs=\"1\" nombre=\"sobrecorriente\" unidad=\"\" "
                "min=\"0\" max=\"1\" interesante=\"si\" alarma=\"si\"/>\n"
                "  </pieza>\n"
                "</catalogo>\n");
        m.manda(T_LISTO);
        auto* arr = v.findChild<QPushButton*>("arrancar");
        comprueba(espera([&] { return arr->isEnabled(); }) &&
                      v.sesion().placa().piezas.size() == 1 &&
                      v.sesion().placa().piezas[0].observables.size() == 2 &&
                      !v.sesion().placa().piezas[0].observables[0].alarma &&
                      v.sesion().placa().piezas[0].observables[1].alarma,
                  "el catalogo se lee con su alarma: `sobrecorriente` lo es y `corriente` no");
        comprueba(resumen && resumen->text().contains("fuente-y-masa") &&
                      resumen->text().contains("sin MCU"),
                  "el resumen dice \"sin MCU\", no un hueco: \"" +
                      resumen->text().toStdString() + "\"");
        arr->click();
        auto* cor = v.findChild<QLabel*>("obs:0");
        auto* sob = v.findChild<QLabel*>("obs:1");
        auto* caja = v.findChild<QGroupBox*>("pieza:0");
        auto inst = [&](uint64_t t, float c, float s) {
            CabInstantanea in{t, 2, 0};
            m.manda(T_INSTANTANEA, bytes(in) + bytes(Muestra{0, 0, c}) + bytes(Muestra{1, 0, s}));
        };
        inst(1000000, 8.48f, 0.f);
        comprueba(cor && sob && caja &&
                      espera([&] { return cor->text() == "8.48 mA"; }) &&
                      sob->text() == QString::fromUtf8("○") && sob->styleSheet().isEmpty() &&
                      !caja->property("alarma").toBool(),
                  "en reposo: la corriente, 8.48 mA, y la alarma apagada");
        inst(2000000, 20.f, 1.f);
        comprueba(espera([&] { return sob->text() == QString::fromUtf8("⚠ SI"); }) &&
                      cor->text() == "20 mA" && sob->styleSheet().contains("#c62828") &&
                      caja->property("alarma").toBool() &&
                      caja->styleSheet().contains("#c62828"),
                  "con sobrecorriente: 20 mA, ⚠ SI en rojo y el titulo de F1 en rojo");
        inst(3000000, 8.48f, 0.f);
        comprueba(espera([&] { return sob->text() == QString::fromUtf8("○"); }) &&
                      sob->styleSheet().isEmpty() && !caja->property("alarma").toBool() &&
                      caja->styleSheet().isEmpty(),
                  "y al pasar, todo vuelve a su color");
    }

    return resultado();
}
