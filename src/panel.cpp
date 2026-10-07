#include "panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QWidget>

#include <QPixmap>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace mcusim {

namespace {

// Un mando de tipo boton tiene DOS controles, el dedo y el «switch», y lo que
// el modelo recibe es uno O el otro. Este es su estado, compartido por los dos.
struct EstadoBoton {
    bool dedo = false;        // el boton momentaneo, hundido
    bool fijo = false;        // el «switch», marcado
    bool enviado = false;     // lo ultimo que se ordeno: no se repite
};

// El control de un mando, y lo que ordena. Nace desactivado. Los widgets que
// se encienden y se apagan van a `controles`; lo que se devuelve es lo que se
// coloca en el recuadro.
QWidget* control_de(Panel* panel, const PiezaGui& p, const MandoGui& m, QWidget* padre,
                    QVector<QWidget*>& controles)
{
    const quint16 pz = quint16(p.idx), md = quint16(m.idx);
    const float lo = float(m.min), hi = float(m.max);
    QWidget* w = nullptr;
    QString ayuda;
    if (m.tipo == QLatin1String("interruptor")) {
        auto* c = new QCheckBox(m.nombre, padre);
        c->setChecked(m.valor >= (m.min + m.max) / 2);   // donde esta el modelo
        QObject::connect(c, &QCheckBox::toggled, panel,
                         [=](bool si) { emit panel->orden(pz, md, si ? hi : lo); });
        ayuda = QObject::tr("Marcado ordena %1; desmarcado, %2.").arg(hi).arg(lo);
        w = c;
    } else if (m.tipo == QLatin1String("discreto")) {
        // Los ENTEROS del rango: un desplegable, que es lo que se pidio para
        // `rebotes` -de 1 a 9- y lo que vale para cualquier mando de pocas
        // opciones. Si el rango es largo, un desplegable de cien lineas no se
        // maneja: entonces una caja numerica. Como el deslizador, nace donde
        // esta el modelo, y colocarlo ahi no ordena nada.
        const int a = int(std::ceil(m.min)), b = int(std::floor(m.max));
        const int inicial = qBound(a, int(std::lround(m.valor)), b);
        auto* fila = new QWidget(padre);
        auto* h = new QHBoxLayout(fila);
        h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(new QLabel(m.nombre, fila));
        QWidget* c = nullptr;
        if (b - a <= 30) {
            auto* cb = new QComboBox(fila);
            for (int k = a; k <= b; ++k) cb->addItem(QString::number(k), k);
            {
                const QSignalBlocker quieto(cb);
                cb->setCurrentIndex(inicial - a);
            }
            QObject::connect(cb, &QComboBox::currentIndexChanged, panel, [=](int i) {
                if (i >= 0) emit panel->orden(pz, md, float(cb->itemData(i).toInt()));
            });
            c = cb;
        } else {
            auto* sb = new QSpinBox(fila);
            sb->setRange(a, b);
            {
                const QSignalBlocker quieto(sb);
                sb->setValue(inicial);
            }
            QObject::connect(sb, &QSpinBox::valueChanged, panel,
                             [=](int v) { emit panel->orden(pz, md, float(v)); });
            c = sb;
        }
        c->setObjectName(QStringLiteral("mando:%1:%2").arg(p.idx).arg(m.idx));
        c->setEnabled(false);
        c->setToolTip(QObject::tr("%1: un entero de %2 a %3.").arg(m.nombre).arg(a).arg(b));
        controles.push_back(c);
        h->addWidget(c);
        h->addStretch(1);
        return fila;
    } else if (m.tipo == QLatin1String("continuo")) {
        // El deslizador, con su NOMBRE y su VALOR al lado: sin el número no se
        // sabe qué se está pidiendo. Nace donde está el modelo (el `valor` del
        // catálogo), y colocarlo ahí no ordena nada: no es la mano.
        auto* fila = new QWidget(padre);
        auto* h = new QHBoxLayout(fila);
        h->setContentsMargins(0, 0, 0, 0);
        auto* s = new QSlider(Qt::Horizontal, fila);
        s->setObjectName(QStringLiteral("mando:%1:%2").arg(p.idx).arg(m.idx));
        s->setRange(0, 1000);               // de min a max, en milésimas
        auto* num = new QLabel(fila);
        num->setObjectName(QStringLiteral("valor:%1:%2").arg(p.idx).arg(m.idx));
        num->setMinimumWidth(num->fontMetrics().horizontalAdvance(QStringLiteral("00000.0")));
        auto escribe = [=](int v) {
            num->setText(QString::number(double(lo + (hi - lo) * float(v) / 1000.f), 'g', 4));
        };
        const double f = hi > lo ? (m.valor - m.min) / (m.max - m.min) : 0.0;
        {
            const QSignalBlocker quieto(s);
            s->setValue(qBound(0, int(f * 1000.0 + 0.5), 1000));
        }
        escribe(s->value());
        QObject::connect(s, &QSlider::valueChanged, panel, [=](int v) {
            escribe(v);
            emit panel->orden(pz, md, lo + (hi - lo) * float(v) / 1000.f);
        });
        s->setEnabled(false);
        s->setToolTip(QObject::tr("%1: de %2 a %3.").arg(m.nombre).arg(lo).arg(hi));
        controles.push_back(s);
        h->addWidget(new QLabel(m.nombre, fila));
        h->addWidget(s, 1);
        h->addWidget(num);
        return fila;
    } else {                                // "boton", y lo que no se conozca
        // DOS CONTROLES PARA UN MANDO. El de siempre, que es un dedo: la
        // pulsacion dura lo que dura el raton abajo. Y el «switch», que la deja
        // puesta hasta la siguiente vez que se toque: es lo que hace falta para
        // depurar paso a paso con el boton pulsado desde OTRA ventana -la del
        // IDE-, sin tener que sujetar el raton aqui.
        //
        // El mando esta hundido si lo esta CUALQUIERA de los dos, y solo se
        // ordena cuando eso cambia: con el «switch» puesto, el dedo no ordena
        // nada -ni al bajar ni, sobre todo, al subir, que soltaria lo que el
        // «switch» sujeta-; y quitar el «switch» con el dedo abajo tampoco.
        auto estado = std::make_shared<EstadoBoton>();
        auto ordena = [=] {
            const bool ahora = estado->dedo || estado->fijo;
            if (ahora == estado->enviado) return;
            estado->enviado = ahora;
            emit panel->orden(pz, md, ahora ? hi : lo);
        };
        auto* fila = new QWidget(padre);
        auto* h = new QHBoxLayout(fila);
        h->setContentsMargins(0, 0, 0, 0);
        auto* b = new QPushButton(m.nombre, fila);
        b->setObjectName(QStringLiteral("mando:%1:%2").arg(p.idx).arg(m.idx));
        QObject::connect(b, &QPushButton::pressed, panel,
                         [=] { estado->dedo = true;  ordena(); });
        QObject::connect(b, &QPushButton::released, panel,
                         [=] { estado->dedo = false; ordena(); });
        b->setToolTip(QObject::tr("Mientras esta hundido ordena %1; al soltarlo, %2. "
                                  "Con el «switch» puesto no ordena nada.")
                          .arg(hi).arg(lo));
        auto* sw = new QPushButton(QStringLiteral("switch"), fila);
        sw->setObjectName(QStringLiteral("fija:%1:%2").arg(p.idx).arg(m.idx));
        sw->setCheckable(true);
        QObject::connect(sw, &QPushButton::toggled, panel,
                         [=](bool si) { estado->fijo = si; ordena(); });
        sw->setToolTip(QObject::tr("Cada pulsacion cambia %1 entre hundido (%2) y suelto "
                                   "(%3), y lo deja asi: para seguir con el hundido "
                                   "mientras se trabaja en otra ventana.")
                           .arg(m.nombre).arg(hi).arg(lo));
        h->addWidget(b, 1);
        h->addWidget(sw);
        for (QPushButton* x : {b, sw}) {
            x->setEnabled(false);
            controles.push_back(x);
        }
        return fila;
    }
    w->setObjectName(QStringLiteral("mando:%1:%2").arg(p.idx).arg(m.idx));
    w->setEnabled(false);
    w->setToolTip(ayuda);
    controles.push_back(w);
    return w;
}

// Una pieza con más patillas que esto -un conector de 38 pines- las pliega en
// una línea, con la lista entera en la ayuda emergente: si no, su recuadro se
// come la pantalla y no dice nada que se lea.
constexpr int MAX_PATILLAS_A_LA_VISTA = 8;

QGroupBox* recuadro_de(Panel* panel, const PiezaGui& p, const PlacaGui& placa,
                       QWidget* padre, QHash<quint16, QLabel*>& etiquetas,
                       QVector<QWidget*>& controles, QHash<quint16, QLabel*>& imagenes)
{
    // En un sistema, el recuadro de la placa ya dice cuál es: aquí basta el
    // nombre de la pieza dentro de ella (`LD2`, no `N/LD2`).
    QString titulo = QStringLiteral("%1 · %2").arg(p.id_local, p.tipo);
    if (!p.conectada) titulo += QStringLiteral(" (desoldada)");
    auto* g = new QGroupBox(titulo, padre);
    g->setObjectName(QStringLiteral("pieza:%1").arg(p.idx));
    auto* f = new QFormLayout(g);

    if (p.patillas.size() > MAX_PATILLAS_A_LA_VISTA) {
        int soldadas = 0;
        QStringList lista;
        for (const PatillaGui& t : p.patillas) {
            lista << QStringLiteral("%1 → %2").arg(t.nombre, t.nodo);
            // Un pin al aire se llama como él mismo: `N/CN5.7`
            if (!t.nodo.endsWith(QLatin1Char('.') + t.nombre)) ++soldadas;
        }
        auto* l = new QLabel(QObject::tr("%1 patillas, %2 a nodos de la placa")
                                 .arg(p.patillas.size()).arg(soldadas), g);
        l->setObjectName(QStringLiteral("patillas:%1").arg(p.idx));
        l->setToolTip(lista.join(QLatin1Char('\n')));
        f->addRow(QObject::tr("patillas →"), l);
    } else {
        for (const PatillaGui& t : p.patillas) {
            auto* l = new QLabel(t.nodo, g);
            l->setObjectName(QStringLiteral("patilla:%1:%2").arg(p.idx).arg(t.nombre));
            f->addRow(t.nombre + QStringLiteral(" →"), l);
        }
    }
    // Un conector enchufado dice con qué: lo dice el <acopla> del sistema
    for (const AcopleGui& a : placa.acoples) {
        if (!a.conectores.contains(p.id)) continue;
        QStringList otros = a.conectores;
        otros.removeAll(p.id);
        QString texto = otros.join(QStringLiteral(", "));
        if (a.espejo) texto += QObject::tr(" (en espejo)");
        else if (a.conectores.size() > 2) texto += QObject::tr(" (en pila)");
        auto* l = new QLabel(texto, g);
        l->setObjectName(QStringLiteral("acople:%1").arg(p.idx));
        f->addRow(QObject::tr("enchufado a ⇄"), l);
    }
    int ocultos = 0;
    for (const ObservableGui& o : p.observables) {
        if (!o.interesante) { ++ocultos; continue; }
        auto* l = new QLabel(QStringLiteral("—"), g);
        l->setObjectName(QStringLiteral("obs:%1").arg(o.id_obs));
        etiquetas.insert(quint16(o.id_obs), l);
        QString nombre = o.nombre;
        if (!o.unidad.isEmpty()) nombre += QStringLiteral(" (%1)").arg(o.unidad);
        f->addRow(QStringLiteral("<b>%1</b>").arg(nombre), l);
    }
    // Las imágenes, a su tamaño: una pantalla de 128x160 se ve como es. Nace
    // negra, que es lo que se ve sin luz.
    for (const ImagenGui& im : p.imagenes) {
        auto* l = new QLabel(g);
        l->setObjectName(QStringLiteral("img:%1").arg(im.id_obs));
        l->setFixedSize(im.ancho, im.alto);
        l->setStyleSheet(QStringLiteral("background: #000000;"));
        l->setToolTip(QObject::tr("%1: %2x%3 pixeles").arg(im.nombre).arg(im.ancho).arg(im.alto));
        imagenes.insert(quint16(im.id_obs), l);
        f->addRow(QStringLiteral("<b>%1</b>").arg(im.nombre), l);
    }
    for (const MandoGui& m : p.mandos)
        f->addRow(control_de(panel, p, m, g, controles));
    if (ocultos > 0) {
        auto* l = new QLabel(QObject::tr("+%n observable(s) sin pintar", nullptr, ocultos), g);
        l->setObjectName(QStringLiteral("ocultos:%1").arg(p.idx));
        l->setEnabled(false);
        f->addRow(l);
    }
    return g;
}

} // namespace

Panel::Panel(const PlacaGui& placa, QWidget* padre) : QWidget(padre)
{
    setObjectName(QStringLiteral("panel"));
    const int columnas = 3;
    QHash<quint16, QLabel*> etiquetas;
    // Las piezas en una rejilla de tres columnas, dentro de `dentro`
    auto rejilla_de = [&](QWidget* dentro, const QVector<int>& cuales) {
        auto* rejilla = new QGridLayout(dentro);
        for (int k = 0; k < cuales.size(); ++k)
            rejilla->addWidget(recuadro_de(this, placa.piezas[cuales[k]], placa, dentro,
                                           etiquetas, controles_, img_),
                               k / columnas, k % columnas, Qt::AlignTop);
        rejilla->setRowStretch(int((cuales.size() + columnas - 1) / columnas), 1);
    };
    if (!placa.es_sistema()) {
        QVector<int> todas;
        for (int i = 0; i < placa.piezas.size(); ++i) todas.push_back(i);
        rejilla_de(this, todas);
    } else {
        // UN RECUADRO POR PLACA, con las suyas dentro, en el orden del sistema.
        // Una pieza que no diga de qué placa es -no debería haberla- va al
        // final, en uno aparte, para que no se pierda.
        auto* v = new QVBoxLayout(this);
        const QVector<EnlaceGui> enlaces = placa.enlaces();
        QVector<SubPlacaGui> grupos = placa.placas;
        grupos.push_back({QString(), tr("(sin placa)"), QString()});
        for (const SubPlacaGui& s : grupos) {
            QVector<int> suyas;
            for (int i = 0; i < placa.piezas.size(); ++i)
                if (placa.piezas[i].placa == s.id) suyas.push_back(i);
            if (suyas.isEmpty() && s.id.isEmpty()) continue;
            auto* g = new QGroupBox(s.id.isEmpty() ? s.nombre
                                                   : QStringLiteral("%1 · %2").arg(s.id, s.nombre),
                                    this);
            g->setObjectName(QStringLiteral("placa:%1").arg(s.id));
            // La ayuda dice de dónde sale la placa y a cuáles está unida
            QStringList ayuda;
            if (!s.fichero.isEmpty()) ayuda << s.fichero;
            for (const EnlaceGui& e : enlaces) {
                if (e.placa_a == s.id && !s.id.isEmpty())
                    ayuda << tr("unida a %1 por %2").arg(e.placa_b, e.por);
                else if (e.placa_b == s.id && !s.id.isEmpty())
                    ayuda << tr("unida a %1 por %2").arg(e.placa_a, e.por);
            }
            g->setToolTip(ayuda.join(QLatin1Char('\n')));
            rejilla_de(g, suyas);
            v->addWidget(g);
        }
        v->addStretch(1);
    }
    for (const PiezaGui& p : placa.piezas)
        for (const ObservableGui& o : p.observables)
            if (QLabel* l = etiquetas.value(quint16(o.id_obs), nullptr)) {
                ind_.insert(quint16(o.id_obs), {l, o, l->parentWidget(), false});
                pintados_.push_back(quint16(o.id_obs));
            }
    for (const PiezaGui& p : placa.piezas)
        for (const ImagenGui& im : p.imagenes)
            if (img_.contains(quint16(im.id_obs))) pintados_.push_back(quint16(im.id_obs));
}

QImage Panel::imagen_de(int ancho, int alto, const QByteArray& rgb, float brillo)
{
    if (ancho <= 0 || alto <= 0 || rgb.size() < qint64(ancho) * alto * 3) return QImage();
    QImage im(reinterpret_cast<const uchar*>(rgb.constData()), ancho, alto, ancho * 3,
              QImage::Format_RGB888);
    QImage c = im.copy();                    // `im` no es dueño de sus datos
    const float b = std::clamp(brillo, 0.f, 1.f);
    if (b < 0.999f) {
        // La luz multiplica cada color: sin luz, negro
        const int k = int(b * 256.f + 0.5f);
        for (int y = 0; y < alto; ++y) {
            uchar* f = c.scanLine(y);
            for (int x = 0; x < ancho * 3; ++x) f[x] = uchar((f[x] * k) >> 8);
        }
    }
    return c;
}

void Panel::pon_imagen(quint16 id_obs, int ancho, int alto, const QByteArray& rgb, float brillo)
{
    QLabel* l = img_.value(id_obs, nullptr);
    if (!l) return;
    const QImage im = imagen_de(ancho, alto, rgb, brillo);
    if (!im.isNull()) l->setPixmap(QPixmap::fromImage(im));
}

bool Panel::en_alarma(const ObservableGui& o, float valor)
{
    return o.alarma && valor >= 0.5f;
}

QString Panel::texto_de(const ObservableGui& o, float valor)
{
    if (o.alarma)
        return en_alarma(o, valor) ? QStringLiteral("⚠ SI") : QStringLiteral("○");
    if (o.min == 0 && o.max == 1 && o.unidad.isEmpty())
        return valor >= 0.5f ? QStringLiteral("●") : QStringLiteral("○");
    QString t = QString::number(double(valor), 'g', 4);
    if (!o.unidad.isEmpty()) t += QLatin1Char(' ') + o.unidad;
    return t;
}

void Panel::pon_valor(quint16 id_obs, float valor)
{
    const auto it = ind_.constFind(id_obs);
    if (it == ind_.constEnd()) return;
    const QString t = texto_de(it->obs, valor);
    if (it->etiqueta->text() != t) it->etiqueta->setText(t);
    const bool d = en_alarma(it->obs, valor);
    if (d == it->disparada) return;
    ind_[id_obs].disparada = d;
    // Solo al cambiar: la hoja de estilo cuesta, y llegan muestras cada pocos ms
    const QString rojo = QStringLiteral("color: #c62828; font-weight: bold;");
    it->etiqueta->setStyleSheet(d ? rojo : QString());
    // El recuadro de la pieza avisa si alguna de sus alarmas está disparada
    bool alguna = false;
    for (const Indicador& x : std::as_const(ind_))
        if (x.recuadro == it->recuadro && x.disparada) alguna = true;
    it->recuadro->setProperty("alarma", alguna);
    it->recuadro->setStyleSheet(alguna ? QStringLiteral("QGroupBox { color: #c62828; "
                                                        "font-weight: bold; }")
                                       : QString());
}

void Panel::activa_mandos(bool si)
{
    activos_ = si;
    for (QWidget* w : controles_) w->setEnabled(si);
}

Panel* construye_panel(const PlacaGui& placa, QWidget* padre)
{
    return new Panel(placa, padre);
}

} // namespace mcusim
