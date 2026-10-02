#include "dialogo_lanzamiento.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <functional>

#include "lanzador.h"

namespace mcusim {

namespace {

// Un campo de texto con un botón «…» que abre un selector de fichero (o de
// directorio, si `filtro` es nulo).
QWidget* con_examinar(QLineEdit* e, QWidget* padre, const QString& filtro,
                      std::function<QString()> base, bool directorio = false)
{
    auto* w = new QWidget(padre);
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->addWidget(e, 1);
    auto* b = new QPushButton(QStringLiteral("…"), w);
    b->setMaximumWidth(32);
    h->addWidget(b);
    QObject::connect(b, &QPushButton::clicked, w, [=] {
        const QString ini = base();
        const QString r = directorio
            ? QFileDialog::getExistingDirectory(w, QString(), ini)
            : QFileDialog::getOpenFileName(w, QString(), ini, filtro);
        if (r.isEmpty()) return;
        // Relativa a la base si cuelga de ella: así la configuración viaja
        const QDir d(ini);
        const QString rel = d.relativeFilePath(r);
        e->setText(rel.startsWith(QLatin1String("..")) ? r : rel);
        emit e->editingFinished();
    });
    return w;
}

} // namespace

DialogoLanzamiento::DialogoLanzamiento(const Configuracion& c, const QString& destino_gui,
                                       const ArgumentosCli* ya, QWidget* padre)
    : QDialog(padre), cfg_(c), destino_(destino_gui)
{
    setWindowTitle(tr("Lanzar mcu-sim"));
    resize(640, 560);
    auto* caja = new QVBoxLayout(this);

    auto* arriba = new QFormLayout;
    exe_ = new QLineEdit(c.ejecutable, this);
    exe_->setObjectName(QStringLiteral("arg:ejecutable"));
    exe_->setPlaceholderText(tr("ruta a mcu-sim"));
    dir_ = new QLineEdit(c.directorio, this);
    dir_->setObjectName(QStringLiteral("arg:directorio"));
    dir_->setPlaceholderText(tr("desde donde se lanza: la placa y el firmware se buscan aqui"));
    auto base_cfg = [this] { return QFileInfo(cfg_.ruta).absolutePath(); };
    arriba->addRow(tr("Ejecutable"), con_examinar(exe_, this, QString(), base_cfg));
    arriba->addRow(tr("Directorio"), con_examinar(dir_, this, QString(), base_cfg, true));
    caja->addLayout(arriba);

    estado_ = new QLabel(this);
    estado_->setObjectName(QStringLiteral("estado"));
    estado_->setWordWrap(true);
    caja->addWidget(estado_);

    auto* sc = new QScrollArea(this);
    sc->setWidgetResizable(true);
    campos_ = new QWidget(sc);
    forma_ = new QFormLayout(campos_);
    sc->setWidget(campos_);
    caja->addWidget(sc, 1);

    a_mano_ = new QLineEdit(c.a_mano, this);
    a_mano_->setObjectName(QStringLiteral("arg:a_mano"));
    a_mano_->setPlaceholderText(tr("cualquier otro argumento, tal cual: --serie VCP=tcp:4000 ..."));
    auto* fm = new QFormLayout;
    fm->addRow(tr("A mano"), a_mano_);
    caja->addLayout(fm);

    orden_ = new QLabel(this);
    orden_->setObjectName(QStringLiteral("orden"));
    orden_->setWordWrap(true);
    orden_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    caja->addWidget(orden_);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    lanzar_ = bb->addButton(tr("Lanzar"), QDialogButtonBox::AcceptRole);
    lanzar_->setObjectName(QStringLiteral("lanzar"));
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    caja->addWidget(bb);

    connect(exe_, &QLineEdit::editingFinished, this, &DialogoLanzamiento::relee);
    connect(dir_, &QLineEdit::editingFinished, this, &DialogoLanzamiento::actualiza);
    connect(a_mano_, &QLineEdit::textChanged, this, &DialogoLanzamiento::actualiza);

    if (ya) {
        args_ = *ya;
        construye();
    } else {
        relee();
    }
}

void DialogoLanzamiento::relee()
{
    // Lo que hubiera en los campos se conserva al reconstruirlos
    if (!w_.isEmpty()) cfg_.argumentos = configuracion().argumentos;
    cfg_.ejecutable = exe_->text().trimmed();
    QString e;
    const QByteArray s = Lanzador::argumentos_de(cfg_.absoluta(cfg_.ejecutable),
                                                 cfg_.absoluta(dir_->text().trimmed()), e);
    args_ = ArgumentosCli();
    aviso_.clear();
    if (s.isEmpty() || !lee_argumentos(s, args_, e)) {
        args_ = ArgumentosCli();
        aviso_ = e;
    }
    construye();
}

void DialogoLanzamiento::construye()
{
    while (forma_->rowCount() > 0) forma_->removeRow(0);
    w_.clear();
    const QString dir = cfg_.absoluta(dir_->text().trimmed());

    // Sin lista, los dos posicionales que todo mcu-sim ha tenido siempre
    QVector<PosicionalCli> pos = args_.posicionales;
    if (pos.isEmpty()) {
        pos = {PosicionalCli{QStringLiteral("placa"), QStringLiteral("fichero"),
                             QStringLiteral("*.xml"), tr("la placa, en XML"), true},
               PosicionalCli{QStringLiteral("firmware"), QStringLiteral("fichero"),
                             QStringLiteral("*.bin"), tr("el firmware"), false}};
    }
    for (const PosicionalCli& p : pos) {
        auto* e = new QLineEdit(cfg_.argumentos.value(p.nombre), campos_);
        e->setObjectName(QStringLiteral("arg:") + p.nombre);
        e->setToolTip(p.ayuda);
        e->setPlaceholderText(p.obligatorio ? tr("obligatorio") : tr("opcional"));
        connect(e, &QLineEdit::textChanged, this, &DialogoLanzamiento::actualiza);
        w_.insert(p.nombre, e);
        const QString filtro = p.filtro.isEmpty() ? QString()
                                                  : QStringLiteral("%1 (%2);;* (*)").arg(p.nombre, p.filtro);
        forma_->addRow(p.nombre, con_examinar(e, campos_, filtro, [this] {
                           return cfg_.absoluta(dir_->text().trimmed());
                       }));
    }

    for (const OpcionCli& o : args_.opciones) {
        if (!o.se_ofrece()) continue;
        const QString val = cfg_.argumentos.value(o.nombre);
        QWidget* w = nullptr;
        QString etiqueta = o.nombre;
        if (o.forma == QLatin1String("bandera")) {
            auto* c = new QCheckBox(o.nombre, campos_);
            c->setChecked(val == QLatin1String("si"));
            connect(c, &QCheckBox::toggled, this, [this, c, o](bool si) {
                // Las del mismo grupo se excluyen: marcar una desmarca las otras
                if (si && !o.grupo.isEmpty())
                    for (const OpcionCli& x : args_.opciones)
                        if (x.grupo == o.grupo && x.nombre != o.nombre)
                            if (auto* y = qobject_cast<QCheckBox*>(w_.value(x.nombre)))
                                y->setChecked(false);
                actualiza();
            });
            etiqueta.clear();
            w = c;
        } else if (o.tipo == QLatin1String("eleccion")) {
            auto* c = new QComboBox(campos_);
            c->addItem(o.omision.isEmpty() ? tr("(sin decir)")
                                           : tr("(sin decir: %1)").arg(o.omision),
                       QString());
            for (const QString& v : o.valores) c->addItem(v, v);
            const int i = c->findData(val);
            c->setCurrentIndex(i < 0 ? 0 : i);
            connect(c, &QComboBox::currentIndexChanged, this, &DialogoLanzamiento::actualiza);
            w = c;
        } else {
            auto* e = new QLineEdit(val, campos_);
            QString ph = o.omision.isEmpty() ? QString() : tr("sin decir: %1").arg(o.omision);
            if (!o.unidad.isEmpty() && !ph.isEmpty()) ph += QLatin1Char(' ') + o.unidad;
            if (o.repetible)
                ph = tr("una o varias, separadas por ';'") +
                     (o.ejemplo.isEmpty() ? QString() : tr(": %1").arg(o.ejemplo));
            e->setPlaceholderText(ph);
            if (o.tipo == QLatin1String("entero")) e->setValidator(new QIntValidator(0, 65535, e));
            if (o.tipo == QLatin1String("numero")) {
                auto* v = new QDoubleValidator(0, 1e12, 6, e);
                v->setLocale(QLocale::c());
                e->setValidator(v);
            }
            connect(e, &QLineEdit::textChanged, this, &DialogoLanzamiento::actualiza);
            if (o.tipo == QLatin1String("fichero"))
                w = con_examinar(e, campos_, QString(), [this] {
                    return cfg_.absoluta(dir_->text().trimmed());
                });
            else
                w = e;
            w_.insert(o.nombre, e);
        }
        if (!w_.contains(o.nombre)) w_.insert(o.nombre, w);
        // El nombre y la ayuda, en el campo que tiene el valor
        w_.value(o.nombre)->setObjectName(QStringLiteral("arg:") + o.nombre);
        w_.value(o.nombre)->setToolTip(o.ayuda);
        if (o.unidad.isEmpty() || etiqueta.isEmpty()) forma_->addRow(etiqueta, w);
        else forma_->addRow(QStringLiteral("%1 (%2)").arg(etiqueta, o.unidad), w);
    }

    if (args_.vacio() || args_.opciones.isEmpty())
        estado_->setText(tr("<b>No se pueden leer las opciones de mcu-sim</b>: %1. Se puede "
                            "lanzar igual con la placa, el firmware y lo que se escriba a mano.")
                             .arg(aviso_.toHtmlEscaped()));
    else
        estado_->setText(tr("Opciones leidas del propio %1 (version %2) con --argumentos.")
                             .arg(args_.programa, args_.version));
    actualiza();
}

QString DialogoLanzamiento::valor(const QString& nombre) const
{
    QWidget* w = w_.value(nombre);
    if (auto* e = qobject_cast<QLineEdit*>(w)) return e->text().trimmed();
    if (auto* c = qobject_cast<QCheckBox*>(w)) return c->isChecked() ? QStringLiteral("si") : QString();
    if (auto* c = qobject_cast<QComboBox*>(w)) return c->currentData().toString();
    return {};
}

Configuracion DialogoLanzamiento::configuracion() const
{
    Configuracion c = cfg_;
    c.ejecutable = exe_->text().trimmed();
    c.directorio = dir_->text().trimmed();
    c.a_mano = a_mano_->text().trimmed();
    c.argumentos.clear();
    for (auto it = w_.begin(); it != w_.end(); ++it) {
        const QString v = valor(it.key());
        if (!v.isEmpty()) c.argumentos.insert(it.key(), v);
    }
    return c;
}

QStringList DialogoLanzamiento::linea(QString* error) const
{
    const Configuracion c = configuracion();
    return linea_de_ordenes(args_, c.argumentos, c.a_mano, error);
}

QString DialogoLanzamiento::aviso() const { return aviso_; }

void DialogoLanzamiento::actualiza()
{
    QString e;
    const QStringList l = linea(&e);
    const Configuracion c = configuracion();
    if (!e.isEmpty()) {
        orden_->setText(tr("<i>%1</i>").arg(e.toHtmlEscaped()));
        lanzar_->setEnabled(false);
        return;
    }
    QStringList todo = l;
    todo << QStringLiteral("--gui") << destino_;
    orden_->setText(tr("<code>%1</code>")
                        .arg(como_texto(c.ejecutable.isEmpty() ? QStringLiteral("mcu-sim")
                                                               : c.ejecutable,
                                        todo)
                                 .toHtmlEscaped()));
    lanzar_->setEnabled(!c.ejecutable.isEmpty());
}

} // namespace mcusim
