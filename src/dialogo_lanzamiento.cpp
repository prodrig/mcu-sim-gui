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
#include <QTimer>
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

    // De dónde sale lo que hay en los campos
    auto* origen = new QLabel(this);
    origen->setObjectName(QStringLiteral("origen"));
    origen->setWordWrap(true);
    origen->setTextInteractionFlags(Qt::TextSelectableByMouse);
    const QString ruta = QDir::toNativeSeparators(c.ruta);
    if (c.ruta.isEmpty()) {
        origen->setText(tr("Sin fichero de configuracion: lo que se ponga aqui no se guarda."));
    } else if (c.existia) {
        origen->setText(tr("Configuracion leida de <code>%1</code>. Al lanzar se guarda ahi.")
                            .arg(ruta.toHtmlEscaped()));
    } else {
        QStringList donde;
        for (const QString& b : c.buscadas)
            donde << QStringLiteral("<code>%1</code>")
                         .arg(QDir::toNativeSeparators(b).toHtmlEscaped());
        if (donde.isEmpty()) donde << QStringLiteral("<code>%1</code>").arg(ruta.toHtmlEscaped());
        origen->setText(
            tr("<b>No hay configuracion</b>, y por eso esto sale vacio: no se ha encontrado %1. "
               "Pon donde esta mcu-sim y el directorio desde el que se lanza -el de sus "
               "placas/ y verif/-; al lanzar se guardara en <code>%2</code> y la proxima vez "
               "saldra relleno.")
                .arg(donde.join(tr(" ni ")), ruta.toHtmlEscaped()));
    }
    caja->addWidget(origen);

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
    // El directorio cambia de dónde se lee la placa: sus MCUs, otra vez
    connect(dir_, &QLineEdit::editingFinished, this, &DialogoLanzamiento::relee_mcus);
    connect(a_mano_, &QLineEdit::textChanged, this, &DialogoLanzamiento::actualiza);

    if (ya) {
        args_ = *ya;
        construye();
    } else {
        relee();
    }
}

// Plan §42: una fila de firmware -el fichero, «…» y la casilla «sin
// firmware»-. Con la casilla, el fichero no cuenta y se apaga
QWidget* DialogoLanzamiento::fila_firmware(const QString& clave, const QString& clave_sin,
                                           const QString& del_xml)
{
    auto* e = new QLineEdit(cfg_.argumentos.value(clave), campos_);
    e->setObjectName(QStringLiteral("arg:") + clave);
    e->setPlaceholderText(del_xml.isEmpty() ? tr("el XML no dice ninguno: sin firmware")
                                            : tr("el del XML: %1").arg(del_xml));
    e->setToolTip(tr("La imagen binaria para la Flash. Vacio, la que diga el XML"));
    connect(e, &QLineEdit::textChanged, this, &DialogoLanzamiento::actualiza);
    auto* c = new QCheckBox(tr("sin firmware"), campos_);
    c->setObjectName(QStringLiteral("arg:") + clave_sin);
    c->setToolTip(tr("Lanzar este MCU sin firmware, aunque el XML diga uno: el nucleo se "
                     "aparca en wfe"));
    c->setChecked(cfg_.argumentos.value(clave_sin) == QLatin1String("si"));
    auto* w = new QWidget(campos_);
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->addWidget(con_examinar(e, w, tr("firmware (*.bin *.hex);;* (*)"),
                              [this] { return cfg_.absoluta(dir_->text().trimmed()); }),
                 1);
    h->addWidget(c);
    e->setEnabled(!c->isChecked());
    connect(c, &QCheckBox::toggled, this, [this, e](bool si) {
        e->setEnabled(!si);
        actualiza();
    });
    w_.insert(clave, e);
    w_.insert(clave_sin, c);
    return w;
}

// `mcu-sim placa --mcus`, con el --mcu que se haya elegido. Sin ejecutable o
// sin placa no hay a quién preguntar; si no sabe contestar, se dice y se
// queda la fila de siempre
bool DialogoLanzamiento::pide_mcus()
{
    mcus_.clear();
    aviso_mcus_.clear();
    const QString exe = exe_->text().trimmed();
    const QString placa = cfg_.argumentos.value(QStringLiteral("placa")).trimmed();
    if (exe.isEmpty() || placa.isEmpty()) return false;
    QStringList extra;
    const QString mcu = cfg_.argumentos.value(QStringLiteral("--mcu"));
    if (!mcu.isEmpty()) extra << QStringLiteral("--mcu=") + mcu;
    QString e;
    const QByteArray s = Lanzador::mcus_de(cfg_.absoluta(exe), cfg_.absoluta(dir_->text().trimmed()),
                                           placa, extra, e);
    if (s.isEmpty() || !lee_mcus(s, mcus_, e)) {
        mcus_.clear();
        aviso_mcus_ = e;
        return false;
    }
    return true;
}

void DialogoLanzamiento::pon_mcus(const QVector<McuCli>& m)
{
    if (!w_.isEmpty()) cfg_.argumentos = configuracion().argumentos;
    mcus_ = m;
    aviso_mcus_.clear();
    construye();
}

void DialogoLanzamiento::relee_mcus()
{
    if (!w_.isEmpty()) cfg_.argumentos = configuracion().argumentos;
    pide_mcus();
    construye();
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
    pide_mcus();
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
        // Plan §42: el firmware, según los chips de la placa
        if (p.nombre == QLatin1String("firmware") && mcus_.size() == 1) {
            const McuCli& m = mcus_.front();
            const QString quien = m.id.isEmpty() ? m.tipo : QStringLiteral("%1 (%2)").arg(m.id, m.tipo);
            QWidget* f = fila_firmware(p.nombre, QStringLiteral("sin-firmware"), m.firmware);
            forma_->addRow(tr("firmware de %1").arg(quien), f);
            continue;
        }
        if (p.nombre == QLatin1String("firmware") && mcus_.size() > 1) {
            auto* t = new QLabel(tr("<b>El firmware de cada MCU</b>: vacio, el que diga el XML"),
                                 campos_);
            t->setObjectName(QStringLiteral("firmwares"));
            forma_->addRow(t);
            for (const McuCli& m : mcus_)
                forma_->addRow(QStringLiteral("%1 (%2)").arg(m.id, m.tipo),
                               fila_firmware(QStringLiteral("firmware:") + m.id,
                                             QStringLiteral("sin-firmware:") + m.id, m.firmware));
            continue;
        }
        auto* e = new QLineEdit(cfg_.argumentos.value(p.nombre), campos_);
        e->setObjectName(QStringLiteral("arg:") + p.nombre);
        e->setToolTip(p.ayuda);
        e->setPlaceholderText(p.obligatorio ? tr("obligatorio") : tr("opcional"));
        connect(e, &QLineEdit::textChanged, this, &DialogoLanzamiento::actualiza);
        // Otra placa puede llevar otros chips: se le preguntan al acabar de
        // escribirla, o al elegirla con «…»
        if (p.nombre == QLatin1String("placa"))
            connect(e, &QLineEdit::editingFinished, this, [this, e] {
                // En diferido: rehacer el diálogo borra este mismo campo
                if (e->text().trimmed() != cfg_.argumentos.value(QStringLiteral("placa")))
                    QTimer::singleShot(0, this, &DialogoLanzamiento::relee_mcus);
            });
        w_.insert(p.nombre, e);
        const QString filtro = p.filtro.isEmpty() ? QString()
                                                  : QStringLiteral("%1 (%2);;* (*)").arg(p.nombre, p.filtro);
        forma_->addRow(p.nombre, con_examinar(e, campos_, filtro, [this] {
                           return cfg_.absoluta(dir_->text().trimmed());
                       }));
    }

    for (const OpcionCli& o : args_.opciones) {
        if (!o.se_ofrece()) continue;
        // Plan §42: esas dos las escriben las filas del firmware
        if (o.nombre == QLatin1String("--firmware") || o.nombre == QLatin1String("--sin-firmware"))
            continue;
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

    if ((args_.vacio() || args_.opciones.isEmpty()) && exe_->text().trimmed().isEmpty())
        estado_->setText(tr("Cuando se diga donde esta mcu-sim, se le pediran sus opciones "
                            "(--argumentos) y saldran aqui."));
    else if (args_.vacio() || args_.opciones.isEmpty())
        estado_->setText(tr("<b>No se pueden leer las opciones de mcu-sim</b>: %1. Se puede "
                            "lanzar igual con la placa, el firmware y lo que se escriba a mano.")
                             .arg(aviso_.toHtmlEscaped()));
    else {
        QString t = tr("Opciones leidas del propio %1 (version %2) con --argumentos.")
                        .arg(args_.programa, args_.version);
        if (!aviso_mcus_.isEmpty())
            t += QLatin1Char(' ') + tr("No se sabe que MCUs lleva la placa (%1): el firmware, "
                                       "como siempre.").arg(aviso_mcus_.toHtmlEscaped());
        estado_->setText(t);
    }
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
    // Plan §42: el firmware de los chips que ahora no están -el posicional con
    // varios, o los de otro sistema- se recuerda: la configuración es de quien
    // la usa, y volverá a la placa de antes
    for (auto it = cfg_.argumentos.begin(); it != cfg_.argumentos.end(); ++it) {
        const bool de_firmware = it.key() == QLatin1String("firmware") ||
                                 it.key() == QLatin1String("sin-firmware") ||
                                 it.key().startsWith(QLatin1String("firmware:")) ||
                                 it.key().startsWith(QLatin1String("sin-firmware:"));
        if (de_firmware && !w_.contains(it.key()) && !it.value().isEmpty())
            c.argumentos.insert(it.key(), it.value());
    }
    return c;
}

QStringList DialogoLanzamiento::linea(QString* error) const
{
    const Configuracion c = configuracion();
    return linea_de_ordenes(args_, c.argumentos, c.a_mano, error, mcus_);
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
