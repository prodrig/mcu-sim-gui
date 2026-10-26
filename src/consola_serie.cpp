#include "consola_serie.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>

namespace mcusim {

namespace {
// En el formato de cada trozo va de qué sentido es: así se le cambia el color
// después, aunque los dos colores sean el mismo
constexpr int SENTIDO = QTextFormat::UserProperty + 1;
constexpr int MAX_PREVIA = 24;           // bytes que se enseñan en hexadecimal

QString hex_de(const QByteArray& b)
{
    QString s;
    for (int i = 0; i < b.size() && i < MAX_PREVIA; ++i)
        s += QStringLiteral("%1 ").arg(quint8(b.at(i)), 2, 16, QLatin1Char('0')).toUpper();
    if (b.size() > MAX_PREVIA) s += QStringLiteral("…");
    return s.trimmed();
}
} // namespace

QIcon ConsolaSerie::icono(const QColor& c, bool enviado)
{
    const int t = 22;
    QPixmap px(t * 2, t * 2);               // a doble resolución
    px.setDevicePixelRatio(2.0);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(0, 0, 0, 90), 1));
    p.setBrush(c);
    p.drawRoundedRect(QRectF(1.5, 1.5, t - 3, t - 3), 4, 4);
    // La flecha, blanca o negra según lo claro que sea el color
    const bool claro = c.lightnessF() > 0.6;
    p.setPen(QPen(claro ? Qt::black : Qt::white, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    const double y = t / 2.0, a = 6.0, b = t - 6.0;
    QPainterPath f;
    f.moveTo(a, y);
    f.lineTo(b, y);
    if (enviado) { f.moveTo(b - 4, y - 4); f.lineTo(b, y); f.lineTo(b - 4, y + 4); }
    else         { f.moveTo(a + 4, y - 4); f.lineTo(a, y); f.lineTo(a + 4, y + 4); }
    p.drawPath(f);
    return QIcon(px);
}

ConsolaSerie::ConsolaSerie(quint16 pieza, const QString& nombre, const QString& formato,
                           QWidget* padre)
    : QWidget(padre), pieza_(pieza), nombre_(nombre)
{
    setObjectName(QStringLiteral("serie_") + nombre);
    auto* caja = new QVBoxLayout(this);
    caja->setContentsMargins(2, 2, 2, 2);
    caja->setSpacing(3);

    // --- Arriba: qué es, los dos colores y limpiar ------------------------
    auto* fila = new QHBoxLayout;
    auto* que = new QLabel(tr("<b>%1</b> &nbsp; %2, en memoria")
                               .arg(nombre.toHtmlEscaped(), formato.toHtmlEscaped()), this);
    que->setObjectName(QStringLiteral("formato"));
    b_tx_ = new QToolButton(this);
    b_tx_->setObjectName(QStringLiteral("color_enviado"));
    b_tx_->setToolTip(tr("Color de lo enviado"));
    b_tx_->setAutoRaise(true);
    b_rx_ = new QToolButton(this);
    b_rx_->setObjectName(QStringLiteral("color_recibido"));
    b_rx_->setToolTip(tr("Color de lo recibido"));
    b_rx_->setAutoRaise(true);
    cuenta_ = new QLabel(this);
    cuenta_->setObjectName(QStringLiteral("cuenta"));
    auto* limpiar = new QToolButton(this);
    limpiar->setObjectName(QStringLiteral("limpiar"));
    limpiar->setText(tr("Limpiar"));
    limpiar->setToolTip(tr("Borra lo que se ve; no toca la linea"));
    fila->addWidget(que);
    fila->addStretch(1);
    fila->addWidget(cuenta_);
    fila->addWidget(b_tx_);
    fila->addWidget(b_rx_);
    fila->addWidget(limpiar);
    caja->addLayout(fila);

    // --- Lo que pasa por la línea ------------------------------------------
    vista_ = new QPlainTextEdit(this);
    vista_->setObjectName(QStringLiteral("trafico"));
    vista_->setReadOnly(true);
    vista_->setMaximumBlockCount(10000);
    vista_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    vista_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    caja->addWidget(vista_, 1);

    // --- La entrada ---------------------------------------------------------
    auto* fe = new QHBoxLayout;
    entrada_ = new QLineEdit(this);
    entrada_->setObjectName(QStringLiteral("entrada"));
    entrada_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    entrada_->setPlaceholderText(tr("ASCII, con bytes en hexadecimal donde se quiera: \\x41, "
                                    "\\x{0102}, hola\\x0D..."));
    entrada_->setToolTip(tr("Texto ASCII. \\xNN es un byte en hexadecimal, con dos cifras "
                            "exactas: \\x41BC son 0x41, 'B' y 'C'. \\x{NNMM...} son varios, con "
                            "un numero par de cifras: \\x{41BC} son 0x41 y 0xBC, y \\x{0D0A} lo "
                            "mismo que \\x0D\\x0A. Tambien \\r, \\n, \\t, \\0 y \\\\ para la "
                            "barra."));
    enviar_ = new QPushButton(tr("Enviar"), this);
    enviar_->setObjectName(QStringLiteral("enviar"));
    fe->addWidget(entrada_, 1);
    fe->addWidget(enviar_);
    caja->addLayout(fe);

    auto* fo = new QHBoxLayout;
    cr_ = new QCheckBox(tr("Enviar CR (\\r, \\x0D)"), this);
    cr_->setObjectName(QStringLiteral("cr"));
    lf_ = new QCheckBox(tr("Enviar LF (\\n, \\x0A)"), this);
    lf_->setObjectName(QStringLiteral("lf"));
    crc_ = new QCheckBox(tr("Enviar CRC"), this);
    crc_->setObjectName(QStringLiteral("crc"));
    crc_->setToolTip(tr("Calculado sobre lo escrito, y delante de CR y LF, que no entran en el"));
    tipo_crc_ = new QComboBox(this);
    tipo_crc_->setObjectName(QStringLiteral("tipo_crc"));
    for (const trama::CrcConocido& c : trama::crcs_conocidos()) {
        tipo_crc_->addItem(c.nombre, int(c.id));
        tipo_crc_->setItemData(tipo_crc_->count() - 1, c.ayuda, Qt::ToolTipRole);
    }
    tipo_crc_->setEnabled(false);
    previa_ = new QLabel(this);
    previa_->setObjectName(QStringLiteral("previa"));
    previa_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fo->addWidget(cr_);
    fo->addWidget(lf_);
    fo->addWidget(crc_);
    fo->addWidget(tipo_crc_);
    fo->addSpacing(12);
    fo->addWidget(previa_, 1);
    caja->addLayout(fo);

    connect(entrada_, &QLineEdit::textChanged, this, &ConsolaSerie::revisa);
    connect(entrada_, &QLineEdit::returnPressed, this, &ConsolaSerie::manda);
    connect(enviar_, &QPushButton::clicked, this, &ConsolaSerie::manda);
    for (QCheckBox* c : {cr_, lf_, crc_}) connect(c, &QCheckBox::toggled, this, &ConsolaSerie::revisa);
    connect(crc_, &QCheckBox::toggled, tipo_crc_, &QWidget::setEnabled);
    connect(tipo_crc_, &QComboBox::currentIndexChanged, this, &ConsolaSerie::revisa);
    connect(b_tx_, &QToolButton::clicked, this, [this] { elige_color(true); });
    connect(b_rx_, &QToolButton::clicked, this, [this] { elige_color(false); });
    connect(limpiar, &QToolButton::clicked, this, [this] {
        vista_->clear();
        ultimo_ = -1;
    });

    pon_colores(col_tx_, col_rx_);
    activa(false);
}

bool ConsolaSerie::trama(QByteArray& out, QString* error) const
{
    trama::Opciones o;
    o.cr = cr_->isChecked();
    o.lf = lf_->isChecked();
    o.crc = crc_->isChecked();
    o.tipo = trama::Crc(tipo_crc_->currentData().toInt());
    return trama::construye(entrada_->text(), o, out, error);
}

// Debajo, los bytes que saldrían, o por qué no se puede
void ConsolaSerie::revisa()
{
    QByteArray t;
    QString e;
    const bool ok = trama(t, &e);
    if (!ok) {
        previa_->setText(QStringLiteral("<span style=\"color:#c0392b\">%1</span>")
                             .arg(e.toHtmlEscaped()));
        previa_->setToolTip(e);
        entrada_->setStyleSheet(QStringLiteral("QLineEdit { border: 1px solid #c0392b; }"));
    } else {
        previa_->setText(t.isEmpty() ? QString()
                         : (t.size() == 1 ? tr("1 byte: %1") : tr("%1 bytes: %2").arg(t.size()))
                               .arg(hex_de(t)));
        previa_->setToolTip(QString());
        entrada_->setStyleSheet(QString());
    }
    enviar_->setEnabled(activa_ && ok && !t.isEmpty());
}

void ConsolaSerie::manda()
{
    QByteArray t;
    if (!activa_ || !trama(t) || t.isEmpty()) return;
    emit envia(pieza_, t);
    entrada_->selectAll();
}

void ConsolaSerie::activa(bool si)
{
    activa_ = si;
    revisa();
    enviar_->setToolTip(si ? QString()
                           : tr("Se puede mandar con la simulacion en marcha"));
}

void ConsolaSerie::recibido(const QByteArray& b)
{
    n_rx_ += quint64(b.size());
    pon(b, false);
}

void ConsolaSerie::enviado(const QByteArray& b)
{
    n_tx_ += quint64(b.size());
    pon(b, true);
}

void ConsolaSerie::pon(const QByteArray& b, bool tx)
{
    if (b.isEmpty()) return;
    QScrollBar* sb = vista_->verticalScrollBar();
    const bool abajo = sb->value() >= sb->maximum() - 4;
    QTextCursor c(vista_->document());
    c.movePosition(QTextCursor::End);
    // Otro sentido a media línea: a la siguiente. Solo la vista
    const int sentido = tx ? 1 : 0;
    if (ultimo_ >= 0 && ultimo_ != sentido && !c.block().text().isEmpty()) {
        c.insertBlock();
        (tx ? cr_tx_ : cr_rx_) = false;
    }
    ultimo_ = sentido;
    QTextCharFormat f;
    f.setForeground(tx ? col_tx_ : col_rx_);
    f.setProperty(SENTIDO, sentido);
    const QString s = trama::legible(b, tx ? cr_tx_ : cr_rx_);
    // Por líneas: insertText con '\n' dentro mete un U+2029, que es un
    // bloque nuevo, pero así queda claro
    const QStringList ls = s.split(QLatin1Char('\n'));
    for (int i = 0; i < ls.size(); ++i) {
        if (i) c.insertBlock();
        if (!ls[i].isEmpty()) c.insertText(ls[i], f);
    }
    if (abajo) sb->setValue(sb->maximum());
    cuenta_->setText(tr("%1 B enviados, %2 B recibidos").arg(n_tx_).arg(n_rx_));
}

void ConsolaSerie::pon_colores(const QColor& enviado, const QColor& recibido)
{
    col_tx_ = enviado.isValid() ? enviado : color_enviado_omision();
    col_rx_ = recibido.isValid() ? recibido : color_recibido_omision();
    b_tx_->setIcon(icono(col_tx_, true));
    b_rx_->setIcon(icono(col_rx_, false));
    b_tx_->setToolTip(tr("Color de lo enviado (%1)").arg(col_tx_.name()));
    b_rx_->setToolTip(tr("Color de lo recibido (%1)").arg(col_rx_.name()));
    repinta_colores();
}

// Lo que ya está escrito, con los colores nuevos
void ConsolaSerie::repinta_colores()
{
    QTextDocument* d = vista_->document();
    QTextCursor c(d);
    c.beginEditBlock();
    for (QTextBlock b = d->begin(); b.isValid(); b = b.next()) {
        for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it) {
            const QTextFragment fr = it.fragment();
            if (!fr.isValid()) continue;
            const QVariant s = fr.charFormat().property(SENTIDO);
            if (!s.isValid()) continue;
            QTextCharFormat f = fr.charFormat();
            f.setForeground(s.toInt() == 1 ? col_tx_ : col_rx_);
            c.setPosition(fr.position());
            c.setPosition(fr.position() + fr.length(), QTextCursor::KeepAnchor);
            c.setCharFormat(f);
        }
    }
    c.endEditBlock();
}

void ConsolaSerie::elige_color(bool tx)
{
    const QColor viejo = tx ? col_tx_ : col_rx_;
    const QColor c = QColorDialog::getColor(
        viejo, this, tx ? tr("Color de lo enviado") : tr("Color de lo recibido"));
    if (!c.isValid() || c == viejo) return;
    if (tx) pon_colores(c, col_rx_);
    else    pon_colores(col_tx_, c);
    emit colores(col_tx_, col_rx_);
}

QString ConsolaSerie::texto() const
{
    return vista_->toPlainText();
}

} // namespace mcusim
