#include "ventana_principal.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QScreen>
#include <QTabWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QJsonValue>
#include <QDir>
#include <QMessageBox>
#include <QSaveFile>
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

    // En el centro, el panel de la placa. La ilustración va aparte, en su
    // propia ventana (`ver_ilustracion`)
    centro_ = new QScrollArea(cuerpo);
    centro_->setObjectName(QStringLiteral("centro"));
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
    vista->addSeparator();
    // La ilustración, en su ventana: marcada, a la vista
    act_ver_ilus_ = vista->addAction(tr("&Ilustracion"));
    act_ver_ilus_->setObjectName(QStringLiteral("ver_ilustracion"));
    act_ver_ilus_->setCheckable(true);
    act_ver_ilus_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    act_ver_ilus_->setEnabled(false);
    connect(act_ver_ilus_, &QAction::toggled, this, [this](bool si) { ver_ilustracion(si); });
    act_dibujo_ = vista->addAction(tr("Abrir &dibujo de la placa..."), this,
                                   [this] { elige_dibujo(); });
    act_dibujo_->setObjectName(QStringLiteral("abrir_dibujo"));
    act_dibujo_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    act_dibujo_->setEnabled(false);
    if (cfg_.siempre_encima) setWindowFlag(Qt::WindowStaysOnTopHint, true);
    connect(act_encima_, &QAction::toggled, this, [this](bool si) {
        // Las dos ventanas: la de la ilustración es la que se quiere ver
        // mientras se depura en el IDE
        pon_encima(this, si);
        if (ventana_ilus_) pon_encima(ventana_ilus_, si);
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
    connect(&ses_, &Sesion::ilustracion, this, &VentanaPrincipal::llega_dibujo);
    connect(&ses_, &Sesion::listo, this, [this] {
        // Antes de arrancar: asi la secuencia se repite al picosegundo
        suscribe();
        pon_controles();
        // Lo que se toque desde ya se aplica en t = 0 (doc/protocolo.md §5)
        if (panel_) panel_->activa_mandos(true);
        if (ilus_) ilus_->activa_mandos(true);
        statusBar()->showMessage(tr("el modelo esta construido y esperando: pulsa Arrancar"));
    });
    connect(&ses_, &Sesion::fin, this, &VentanaPrincipal::termina);
    connect(&ses_, &Sesion::orden_hecha, this, &VentanaPrincipal::pon_eco);
    connect(&ses_, &Sesion::muestra, this, [this](quint16 id, float v) {
        if (panel_) panel_->pon_valor(id, v);
        if (ilus_) ilus_->pon_valor(id, v);
    });
    connect(&ses_, &Sesion::imagen, this,
            [this](quint16 id, quint64, int an, int al, const QByteArray& rgb, float b) {
                if (panel_) panel_->pon_imagen(id, an, al, rgb, b);
                if (ilus_) ilus_->pon_imagen(id, an, al, rgb, b);
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
    // Plan §36: si se estaba colocando placas, lo colocado se guarda
    if (ilus_) ilus_->termina_edicion();
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

// -----------------------------------------------------------------------------
// La ventana de la ilustración
void VentanaPrincipal::ver_ilustracion(bool si)
{
    if (!ventana_ilus_) return;
    if (!si) {
        ventana_ilus_->hide();
        return;
    }
    if (!ilus_colocada_) {
        // La primera vez: tan grande como pide el dibujo -sin pasarse de la
        // pantalla-, y al lado de esta ventana si cabe, que no la tape
        ilus_colocada_ = true;
        QScreen* pantalla = screen();
        const QRect libre = pantalla ? pantalla->availableGeometry() : QRect(0, 0, 1280, 800);
        QSize t = ilus_ ? ilus_->sizeHint() : QSize();
        if (!t.isValid() || t.width() < 400 || t.height() < 300) t = QSize(800, 600);
        t = t.boundedTo(QSize(libre.width() * 3 / 4, libre.height() * 9 / 10));
        ventana_ilus_->resize(t);
        const QRect yo = frameGeometry();
        if (yo.right() + 8 + t.width() <= libre.right())
            ventana_ilus_->move(yo.right() + 8, yo.top());
        else if (yo.left() - 8 - t.width() >= libre.left())
            ventana_ilus_->move(yo.left() - 8 - t.width(), yo.top());
    }
    ventana_ilus_->show();
    ventana_ilus_->raise();
}

void VentanaPrincipal::pon_encima(QWidget* w, bool si)
{
    // Cambiar las banderas de una ventana la esconde: hay que volver a
    // enseñarla, y solo si ya se estaba viendo
    const bool visible = w->isVisible();
    w->setWindowFlag(Qt::WindowStaysOnTopHint, si);
    if (visible) w->show();
}

bool VentanaPrincipal::eventFilter(QObject* o, QEvent* e)
{
    // Cerrar la ventana de la ilustración solo la esconde, y lo dice el menú
    if (o == ventana_ilus_ && e->type() == QEvent::Close) {
        e->ignore();
        act_ver_ilus_->setChecked(false);
        return true;
    }
    return QMainWindow::eventFilter(o, e);
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
    // Un sistema dice sus placas y cuántos acoples las unen; el resto es igual
    if (p.es_sistema()) {
        QStringList pl;
        for (const SubPlacaGui& x : p.placas)
            pl << QStringLiteral("%1: %2").arg(x.id, x.nombre).toHtmlEscaped();
        texto += tr("<br>%n placa(s) &nbsp; %1", nullptr, int(p.placas.size()))
                     .arg(pl.join(QStringLiteral(", ")));
        if (!p.acoples.isEmpty() || p.n_hilos)
            texto += tr(" &nbsp; <i>%1 acople(s), %2 hilo(s)</i>")
                         .arg(p.acoples.size()).arg(p.n_hilos);
    }
    if (h.value(QStringLiteral("modo")) == QLatin1String("valida"))
        texto += tr(" &nbsp; <b>(solo validacion: no se simulara)</b>");
    resumen_->setText(texto);
    panel_ = construye_panel(p);
    connect(panel_, &Panel::orden, this, &VentanaPrincipal::ordena);
    centro_->setWidget(panel_);

    // La ilustración: un recuadro por placa, con el dibujo que se recuerde de
    // una placa que se llame igual
    if (!ventana_ilus_) {
        // Una ventana de verdad, hija de esta: se va con ella, no cuenta como
        // «la última ventana» para salir, y no tiene botón en la barra de
        // tareas propio en todos los sistemas, pero se mueve y se agranda
        // sola. Cerrarla solo la esconde (`eventFilter`)
        ventana_ilus_ = new QWidget(this, Qt::Window);
        ventana_ilus_->setObjectName(QStringLiteral("ventana_ilustracion"));
        auto* c = new QVBoxLayout(ventana_ilus_);
        c->setContentsMargins(0, 0, 0, 0);
        ventana_ilus_->installEventFilter(this);
        if (cfg_.siempre_encima) ventana_ilus_->setWindowFlag(Qt::WindowStaysOnTopHint, true);
    }
    if (ilus_) ilus_->termina_edicion();          // lo que estuviera colocando, guardado
    delete ilus_;
    ilus_ = new VistaIlustracion(p, ventana_ilus_);
    // Plan §36: lo que se colocó la otra vez, y guardar lo que se coloque
    {
        const QJsonValue d = cfg_.disposiciones.value(ilus_->clave_disposicion());
        if (d.isObject()) ilus_->pon_disposicion(d.toObject());
    }
    // Plan §38: el XML, si se sabe dónde está, y lo que la ilustración dice
    ilus_->pon_ruta_xml(ruta_xml());
    connect(ilus_, &VistaIlustracion::dice, this,
            [this](const QString& t) { statusBar()->showMessage(t); });
    connect(ilus_, &VistaIlustracion::pide_guardar_xml, this, [this] {
        const QString ruta = ilus_ ? ilus_->ruta_xml() : QString();
        if (ruta.isEmpty()) return;
        // Una copia de `make datos`: se puede escribir, pero no es el original
        const bool copia = QDir::fromNativeSeparators(ruta).contains(QLatin1String("/build/"));
        QString t = tr("Escribir lo colocado -donde va cada placa, su giro y su escala, el lienzo "
                       "y las lineas en tramos rectos- en\n\n%1\n\nSolo se tocan esos "
                       "atributos y las <ruta>; lo demas queda como esta.").arg(ruta);
        if (copia)
            t += tr("\n\nOjo: parece la copia que hace 'make datos' en build/, y el original "
                    "esta en src/. La proxima vez que cambie el original, make la pisara.");
        if (QMessageBox::question(this, tr("Guardar en el XML"), t) != QMessageBox::Yes) return;
        QString e;
        if (!escribe_xml(ruta, &e)) QMessageBox::warning(this, tr("Guardar en el XML"), e);
    });
    connect(ilus_, &VistaIlustracion::guarda_disposicion, this,
            [this](const QString& clave, const QJsonObject& d) {
                if (d.isEmpty()) cfg_.disposiciones.remove(clave);
                else cfg_.disposiciones.insert(clave, d);
                if (cfg_.ruta.isEmpty()) return;
                QString e;
                statusBar()->showMessage(cfg_.guarda(e)
                                             ? tr("Disposicion de la ilustracion guardada en %1")
                                                   .arg(cfg_.ruta)
                                             : tr("No se pudo guardar la disposicion: %1").arg(e));
            });
    ventana_ilus_->layout()->addWidget(ilus_);
    {
        QStringList nombres;
        for (const QString& id : ilus_->placas()) nombres << ilus_->nombre_de(id);
        nombres.removeAll(QString());
        nombres.removeDuplicates();
        ventana_ilus_->setWindowTitle(
            nombres.isEmpty() ? tr("Ilustracion")
                              : tr("Ilustracion — %1").arg(nombres.join(QStringLiteral(" + "))));
    }
    connect(ilus_, &VistaIlustracion::pide_dibujo, this,
            [this](const QString& id) { elige_dibujo(id); });
    // Las órdenes del dibujo salen por el mismo sitio que las del panel
    connect(ilus_, &VistaIlustracion::orden, this, &VentanaPrincipal::ordena);
    act_dibujo_->setEnabled(true);
    act_ver_ilus_->setEnabled(true);
    for (const QString& id : ilus_->placas()) {
        const auto d = dibujos_.constFind(ilus_->nombre_de(id));
        if (d != dibujos_.constEnd() && ilus_->origen(id) != QLatin1String("svg"))
            abre_dibujo(id, *d);
    }
    // Con un dibujo de verdad, la ventana se abre sola (`pon_dibujos`); sin
    // él, se queda como estuviera: abierta, con el generado, si se abrió
    if (ilus_->hay_dibujo()) act_ver_ilus_->setChecked(true);
}

QString VentanaPrincipal::ruta_xml() const
{
    if (!conecto_hijo_) return QString();
    const QString placa = ses_.hola().value(QStringLiteral("placa"));
    if (placa.isEmpty()) return QString();
    const QFileInfo f(QDir(cfg_.directorio_absoluto()), placa);
    return f.exists() ? f.absoluteFilePath() : QString();
}

bool VentanaPrincipal::escribe_xml(const QString& ruta, QString* error)
{
    QString e;
    auto falla = [&](const QString& t) {
        if (error) *error = t;
        statusBar()->showMessage(t);
        return false;
    };
    if (!ilus_) return falla(tr("no hay ilustracion"));
    QFile f(ruta);
    if (!f.open(QIODevice::ReadOnly)) return falla(tr("no se puede leer %1: %2").arg(ruta, f.errorString()));
    QByteArray xml = f.readAll();
    f.close();
    if (!escribe_disposicion(xml, ilus_->para_xml(), e))
        return falla(tr("%1 no se ha tocado: %2").arg(ruta, e));
    QSaveFile s(ruta);
    if (!s.open(QIODevice::WriteOnly) || s.write(xml) != xml.size() || !s.commit())
        return falla(tr("no se puede escribir %1: %2").arg(ruta, s.errorString()));
    // Lo de la configuración ya está en el XML: fuera, que la próxima vez no
    // se ponga encima
    ilus_->xml_guardado();
    if (cfg_.disposiciones.contains(ilus_->clave_disposicion())) {
        cfg_.disposiciones.remove(ilus_->clave_disposicion());
        if (!cfg_.ruta.isEmpty()) cfg_.guarda(e);
    }
    statusBar()->showMessage(tr("Disposicion escrita en %1: la proxima vez sale de ahi").arg(ruta));
    return true;
}

bool VentanaPrincipal::abre_dibujo(const QString& placa_id, const QByteArray& svg,
                                   QString* error)
{
    QString e;
    if (!ilus_) e = tr("todavia no hay placa");
    else if (!ilus_->placas().contains(placa_id)) e = tr("no hay ninguna placa \"%1\"").arg(placa_id);
    if (!e.isEmpty()) {
        if (error) *error = e;
        return false;
    }
    // La placa pedida, y las que se llamen como ella: el mismo módulo dos
    // veces en una pila es el mismo dibujo
    const QString nombre = ilus_->nombre_de(placa_id);
    QStringList cuales{placa_id};
    for (const QString& id : ilus_->placas())
        if (id != placa_id && !nombre.isEmpty() && ilus_->nombre_de(id) == nombre) cuales << id;
    if (!pon_dibujos(cuales, svg, QString(), error)) return false;
    if (!nombre.isEmpty()) dibujos_.insert(nombre, svg);
    return true;
}

// Pone un dibujo en esas placas, cada una con SU tabla de enlaces -la que
// llegó en T_PLACA-, y dice en los avisos lo que ha encontrado en cada una.
bool VentanaPrincipal::pon_dibujos(const QStringList& cuales, const QByteArray& svg,
                                   const QString& fichero, QString* error)
{
    QString e;
    for (const QString& id : cuales) {
        QString quien = id.isEmpty() ? tr("dibujo") : tr("dibujo de %1").arg(id);
        if (!fichero.isEmpty()) quien += QStringLiteral(" (%1)").arg(fichero);
        if (!ilus_->pon_dibujo(id, svg, ses_.placa().tabla_de(id), e)) {
            pon_aviso(proto::N_AVISO, 0, quien, tr("no sirve: %1").arg(e));
            if (error) *error = e;
            return false;
        }
        const InformeDibujo& inf = ilus_->informe(id);
        const QStringList det = inf.detalle();
        pon_aviso(det.isEmpty() ? proto::N_INFO : proto::N_AVISO, 0, quien, inf.resumen());
        for (const QString& l : det) pon_aviso(proto::N_INFO, 0, quien, l);
    }
    act_ver_ilus_->setChecked(true);
    if (ventana_ilus_->isVisible()) ventana_ilus_->raise();
    // Un dibujo que llega con el modelo ya esperando o corriendo necesita sus
    // observables: se vuelve a suscribir, y vale la última suscripción
    suscribe();
    return true;
}

// Un dibujo que manda el modelo (T_ILUSTRACION): el de la placa, el que la
// acompaña. Manda sobre uno abierto a mano y recordado.
void VentanaPrincipal::llega_dibujo(int i)
{
    if (!ilus_ || i < 0 || i >= ses_.ilustraciones().size()) return;
    const Sesion::Ilustracion& il = ses_.ilustraciones()[i];
    QStringList cuales;
    for (const QString& id : il.placas) {
        if (ilus_->placas().contains(id)) cuales << id;
        else
            pon_aviso(proto::N_AVISO, 0, tr("dibujo"),
                      tr("%1 es de la placa \"%2\", y no hay ninguna que se llame asi")
                          .arg(il.fichero, id));
    }
    if (!cuales.isEmpty()) pon_dibujos(cuales, il.svg, il.fichero);
}

// Lo que pinta el panel y, detrás, lo que necesita la ilustración que el
// panel no pinta -la corriente de un LED, para su brillo-. Antes de T_LISTO
// no se manda nada: `Sesion::suscribe` lo rechaza, y T_LISTO lo vuelve a
// pedir.
void VentanaPrincipal::suscribe()
{
    QVector<quint16> ids;
    if (panel_) ids = panel_->pintados();
    if (ilus_)
        for (quint16 id : ilus_->observados())
            if (!ids.contains(id)) ids.push_back(id);
    if (!ids.isEmpty()) ses_.suscribe(quint64(cfg_.periodo_ms * 1e6 + 0.5), ids);
}

void VentanaPrincipal::elige_dibujo(const QString& placa_id)
{
    if (!ilus_) {
        statusBar()->showMessage(tr("todavia no hay placa: el dibujo es de una placa"));
        return;
    }
    QString id = placa_id;
    const QStringList ids = ilus_->placas();
    if (id.isNull() && ids.size() == 1) {
        id = ids.first();
    } else if (id.isNull()) {
        QStringList nombres;
        for (const QString& x : ids) nombres << QStringLiteral("%1 · %2").arg(x, ilus_->nombre_de(x));
        bool ok = false;
        const QString elegida = QInputDialog::getItem(this, tr("Abrir dibujo"),
                                                      tr("De que placa es el dibujo:"),
                                                      nombres, 0, false, &ok);
        if (!ok) return;
        id = ids.value(int(nombres.indexOf(elegida)));
    }
    const QString f = QFileDialog::getOpenFileName(
        this, tr("Dibujo de %1").arg(ilus_->nombre_de(id)), QString(),
        tr("Dibujos SVG (*.svg);;Todos los ficheros (*)"));
    if (f.isEmpty()) return;
    QFile fichero(f);
    if (QFileInfo(f).size() > DibujoPlaca::TAMANO_MAX) {
        statusBar()->showMessage(tr("%1 es demasiado grande para ser un dibujo").arg(f));
        return;
    }
    if (!fichero.open(QIODevice::ReadOnly)) {
        statusBar()->showMessage(tr("no se puede leer %1: %2").arg(f, fichero.errorString()));
        return;
    }
    QString e;
    if (abre_dibujo(id, fichero.readAll(), &e))
        statusBar()->showMessage(tr("dibujo %1").arg(QFileInfo(f).fileName()));
    else
        statusBar()->showMessage(tr("%1 no sirve como dibujo: %2").arg(QFileInfo(f).fileName(), e));
}

void VentanaPrincipal::ordena(quint16 pieza, quint16 mando, float v)
{
    if (!ses_.ordena(pieza, mando, v))
        statusBar()->showMessage(tr("no se puede ordenar: el modelo no esta esperando "
                                    "ni corriendo"));
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
