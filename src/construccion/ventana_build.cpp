#include "ventana_build.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QStatusBar>
#include <QToolBar>
#include <QUndoCommand>
#include <QUndoStack>

#include "abre.h"
#include "biblioteca.h"
#include "catalogo.h"
#include "exporta_placa.h"
#include "importa.h"
#include "lienzo.h"
#include "panel_biblioteca.h"
#include "panel_propiedades.h"
#include "redes.h"
#include "valida.h"

namespace mcusim::build {

// --- La configuración --------------------------------------------------------

QString ConfigBuild::ruta_por_omision()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
           QStringLiteral("/mcu-sim-build.json");
}

ConfigBuild ConfigBuild::lee(const QString& r)
{
    ConfigBuild c;
    c.ruta = r.isEmpty() ? ruta_por_omision() : r;
    QFile f(c.ruta);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        c.mcu_sim = o.value(QLatin1String("mcu_sim")).toString();
        c.ultima_carpeta = o.value(QLatin1String("ultima_carpeta")).toString();
    }
    if (c.mcu_sim.isEmpty()) c.mcu_sim = qEnvironmentVariable("MCU_SIM");
    return c;
}

bool ConfigBuild::guarda() const
{
    QDir().mkpath(QFileInfo(ruta).path());
    QFile f(ruta);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QJsonObject o;
    o.insert(QLatin1String("mcu_sim"), mcu_sim);
    o.insert(QLatin1String("ultima_carpeta"), ultima_carpeta);
    f.write(QJsonDocument(o).toJson());
    return true;
}

// --- Deshacer: el documento entero ------------------------------------------

namespace {
class Instantanea : public QUndoCommand {
public:
    Instantanea(std::function<void(const Documento&)> pon, Documento antes, Documento despues, const QString& t)
        : QUndoCommand(t), pon_(std::move(pon)), antes_(std::move(antes)), despues_(std::move(despues)) {}
    void undo() override { pon_(antes_); }
    void redo() override { pon_(despues_); }
private:
    std::function<void(const Documento&)> pon_;
    Documento antes_, despues_;
};
} // namespace

// --- La ventana --------------------------------------------------------------

VentanaBuild::VentanaBuild(const QString& datos, const QString& config, QWidget* padre)
    : QMainWindow(padre), cfg_(ConfigBuild::lee(config))
{
    const QString d = datos.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) : datos;
    bib_ = new Biblioteca(d);
    cat_ = new Catalogo(bib_);
    pila_ = new QUndoStack(this);
    lienzo_ = new Lienzo(cat_, this);
    setCentralWidget(lienzo_);

    panel_bib_ = new PanelBiblioteca(bib_, this);
    auto* db = new QDockWidget(QStringLiteral("Biblioteca"), this);
    db->setObjectName(QStringLiteral("biblioteca"));
    db->setWidget(panel_bib_);
    addDockWidget(Qt::LeftDockWidgetArea, db);
    panel_prop_ = new PanelPropiedades(lienzo_, this);
    auto* dp = new QDockWidget(QStringLiteral("Propiedades"), this);
    dp->setObjectName(QStringLiteral("propiedades"));
    dp->setWidget(panel_prop_);
    addDockWidget(Qt::RightDockWidgetArea, dp);
    log_ = new QPlainTextEdit(this);
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(2000);
    auto* dl = new QDockWidget(QStringLiteral("Mensajes"), this);
    dl->setObjectName(QStringLiteral("mensajes"));
    dl->setWidget(log_);
    addDockWidget(Qt::BottomDockWidgetArea, dl);

    // Las acciones
    auto* m_arch = menuBar()->addMenu(QStringLiteral("&Archivo"));
    auto* a_nuevo = m_arch->addAction(QStringLiteral("&Nueva placa"), this, [this] {
        if (puede_cerrar()) nuevo();
    });
    a_nuevo->setShortcut(QKeySequence::New);
    auto* a_abrir = m_arch->addAction(QStringLiteral("&Abrir…"), this, &VentanaBuild::abre_dialogo);
    a_abrir->setShortcut(QKeySequence::Open);
    auto* a_exp = m_arch->addAction(QStringLiteral("&Exportar"), this, [this] { exporta_dialogo(false); });
    a_exp->setShortcut(QKeySequence::Save);
    a_exp->setToolTip(QStringLiteral("Escribe el XML de la placa y su SVG (valida antes con mcu-sim)"));
    auto* a_como = m_arch->addAction(QStringLiteral("Exportar &como…"), this, [this] { exporta_dialogo(true); });
    a_como->setShortcut(QKeySequence::SaveAs);
    m_arch->addSeparator();
    m_arch->addAction(QStringLiteral("A&justes…"), this, &VentanaBuild::ajustes);
    m_arch->addSeparator();
    auto* a_salir = m_arch->addAction(QStringLiteral("&Salir"), this, &QWidget::close);
    a_salir->setShortcut(QKeySequence::Quit);

    auto* m_ed = menuBar()->addMenu(QStringLiteral("&Editar"));
    QAction* a_des = pila_->createUndoAction(this, QStringLiteral("Deshacer"));
    a_des->setShortcut(QKeySequence::Undo);
    QAction* a_reh = pila_->createRedoAction(this, QStringLiteral("Rehacer"));
    a_reh->setShortcut(QKeySequence::Redo);
    m_ed->addAction(a_des);
    m_ed->addAction(a_reh);
    m_ed->addSeparator();
    a_girar_ = m_ed->addAction(QStringLiteral("&Girar 90°"), this, [this] { lienzo_->gira(90); });
    a_girar_->setShortcut(Qt::Key_R);
    a_borrar_ = m_ed->addAction(QStringLiteral("&Borrar"), this, [this] { lienzo_->borra(); });
    a_borrar_->setShortcuts({QKeySequence::Delete, Qt::Key_Backspace});

    auto* m_con = menuBar()->addMenu(QStringLiteral("&Conexiones"));
    a_unir_curva_ = m_con->addAction(QStringLiteral("Unir con &curva"), this, [this] { lienzo_->une(false); });
    a_unir_curva_->setShortcut(QStringLiteral("Ctrl+U"));
    a_unir_tramos_ = m_con->addAction(QStringLiteral("Unir en &tramos"), this, [this] { lienzo_->une(true); });
    a_unir_tramos_->setShortcut(QStringLiteral("Ctrl+T"));
    m_con->addSeparator();
    a_trazado_ = m_con->addAction(QStringLiteral("Curva ↔ tramos"), this, [this] { lienzo_->alterna_trazado(); });
    a_trazado_->setShortcut(QStringLiteral("Ctrl+L"));
    a_visible_ = m_con->addAction(QStringLiteral("&Ocultar / mostrar"), this, [this] { lienzo_->alterna_visible(); });
    a_visible_->setShortcut(QStringLiteral("Ctrl+H"));
    a_ocultas_ = m_con->addAction(QStringLiteral("Ver las ocultas"));
    a_ocultas_->setCheckable(true);
    a_ocultas_->setChecked(true);
    connect(a_ocultas_, &QAction::toggled, lienzo_, &Lienzo::mostrar_ocultas);

    auto* m_placa = menuBar()->addMenu(QStringLiteral("&Placa"));
    auto* a_val = m_placa->addAction(QStringLiteral("&Validar con mcu-sim"), this, &VentanaBuild::valida_ahora);
    a_val->setShortcut(QStringLiteral("F5"));
    auto* a_ver = m_placa->addAction(QStringLiteral("&Ver toda"), lienzo_, &Lienzo::ajusta_vista);
    a_ver->setShortcut(QStringLiteral("Ctrl+0"));

    auto* tb = addToolBar(QStringLiteral("Placa"));
    tb->setObjectName(QStringLiteral("barra"));
    tb->addAction(a_nuevo);
    tb->addAction(a_abrir);
    tb->addAction(a_exp);
    tb->addSeparator();
    tb->addAction(a_des);
    tb->addAction(a_reh);
    tb->addSeparator();
    tb->addAction(a_unir_curva_);
    tb->addAction(a_unir_tramos_);
    tb->addAction(a_trazado_);
    tb->addAction(a_visible_);
    tb->addAction(a_girar_);
    tb->addAction(a_borrar_);
    tb->addSeparator();
    tb->addAction(a_val);
    tb->addAction(a_ver);

    connect(lienzo_, &Lienzo::cambiado, this, &VentanaBuild::cambiado);
    connect(lienzo_, &Lienzo::mensaje, this, [this](const QString& t) { dice(t); });
    connect(lienzo_, &Lienzo::seleccion_cambiada, this, &VentanaBuild::actualiza_acciones);
    connect(panel_bib_, &PanelBiblioteca::mensaje, this, [this](const QString& t) { dice(t); });
    connect(panel_bib_, &PanelBiblioteca::refrescada, this, [this] {
        cat_->olvida();
        lienzo_->pon_documento(lienzo_->documento());
    });
    connect(panel_bib_, &PanelBiblioteca::pedida, this, [this](const Entrada& e) {
        lienzo_->coloca(e, lienzo_->mapToScene(lienzo_->viewport()->rect().center()));
    });
    connect(pila_, &QUndoStack::cleanChanged, this, &VentanaBuild::actualiza_titulo);

    // La biblioteca: del índice, y si no hay carpetas, las de mcu-sim
    QApplication::setOverrideCursor(Qt::WaitCursor);
    bib_->abre();
    QApplication::restoreOverrideCursor();
    carpetas_iniciales();
    panel_bib_->rellena();
    for (const QString& e : bib_->errores()) dice(e, true);

    resize(1400, 880);
    nuevo();
}

VentanaBuild::~VentanaBuild()
{
    // Primero lo que mira al lienzo y a la biblioteca, luego ellos; la pila
    // avisa de que queda limpia al irse
    pila_->disconnect(this);
    delete pila_;
    delete panel_prop_;
    delete panel_bib_;
    delete lienzo_;
    lienzo_ = nullptr;
    delete cat_;
    delete bib_;
}

void VentanaBuild::carpetas_iniciales()
{
    if (!bib_->carpetas().isEmpty()) return;
    // MCU_SIM_SRC, o el src/ del ejecutable de mcu-sim (src/build/mcu-sim)
    QStringList srcs;
    if (!qEnvironmentVariable("MCU_SIM_SRC").isEmpty()) srcs << qEnvironmentVariable("MCU_SIM_SRC");
    if (!cfg_.mcu_sim.isEmpty()) srcs << QFileInfo(cfg_.mcu_sim).absoluteDir().filePath(QStringLiteral(".."));
    for (const QString& s : srcs) {
        const QString b = QDir(s).filePath(QStringLiteral("biblioteca"));
        if (!QFileInfo(b + QStringLiteral("/piezas")).isDir()) continue;
        bib_->anade_carpeta(QDir::cleanPath(b));
        const QString p = QDir(s).filePath(QStringLiteral("placas"));
        if (QFileInfo(p).isDir()) bib_->anade_carpeta(QDir::cleanPath(p));
        QApplication::setOverrideCursor(Qt::WaitCursor);
        bib_->refresca(true);
        QApplication::restoreOverrideCursor();
        dice(QStringLiteral("biblioteca: importada la de mcu-sim, %1").arg(QDir::cleanPath(b)));
        return;
    }
    dice(QStringLiteral("la biblioteca está vacía: «Carpetas…» para importar la de mcu-sim (src/biblioteca) y sus placas (src/placas)"));
}

const Documento& VentanaBuild::documento() const { return lienzo_->documento(); }

QString VentanaBuild::mensajes() const { return log_->toPlainText(); }

void VentanaBuild::dice(const QString& t, bool error)
{
    log_->appendPlainText((error ? QStringLiteral("✗ ") : QString()) + t);
    statusBar()->showMessage(t, 6000);
}

void VentanaBuild::pon(const Documento& d)
{
    poniendo_ = true;
    lienzo_->pon_documento(d);
    poniendo_ = false;
    informa_redes();
    actualiza_titulo();
}

void VentanaBuild::cambiado(const Documento& d, const QString& que)
{
    if (poniendo_) return;
    pila_->push(new Instantanea([this](const Documento& x) { pon(x); }, lienzo_->documento(), d, que));
}

void VentanaBuild::informa_redes()
{
    const Redes& r = lienzo_->redes();
    static QString ultimo;
    QStringList ls;
    for (const QString& e : r.errores) ls << QStringLiteral("✗ ") + e;
    for (const QString& a : r.avisos) ls << QStringLiteral("⚠ ") + a;
    if (!r.sin_conectar.isEmpty())
        ls << QStringLiteral("⚠ sin conectar (en rojo): %1").arg(r.sin_conectar.join(QStringLiteral(", ")));
    const QString t = ls.join(QLatin1Char('\n'));
    if (t != ultimo && !t.isEmpty()) log_->appendPlainText(t);
    ultimo = t;
}

void VentanaBuild::actualiza_titulo()
{
    const QString n = ruta_.isEmpty() ? lienzo_->documento().nombre : QFileInfo(ruta_).fileName();
    setWindowTitle(QStringLiteral("%1%2 — mcu-sim-build").arg(n, pila_->isClean() ? QString() : QStringLiteral(" *")));
}

void VentanaBuild::actualiza_acciones()
{
    const bool piezas = !lienzo_->instancias_elegidas().isEmpty();
    const bool lineas = !lienzo_->conexiones_elegidas().isEmpty();
    a_girar_->setEnabled(piezas);
    a_borrar_->setEnabled(piezas || lineas);
    a_trazado_->setEnabled(lineas);
    a_visible_->setEnabled(lineas);
    const bool dos = lienzo_->terminales_elegidos().size() >= 2;
    a_unir_curva_->setEnabled(dos);
    a_unir_tramos_->setEnabled(dos);
}

void VentanaBuild::nuevo()
{
    ruta_.clear();
    pila_->clear();
    lienzo_->olvida_terminales();
    pon(Documento());
    pila_->setClean();
    lienzo_->pon_zoom(6);
    lienzo_->centerOn(50, 40);
    actualiza_acciones();
}

bool VentanaBuild::puede_cerrar()
{
    if (pila_->isClean()) return true;
    const auto r = QMessageBox::question(this, QStringLiteral("mcu-sim-build"),
                                         QStringLiteral("La placa tiene cambios sin exportar. ¿Se exporta?"),
                                         QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (r == QMessageBox::Cancel) return false;
    if (r == QMessageBox::Save) {
        exporta_dialogo(false);
        return pila_->isClean();
    }
    return true;
}

void VentanaBuild::closeEvent(QCloseEvent* e)
{
    if (!puede_cerrar()) {
        e->ignore();
        return;
    }
    cfg_.guarda();
    bib_->guarda();
    e->accept();
}

bool VentanaBuild::abre(const QString& ruta, QString* error)
{
    QFile f(ruta);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    const QString t = QString::fromUtf8(f.readAll());
    QString e;
    Documento d;
    if (tiene_build(t)) {
        d = abre_build(t, *cat_, &e);
    } else {
        // Una placa que no salió de aquí: se reconstruye en rejilla
        const PlacaLeida p = lee_placa(t, &e);
        if (e.isEmpty()) d = reconstruye(p, *cat_, &e);
        if (!d.instancias.isEmpty())
            dice(QStringLiteral("%1 no se hizo con mcu-sim-build: se reconstruye, con las piezas en rejilla")
                     .arg(QFileInfo(ruta).fileName()));
    }
    if (d.instancias.isEmpty()) {
        if (error) *error = e.isEmpty() ? QStringLiteral("la placa no tiene nada que dibujar") : e;
        return false;
    }
    pila_->clear();
    lienzo_->olvida_terminales();
    pon(d);
    // Una reconstruida no se ha exportado: se queda con cambios y sin ruta
    if (tiene_build(t)) {
        ruta_ = ruta;
        pila_->setClean();
    } else {
        ruta_.clear();
        pila_->resetClean();
    }
    actualiza_titulo();
    lienzo_->ajusta_vista();
    return true;
}

void VentanaBuild::abre_dialogo()
{
    if (!puede_cerrar()) return;
    const QString r = QFileDialog::getOpenFileName(this, QStringLiteral("Abrir una placa"), cfg_.ultima_carpeta,
                                                   QStringLiteral("Placas (*.xml)"));
    if (r.isEmpty()) return;
    cfg_.ultima_carpeta = QFileInfo(r).path();
    QString e;
    if (!abre(r, &e)) QMessageBox::warning(this, QStringLiteral("Abrir"), QStringLiteral("%1:\n%2").arg(r, e));
}

bool VentanaBuild::exporta_a(const QString& ruta, bool pregunta)
{
    const Documento& d = lienzo_->documento();
    if (d.instancias.isEmpty()) {
        dice(QStringLiteral("la placa está vacía"), true);
        return false;
    }
    const Redes& r = lienzo_->redes();
    if (!r.errores.isEmpty()) {
        for (const QString& e : r.errores) dice(e, true);
        if (pregunta)
            QMessageBox::warning(this, QStringLiteral("Exportar"),
                                 QStringLiteral("La placa tiene errores y no se exporta:\n\n%1").arg(r.errores.join(QLatin1Char('\n'))));
        return false;
    }
    if (!cfg_.mcu_sim.isEmpty()) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const Validacion v = valida(cfg_.mcu_sim, d, *cat_);
        QApplication::restoreOverrideCursor();
        if (!v.lanzado) {
            dice(QStringLiteral("no se pudo ejecutar %1: se exporta sin validar").arg(cfg_.mcu_sim), true);
        } else if (!v.valida) {
            dice(QStringLiteral("mcu-sim no la valida:\n%1").arg(v.texto()), true);
            if (!pregunta) return false;
            QMessageBox m(QMessageBox::Warning, QStringLiteral("Exportar"),
                          QStringLiteral("mcu-sim no valida la placa. ¿Se escribe igual?"),
                          QMessageBox::Yes | QMessageBox::No, this);
            m.setDetailedText(v.texto());
            m.setDefaultButton(QMessageBox::No);
            if (m.exec() != QMessageBox::Yes) return false;
        } else {
            dice(QStringLiteral("mcu-sim la valida"));
        }
    } else {
        dice(QStringLiteral("sin mcu-sim en los ajustes: se exporta sin validar"));
    }
    const QString e = exporta(d, *cat_, ruta);
    if (!e.isEmpty()) {
        dice(e, true);
        if (pregunta) QMessageBox::warning(this, QStringLiteral("Exportar"), e);
        return false;
    }
    ruta_ = ruta;
    pila_->setClean();
    actualiza_titulo();
    dice(QStringLiteral("exportada: %1 y su .svg").arg(QDir::toNativeSeparators(ruta)));
    return true;
}

void VentanaBuild::exporta_dialogo(bool como)
{
    QString r = ruta_;
    if (como || r.isEmpty()) {
        const QString propuesta = QDir(cfg_.ultima_carpeta).filePath(lienzo_->documento().nombre + QStringLiteral(".xml"));
        r = QFileDialog::getSaveFileName(this, QStringLiteral("Exportar la placa"), propuesta,
                                         QStringLiteral("Placas (*.xml)"));
        if (r.isEmpty()) return;
        if (!r.endsWith(QLatin1String(".xml"))) r += QStringLiteral(".xml");
        cfg_.ultima_carpeta = QFileInfo(r).path();
    }
    exporta_a(r, true);
}

void VentanaBuild::valida_ahora()
{
    if (cfg_.mcu_sim.isEmpty()) {
        dice(QStringLiteral("para validar hace falta mcu-sim: dilo en Archivo → Ajustes"), true);
        return;
    }
    const Redes& r = lienzo_->redes();
    for (const QString& e : r.errores) dice(e, true);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const Validacion v = valida(cfg_.mcu_sim, lienzo_->documento(), *cat_);
    QApplication::restoreOverrideCursor();
    if (!v.lanzado) dice(QStringLiteral("no se pudo ejecutar %1").arg(cfg_.mcu_sim), true);
    else dice(QStringLiteral("%1\n%2").arg(v.valida ? QStringLiteral("✓ mcu-sim la valida") : QStringLiteral("✗ mcu-sim no la valida"),
                                           v.texto()), !v.valida);
}

void VentanaBuild::ajustes()
{
    QDialog d(this);
    d.setWindowTitle(QStringLiteral("Ajustes"));
    auto* f = new QFormLayout(&d);
    auto* e = new QLineEdit(cfg_.mcu_sim, &d);
    auto* b = new QPushButton(QStringLiteral("…"), &d);
    auto* h = new QHBoxLayout;
    h->addWidget(e);
    h->addWidget(b);
    f->addRow(QStringLiteral("mcu-sim"), h);
    f->addRow(new QLabel(QStringLiteral("<small>El ejecutable de mcu-sim: con él se valida cada placa antes de "
                                        "exportarla (mcu-sim --valida)</small>"), &d));
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &d);
    f->addRow(bb);
    connect(b, &QPushButton::clicked, &d, [&] {
        const QString r = QFileDialog::getOpenFileName(&d, QStringLiteral("mcu-sim"), e->text());
        if (!r.isEmpty()) e->setText(r);
    });
    connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    d.resize(520, 120);
    if (d.exec() != QDialog::Accepted) return;
    cfg_.mcu_sim = e->text().trimmed();
    cfg_.guarda();
}

} // namespace mcusim::build
