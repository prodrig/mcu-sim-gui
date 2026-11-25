#include "panel_biblioteca.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDrag>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#include "biblioteca.h"
#include "lienzo.h"

namespace mcusim::build {

namespace {

const int ICONO = 48;

// Una lista que arrastra la clave de su entrada
class ListaArrastrable : public QListWidget {
public:
    using QListWidget::QListWidget;
protected:
    QMimeData* mimeData(const QList<QListWidgetItem*>& its) const override
    {
        auto* m = new QMimeData;
        if (!its.isEmpty()) m->setData(mime_entrada(), its.front()->data(Qt::UserRole).toString().toUtf8());
        return m;
    }
    QStringList mimeTypes() const override { return {mime_entrada()}; }
};

QListWidget* lista(QWidget* padre)
{
    auto* l = new ListaArrastrable(padre);
    l->setIconSize(QSize(ICONO, ICONO));
    l->setDragEnabled(true);
    l->setDragDropMode(QAbstractItemView::DragOnly);
    l->setSelectionMode(QAbstractItemView::SingleSelection);
    l->setUniformItemSizes(true);
    return l;
}

QString clase_de(const Entrada& e)
{
    switch (e.clase) {
    case Entrada::Pieza: return QStringLiteral("pieza");
    case Entrada::Mcu: return QStringLiteral("MCU");
    case Entrada::Perfil: return QStringLiteral("conector");
    case Entrada::Conjunto: return QStringLiteral("conjunto de conectores");
    case Entrada::Tarjeta: return QStringLiteral("placa");
    case Entrada::Sistema: return QStringLiteral("sistema");
    }
    return {};
}

} // namespace

PanelBiblioteca::PanelBiblioteca(Biblioteca* b, QWidget* padre) : QWidget(padre), b_(b)
{
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(2, 2, 2, 2);
    busca_ = new QLineEdit(this);
    busca_->setPlaceholderText(QStringLiteral("Buscar… (Led, STM32F4, arduino)"));
    busca_->setClearButtonEnabled(true);
    v->addWidget(busca_);
    tabs_ = new QTabWidget(this);
    piezas_ = lista(this);
    tarjetas_ = lista(this);
    tabs_->addTab(piezas_, QStringLiteral("Piezas"));
    tabs_->addTab(tarjetas_, QStringLiteral("Tarjetas"));
    v->addWidget(tabs_, 1);
    estado_ = new QLabel(this);
    estado_->setWordWrap(true);
    v->addWidget(estado_);
    auto* h = new QHBoxLayout;
    auto* bc = new QPushButton(QStringLiteral("Carpetas…"), this);
    auto* br = new QPushButton(QStringLiteral("Refrescar"), this);
    br->setToolTip(QStringLiteral("Vuelve a leer lo que haya cambiado en las carpetas.\n"
                                  "Con Mayúsculas, lo relee todo."));
    h->addWidget(bc);
    h->addWidget(br);
    v->addLayout(h);
    reloj_ = new QTimer(this);
    reloj_->setInterval(0);
    connect(reloj_, &QTimer::timeout, this, &PanelBiblioteca::carga_miniaturas);
    connect(busca_, &QLineEdit::textChanged, this, &PanelBiblioteca::filtra);
    connect(bc, &QPushButton::clicked, this, &PanelBiblioteca::carpetas);
    connect(br, &QPushButton::clicked, this, [this] {
        refresca(QApplication::keyboardModifiers() & Qt::ShiftModifier);
    });
    for (QListWidget* l : {piezas_, tarjetas_})
        connect(l, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) {
            if (const Entrada* e = entrada(it)) emit pedida(*e);
        });
    rellena();
}

const Entrada* PanelBiblioteca::entrada(QListWidgetItem* it) const
{
    if (!it) return nullptr;
    const QString k = it->data(Qt::UserRole).toString();
    for (const Entrada& e : b_->entradas())
        if (e.clave() == k && !e.desaparecida) return &e;
    return nullptr;
}

int PanelBiblioteca::cuantas_piezas() const { return piezas_->count(); }
int PanelBiblioteca::cuantas_tarjetas() const { return tarjetas_->count(); }

void PanelBiblioteca::rellena()
{
    reloj_->stop();
    pendientes_.clear();
    piezas_->clear();
    tarjetas_->clear();
    QVector<Entrada> es = b_->piezas();
    es += b_->tarjetas();
    std::sort(es.begin(), es.end(), [](const Entrada& a, const Entrada& b) {
        if (a.clase != b.clase) return a.clase < b.clase;
        return a.nombre.compare(b.nombre, Qt::CaseInsensitive) < 0;
    });
    QPixmap vacio(ICONO, ICONO);
    vacio.fill(Qt::transparent);
    for (const Entrada& e : es) {
        if (e.clase == Entrada::Conjunto) continue;      // todavía no se colocan
        QListWidget* l = e.de_piezas() ? piezas_ : tarjetas_;
        auto* it = new QListWidgetItem(QIcon(vacio), e.nombre, l);
        it->setData(Qt::UserRole, e.clave());
        it->setToolTip(QStringLiteral("<b>%1</b> (%2)<br>%3<br><small>%4</small>")
                           .arg(e.nombre.toHtmlEscaped(), clase_de(e), e.resumen.toHtmlEscaped(),
                                e.ruta.toHtmlEscaped()));
        pendientes_ << it;
    }
    const QStringList c = b_->carpetas();
    estado_->setText(c.isEmpty() ? QStringLiteral("No hay carpetas: «Carpetas…» para importar la "
                                                  "biblioteca de mcu-sim (src/biblioteca) y sus placas")
                                 : QStringLiteral("%1 piezas, %2 tarjetas, de %3 carpetas")
                                       .arg(piezas_->count()).arg(tarjetas_->count()).arg(c.size()));
    filtra();
    reloj_->start();
}

void PanelBiblioteca::carga_miniaturas()
{
    // Primero las que se ven
    for (int n = 0; n < 6 && !pendientes_.isEmpty(); ++n) {
        QListWidgetItem* it = nullptr;
        for (QListWidgetItem* p : std::as_const(pendientes_)) {
            QListWidget* l = p->listWidget();
            if (l && l->isVisible() && !p->isHidden() && l->viewport()->rect().intersects(l->visualItemRect(p))) {
                it = p;
                break;
            }
        }
        if (!it) it = pendientes_.front();
        pendientes_.removeOne(it);
        if (const Entrada* e = entrada(it)) {
            const QImage im = b_->miniatura(*e, ICONO);
            if (!im.isNull()) it->setIcon(QIcon(QPixmap::fromImage(im)));
        }
    }
    if (pendientes_.isEmpty()) reloj_->stop();
}

void PanelBiblioteca::filtra()
{
    const QStringList ps = busca_->text().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (QListWidget* l : {piezas_, tarjetas_})
        for (int i = 0; i < l->count(); ++i) {
            QListWidgetItem* it = l->item(i);
            bool ve = true;
            for (const QString& p : ps)
                ve = ve && (it->text().contains(p, Qt::CaseInsensitive) ||
                            it->toolTip().contains(p, Qt::CaseInsensitive));
            it->setHidden(!ve);
        }
}

void PanelBiblioteca::refresca(bool completo)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const int n = b_->refresca(completo);
    QApplication::restoreOverrideCursor();
    rellena();
    for (const QString& e : b_->errores()) emit mensaje(e);
    emit mensaje(QStringLiteral("biblioteca: %1 ficheros leídos").arg(n));
    emit refrescada(n);
}

void PanelBiblioteca::carpetas()
{
    QDialog d(this);
    d.setWindowTitle(QStringLiteral("Carpetas de la biblioteca"));
    auto* v = new QVBoxLayout(&d);
    v->addWidget(new QLabel(QStringLiteral("Se importan las piezas (*.pieza.xml), los MCU (*.mcu.xml), los "
                                           "conectores y las placas de estas carpetas y de las de dentro:"),
                            &d));
    auto* l = new QListWidget(&d);
    l->addItems(b_->carpetas());
    v->addWidget(l);
    auto* h = new QHBoxLayout;
    auto* mas = new QPushButton(QStringLiteral("Añadir…"), &d);
    auto* menos = new QPushButton(QStringLiteral("Quitar"), &d);
    h->addWidget(mas);
    h->addWidget(menos);
    h->addStretch();
    v->addLayout(h);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &d);
    v->addWidget(bb);
    connect(mas, &QPushButton::clicked, &d, [&] {
        const QString c = QFileDialog::getExistingDirectory(&d, QStringLiteral("Carpeta de la biblioteca"));
        if (!c.isEmpty() && l->findItems(c, Qt::MatchExactly).isEmpty()) l->addItem(c);
    });
    connect(menos, &QPushButton::clicked, &d, [&] { delete l->currentItem(); });
    connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    d.resize(560, 320);
    if (d.exec() != QDialog::Accepted) return;
    QStringList cs;
    for (int i = 0; i < l->count(); ++i) cs << l->item(i)->text();
    if (cs == b_->carpetas()) return;
    b_->pon_carpetas(cs);
    refresca(true);
}

} // namespace mcusim::build
