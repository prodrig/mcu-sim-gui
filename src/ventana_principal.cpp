#include "ventana_principal.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include "dialogo_lanzamiento.h"
#include "panel.h"

namespace mcusim {

namespace {
Configuracion con_puerto(quint16 p)
{
    Configuracion c;
    c.puerto = p;
    return c;
}
int indice_de_ritmo(const QString& r)
{
    if (r == QLatin1String("mitad"))   return 1;
    if (r == QLatin1String("libre"))   return 2;
    if (r == QLatin1String("demanda")) return 3;
    return 0;
}
const char* const RITMOS[] = {"real", "mitad", "libre", "demanda"};
} // namespace

VentanaPrincipal::VentanaPrincipal(quint16 puerto, QWidget* padre)
    : VentanaPrincipal(con_puerto(puerto), padre)
{
}

VentanaPrincipal::VentanaPrincipal(const Configuracion& c, QWidget* padre)
    : QMainWindow(padre), cfg_(c)
{
    construye();
}

void VentanaPrincipal::construye()
{
    setWindowTitle(tr("mcu-sim-gui"));
    resize(1000, 700);

    auto* cuerpo = new QWidget(this);
    auto* caja   = new QVBoxLayout(cuerpo);

    auto* fila = new QHBoxLayout;
    resumen_ = new QLabel(cuerpo);
    resumen_->setObjectName(QStringLiteral("resumen"));
    resumen_->setTextFormat(Qt::RichText);
    resumen_->setWordWrap(true);
    arrancar_ = new QPushButton(tr("Arrancar"), cuerpo);
    arrancar_->setObjectName(QStringLiteral("arrancar"));
    parar_ = new QPushButton(tr("Parar"), cuerpo);
    parar_->setObjectName(QStringLiteral("parar"));
    // Fase 6: el ritmo, que se elige ANTES de arrancar, y el control en marcha
    ritmo_ = new QComboBox(cuerpo);
    ritmo_->setObjectName(QStringLiteral("ritmo"));
    ritmo_->addItem(tr("tiempo real"), QVariantList{proto::RIT_REAL, 1.0});
    ritmo_->addItem(tr("a la mitad"),  QVariantList{proto::RIT_REAL, 0.5});
    ritmo_->addItem(tr("libre"),       QVariantList{proto::RIT_LIBRE, 1.0});
    ritmo_->addItem(tr("a demanda"),   QVariantList{proto::RIT_DEMANDA, 1.0});
    ritmo_->setToolTip(tr("Tiempo real: un segundo simulado por segundo de reloj. Libre: "
                          "todo lo deprisa que se pueda. A demanda: arranca en pausa y "
                          "solo avanza con Paso."));
    pausa_ = new QPushButton(tr("Pausa"), cuerpo);
    pausa_->setObjectName(QStringLiteral("pausa"));
    paso_ = new QPushButton(tr("Paso"), cuerpo);
    paso_->setObjectName(QStringLiteral("paso"));
    paso_ms_ = new QSpinBox(cuerpo);
    paso_ms_->setObjectName(QStringLiteral("paso_ms"));
    paso_ms_->setRange(1, 60000);
    paso_ms_->setValue(100);
    paso_ms_->setSuffix(tr(" ms"));
    relojes_ = new QLabel(cuerpo);
    relojes_->setObjectName(QStringLiteral("relojes"));
    fila->addWidget(resumen_, 1);
    fila->addWidget(relojes_);
    fila->addWidget(ritmo_);
    fila->addWidget(arrancar_);
    fila->addWidget(pausa_);
    fila->addWidget(paso_ms_);
    fila->addWidget(paso_);
    fila->addWidget(parar_);
    caja->addLayout(fila);

    centro_ = new QScrollArea(cuerpo);
    centro_->setWidgetResizable(true);
    caja->addWidget(centro_, 1);

    // Abajo, dos pestañas: los avisos que llegan por el protocolo, y lo que
    // mcu-sim dice por su salida estándar y de error (fase 7)
    abajo_ = new QTabWidget(cuerpo);
    abajo_->setObjectName(QStringLiteral("abajo"));
    abajo_->setMaximumHeight(180);
    avisos_ = new QListWidget(abajo_);
    avisos_->setObjectName(QStringLiteral("avisos"));
    consola_ = new QPlainTextEdit(abajo_);
    consola_->setObjectName(QStringLiteral("consola"));
    consola_->setReadOnly(true);
    consola_->setMaximumBlockCount(5000);
    consola_->setFont(QFont(QStringLiteral("monospace")));
    abajo_->addTab(avisos_, tr("Avisos"));
    abajo_->addTab(consola_, tr("mcu-sim"));
    caja->addWidget(abajo_);
    setCentralWidget(cuerpo);

    ritmo_->setCurrentIndex(indice_de_ritmo(cfg_.ritmo));

    // --- El menú: lanzar, otra vez, detener ------------------------------
    QMenu* m = menuBar()->addMenu(tr("&Simulacion"));
    act_lanzar_ = m->addAction(tr("&Lanzar mcu-sim..."), this, &VentanaPrincipal::abre_dialogo);
    act_lanzar_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    act_otra_ = m->addAction(tr("Lanzar &otra vez"), this, [this] { lanza(); });
    act_otra_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    act_detener_ = m->addAction(tr("&Detener mcu-sim"), this, [this] {
        consola(tr("— se pide a mcu-sim que termine —"), QStringLiteral("gray"));
        lanz_.detiene();
    });
    m->addSeparator();
    m->addAction(tr("&Salir"), this, &QWidget::close)->setShortcut(QKeySequence::Quit);

    // --- Vista: la ventana por encima de las demas ------------------------
    // Para depurar en el IDE con la placa a la vista: los LED se ven y los
    // botones -y su «switch»- se tocan sin traer esta ventana delante cada
    // vez, que es lo que la tapa en cuanto se vuelve al IDE.
    QMenu* vista = menuBar()->addMenu(tr("&Vista"));
    act_encima_ = vista->addAction(tr("Siempre &encima"));
    act_encima_->setObjectName(QStringLiteral("siempre_encima"));
    act_encima_->setCheckable(true);
    act_encima_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    act_encima_->setChecked(cfg_.siempre_encima);
    if (cfg_.siempre_encima) setWindowFlag(Qt::WindowStaysOnTopHint, true);
    connect(act_encima_, &QAction::toggled, this, [this](bool si) {
        // Cambiar las banderas de una ventana la esconde: hay que volver a
        // enseñarla, y solo si ya se estaba viendo
        const bool visible = isVisible();
        setWindowFlag(Qt::WindowStaysOnTopHint, si);
        if (visible) show();
        if (si != cfg_.siempre_encima && !cfg_.ruta.isEmpty()) {
            cfg_.siempre_encima = si;
            QString e;
            cfg_.guarda(e);
        }
        cfg_.siempre_encima = si;
    });

    connect(&lanz_, &Lanzador::linea, this, [this](const QString& t, bool err) {
        consola(t, err ? QStringLiteral("#c0392b") : QString());
    });
    connect(&lanz_, &Lanzador::fallo, this, [this](const QString& por) {
        consola(por, QStringLiteral("#c0392b"));
        statusBar()->showMessage(por);
        abajo_->setCurrentWidget(consola_);
        pon_controles();
    });
    connect(&lanz_, &Lanzador::termino, this, &VentanaPrincipal::hijo_termino);

    connect(arrancar_, &QPushButton::clicked, this, [this] {
        const QVariantList r = ritmo_->currentData().toList();
        // El ritmo elegido se recuerda para la proxima vez
        const QString elegido = QString::fromLatin1(RITMOS[qBound(0, ritmo_->currentIndex(), 3)]);
        if (elegido != cfg_.ritmo && !cfg_.ruta.isEmpty()) {
            cfg_.ritmo = elegido;
            QString e;
            cfg_.guarda(e);
        }
        if (ses_.arranca(r.value(0).toUInt(), r.value(1).toFloat())) {
            statusBar()->showMessage(ses_.ritmo() == proto::RIT_DEMANDA
                                         ? tr("a demanda: en pausa hasta que pulses Paso")
                                         : tr("simulando"));
            pon_controles();
        }
    });
    connect(parar_, &QPushButton::clicked, this, [this] {
        if (ses_.para()) parar_->setEnabled(false);
    });
    // La pausa la decide el modelo: el boton dice lo que dijo el ultimo
    // T_ESTADO, no lo que se pidio
    connect(pausa_, &QPushButton::clicked, this, [this] {
        if (ses_.pausada() ? ses_.sigue() : ses_.pausa()) pausa_->setEnabled(false);
    });
    connect(paso_, &QPushButton::clicked, this, [this] {
        if (ses_.paso(quint64(paso_ms_->value()) * 1000000ull)) {
            paso_->setEnabled(false);       // hasta que diga que vuelve a estar en pausa
            paso_ms_->setEnabled(false);
        }
    });

    connect(&ses_, &Sesion::conectado, this, [this] {
        avisos_->clear();
        relojes_->clear();
        conecto_hijo_ = lanz_.corriendo();
        statusBar()->showMessage(tr("mcu-sim conectado; saludando"));
    });
    connect(&ses_, &Sesion::placa_lista, this, &VentanaPrincipal::pon_placa);
    connect(&ses_, &Sesion::listo, this, [this] {
        // Antes de arrancar: asi la secuencia se repite al picosegundo
        if (panel_ && !panel_->pintados().isEmpty())
            ses_.suscribe(quint64(cfg_.periodo_ms * 1e6 + 0.5), panel_->pintados());
        pon_controles();
        // Lo que se toque desde ya se aplica en t = 0 (doc/protocolo.md §5)
        if (panel_) panel_->activa_mandos(true);
        statusBar()->showMessage(tr("el modelo esta construido y esperando: pulsa Arrancar"));
    });
    connect(&ses_, &Sesion::fin, this, &VentanaPrincipal::termina);
    connect(&ses_, &Sesion::orden_hecha, this, &VentanaPrincipal::pon_eco);
    connect(&ses_, &Sesion::muestra, this, [this](quint16 id, float v) {
        if (panel_) panel_->pon_valor(id, v);
    });
    connect(&ses_, &Sesion::aviso, this, &VentanaPrincipal::pon_aviso);
    connect(&ses_, &Sesion::estado_modelo, this, [this](quint32, quint64 t, double p, quint64 d) {
        pon_relojes(t, p, d);
        pon_controles();
    });
    connect(&ses_, &Sesion::desconectado, this, [this](const QString& m) {
        pon_controles();
        apaga_mandos();
        if (!m.isEmpty())
            statusBar()->showMessage(tr("conexion cerrada: %1").arg(m));
    });
    connect(&ses_, &Sesion::problema, this, [this](const QString& t) {
        statusBar()->showMessage(t);
    });

    escucha();
    espera_modelo();
}

// Escucha PRIMERO, que es lo que quita la carrera de arranque: cuando el hijo
// intente conectarse, esto lleva puesto desde antes de que existiera. Si el
// puerto de la configuración está cogido, cualquier otro: el hijo recibe en
// `--gui` el que haya, y nadie tiene que enterarse.
void VentanaPrincipal::escucha()
{
    QHostAddress dir(QHostAddress::LocalHost);
    if (!cfg_.host.isEmpty() && cfg_.host != QLatin1String("localhost")) {
        QHostAddress h;
        if (h.setAddress(cfg_.host)) dir = h;
    }
    bool ok = ses_.escucha(dir, cfg_.puerto);
    if (!ok && cfg_.puerto != 0) ok = ses_.escucha(dir, 0);
    puerto_ = ok ? ses_.puerto() : 0;
}

QString VentanaPrincipal::destino_gui() const
{
    QString h = cfg_.host;
    if (h.isEmpty() || h == QLatin1String("localhost") || h == QLatin1String("0.0.0.0") ||
        h == QLatin1String("::"))
        h = QStringLiteral("127.0.0.1");
    if (h.contains(QLatin1Char(':'))) h = QLatin1Char('[') + h + QLatin1Char(']');
    return QStringLiteral("%1:%2").arg(h).arg(puerto_);
}

void VentanaPrincipal::consola(const QString& texto, const QString& color)
{
    if (color.isEmpty()) consola_->appendPlainText(texto);
    else
        consola_->appendHtml(QStringLiteral("<span style=\"color:%1\">%2</span>")
                                 .arg(color, texto.toHtmlEscaped()));
}

bool VentanaPrincipal::lanza()
{
    if (lanz_.corriendo()) {
        statusBar()->showMessage(tr("ya hay un mcu-sim lanzado desde aqui: detenlo antes"));
        return false;
    }
    using E = Sesion::Estado;
    if (ses_.estado() != E::Escuchando && ses_.estado() != E::Terminada) {
        statusBar()->showMessage(tr("ya hay un mcu-sim conectado"));
        return false;
    }
    if (puerto_ == 0) {
        statusBar()->showMessage(tr("no se puede escuchar en ningun puerto: %1").arg(ses_.error()));
        return false;
    }
    const QString exe = cfg_.ejecutable_absoluto();
    const QString dir = cfg_.directorio_absoluto();
    if (args_.vacio()) {
        QString e;
        const QByteArray s = Lanzador::argumentos_de(exe, dir, e);
        if (!s.isEmpty() && !lee_argumentos(s, args_, e)) args_ = ArgumentosCli();
        if (args_.vacio() && !e.isEmpty())
            consola(tr("sin lista de opciones (%1): se lanza con la placa, el firmware y lo "
                       "escrito a mano").arg(e), QStringLiteral("gray"));
    }
    QString error;
    QStringList a = linea_de_ordenes(args_, cfg_.argumentos, cfg_.a_mano, &error);
    if (!error.isEmpty()) {
        statusBar()->showMessage(tr("no se puede lanzar: %1").arg(error));
        return false;
    }
    a << QStringLiteral("--gui") << destino_gui();
    conecto_hijo_ = false;
    if (!lanz_.lanza(exe, dir, a)) return false;
    consola(QStringLiteral("$ ") + lanz_.orden(), QStringLiteral("gray"));
    abajo_->setCurrentWidget(consola_);
    statusBar()->showMessage(tr("mcu-sim lanzado; esperando a que se conecte"));
    pon_controles();
    return true;
}

void VentanaPrincipal::abre_dialogo()
{
    DialogoLanzamiento d(cfg_, destino_gui(), args_.vacio() ? nullptr : &args_, this);
    if (d.exec() != QDialog::Accepted) return;
    cfg_ = d.configuracion();
    args_ = d.argumentos();
    QString e;
    if (!cfg_.ruta.isEmpty()) {
        if (cfg_.guarda(e)) cfg_.existia = true;     // la proxima vez, «leida de»
        else statusBar()->showMessage(e);
    }
    lanza();
}

void VentanaPrincipal::hijo_termino(int codigo, bool estrellado)
{
    const QString t = estrellado
        ? tr("— mcu-sim se ha estrellado, o lo han matado —")
        : tr("— mcu-sim ha terminado con codigo %1 —").arg(codigo);
    consola(t, estrellado || codigo != 0 ? QStringLiteral("#c0392b") : QStringLiteral("gray"));
    if (estrellado || codigo != 0) {
        statusBar()->showMessage(estrellado ? tr("mcu-sim se ha estrellado, o lo han matado")
                                            : tr("mcu-sim ha terminado con codigo %1").arg(codigo));
        abajo_->setCurrentWidget(consola_);
    }
    if (!conecto_hijo_)
        resumen_->setText(tr("<b>mcu-sim ha terminado sin llegar a conectarse.</b> Lo que dijo "
                             "esta abajo, en la pestana <i>mcu-sim</i>."));
    pon_controles();
}

// Cerrar con el modelo corriendo: primero se le pide parar, que es el final
// ordenado -su resumen, su T_FIN-; si en dos segundos no ha terminado, se le
// mata. Un hijo no sobrevive a su ventana.
void VentanaPrincipal::closeEvent(QCloseEvent* e)
{
    if (lanz_.corriendo()) {
        if (!ses_.para()) lanz_.detiene();
        QElapsedTimer t;
        t.start();
        while (lanz_.corriendo() && t.elapsed() < 2000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        if (lanz_.corriendo()) lanz_.detiene(1000);
    }
    e->accept();
}

void VentanaPrincipal::espera_modelo()
{
    pon_controles();
    if (puerto_ == 0)
        resumen_->setText(tr("<b>No se puede escuchar en ningun puerto</b>: %1")
                              .arg(ses_.error().toHtmlEscaped()));
    else
        resumen_->setText(
            tr("<b>Esperando a mcu-sim</b> en %1. Lanzalo con <i>Simulacion &gt; Lanzar "
               "mcu-sim</i> (Ctrl+L), o desde una consola con <code>mcu-sim placa.xml "
               "firmware.bin --gui %1</code>.").arg(destino_gui()));
    auto* vacio = new QLabel(tr("Aqui aparecera la placa: una pieza por recuadro, con lo "
                                "que deja ver y lo que se le puede hacer."), centro_);
    vacio->setAlignment(Qt::AlignCenter);
    vacio->setEnabled(false);
    centro_->setWidget(vacio);
}

void VentanaPrincipal::pon_placa()
{
    const PlacaGui& p = ses_.placa();
    const auto& h = ses_.hola();
    QString fw = h.value(QStringLiteral("firmware"));
    if (fw.isEmpty()) fw = tr("sin firmware");
    // Una placa que no declara su MCU puede recibirlo de `--mcu`, y entonces
    // no esta en el XML: lo dice T_HOLA. Si tampoco lo dice el, la placa va
    // sin MCU, que es una placa legitima (una Fuente, una Gnd y lo que
    // cuelgue) y no un dato que falte.
    QString mcus = p.mcus.isEmpty() ? h.value(QStringLiteral("mcu"))
                                    : p.mcus.join(QStringLiteral(", "));
    if (mcus.isEmpty()) mcus = tr("sin MCU");
    QString texto = tr("<b>%1</b> &nbsp; %2 &nbsp; %3 &nbsp; <i>%4 piezas, %5 indicadores, "
                       "%6 mandos</i>")
                        .arg(p.nombre.toHtmlEscaped(), mcus.toHtmlEscaped(),
                             fw.toHtmlEscaped())
                        .arg(p.piezas.size()).arg(p.n_interesantes()).arg(p.n_mandos());
    if (h.value(QStringLiteral("modo")) == QLatin1String("valida"))
        texto += tr(" &nbsp; <b>(solo validacion: no se simulara)</b>");
    resumen_->setText(texto);
    panel_ = construye_panel(p);
    connect(panel_, &Panel::orden, this, [this](quint16 pieza, quint16 mando, float v) {
        if (!ses_.ordena(pieza, mando, v))
            statusBar()->showMessage(tr("no se puede ordenar: el modelo no esta esperando "
                                        "ni corriendo"));
    });
    centro_->setWidget(panel_);
}

void VentanaPrincipal::pon_aviso(quint32 nivel, quint64 t_sim_ns, const QString& origen,
                                 const QString& texto)
{
    static const char* const nombre[] = {"info", "aviso", "error", "fatal"};
    const QString n = QString::fromLatin1(nivel < 4 ? nombre[nivel] : "?");
    auto* it = new QListWidgetItem(QStringLiteral("[%1] t = %2 ms · %3: %4")
                                       .arg(n)
                                       .arg(double(t_sim_ns) / 1e6, 0, 'f', 3)
                                       .arg(origen, texto),
                                   avisos_);
    if (nivel >= proto::N_ERROR) it->setForeground(Qt::red);
    else if (nivel == proto::N_AVISO) it->setForeground(QColor(0xB0, 0x6A, 0x00));
    avisos_->scrollToBottom();
}

void VentanaPrincipal::pon_relojes(quint64 t_sim_ns, double t_pared_s, quint64 deltas)
{
    QString t = tr("t = %1 ms · pared %2 s · %3 deltas")
                    .arg(double(t_sim_ns) / 1e6, 0, 'f', 3)
                    .arg(t_pared_s, 0, 'f', 2)
                    .arg(deltas);
    if (ses_.pausada()) t += tr(" · <b>en pausa</b>");
    if (ses_.perdidas() > 0)
        t += tr(" · <b>va por detras: %1 instantaneas perdidas</b>").arg(ses_.perdidas());
    relojes_->setText(t);
}

void VentanaPrincipal::pon_eco(quint64 t_sim_ns, quint16 pieza, quint16 mando, float valor,
                               quint32 resultado)
{
    // RES_OK no se dice: se ve en los indicadores. RES_RANGO tampoco: el
    // modelo ya manda su propio T_AVISO, con lo que pidio y lo que aplico.
    if (resultado == proto::RES_OK || resultado == proto::RES_RANGO) return;
    QString quien = tr("pieza %1, mando %2").arg(pieza).arg(mando);
    const PlacaGui& p = ses_.placa();
    if (pieza < p.piezas.size()) {
        const PiezaGui& pz = p.piezas[pieza];
        quien = pz.id;
        if (mando < pz.mandos.size()) quien += QStringLiteral(".") + pz.mandos[mando].nombre;
        else                          quien += tr(", mando %1").arg(mando);
    }
    const QString por =
        resultado == proto::RES_PIEZA ? tr("no existe esa pieza")
      : resultado == proto::RES_MANDO ? tr("esa pieza no tiene ese mando")
      : resultado == proto::RES_TARDE ? tr("llego tarde: se aplico al recibirla")
                                      : tr("resultado %1").arg(resultado);
    pon_aviso(resultado == proto::RES_TARDE ? proto::N_INFO : proto::N_AVISO, t_sim_ns,
              tr("ventana"), tr("orden a %1 (%2): %3").arg(quien).arg(valor).arg(por));
}

void VentanaPrincipal::pon_controles()
{
    using E = Sesion::Estado;
    const E e = ses_.estado();
    const bool lista = e == E::Lista, corre = e == E::Corriendo;
    const bool demanda = ses_.ritmo() == proto::RIT_DEMANDA;
    ritmo_->setEnabled(lista);
    arrancar_->setEnabled(lista);
    parar_->setEnabled(lista || corre);
    pausa_->setEnabled(corre && !demanda);
    pausa_->setText(corre && ses_.pausada() ? tr("Sigue") : tr("Pausa"));
    paso_->setEnabled(corre && demanda && ses_.pausada());
    paso_ms_->setEnabled(corre && demanda && ses_.pausada());
    if (act_lanzar_) {
        const bool libre = !lanz_.corriendo() &&
                           (e == E::Escuchando || e == E::Terminada) && puerto_ != 0;
        act_lanzar_->setEnabled(libre);
        act_otra_->setEnabled(libre && !cfg_.ejecutable.isEmpty());
        act_detener_->setEnabled(lanz_.corriendo());
    }
}

void VentanaPrincipal::termina(quint32 motivo, qint32 codigo, quint64 t_sim_ns)
{
    pon_controles();
    apaga_mandos();
    const QString por =
        motivo == proto::M_VENTANA ? tr("se agoto la ventana de simulacion")
      : motivo == proto::M_PARA    ? tr("se pidio parar")
      : motivo == proto::M_ERROR   ? tr("el modelo se rindio")
                                   : tr("el modelo se detuvo");
    statusBar()->showMessage(tr("terminado: %1, en t = %2 ms (codigo %3)")
                                 .arg(por)
                                 .arg(double(t_sim_ns) / 1e6, 0, 'f', 3)
                                 .arg(codigo));
}

} // namespace mcusim
