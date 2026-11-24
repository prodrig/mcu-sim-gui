#include "trazado.h"

#include <QLineF>
#include <QPainterPathStroker>

#include <algorithm>
#include <cmath>

namespace mcusim::trazado {

Enganche enganche(const QRectF& r, const QPointF& hacia)
{
    const QPointF d = hacia - r.center();
    if (std::abs(d.x()) >= std::abs(d.y())) {
        const bool derecha = d.x() > 0;
        return Enganche{QPointF(derecha ? r.right() : r.left(), r.center().y()),
                        QPointF(derecha ? 1 : -1, 0)};
    }
    const bool abajo = d.y() > 0;
    return Enganche{QPointF(r.center().x(), abajo ? r.bottom() : r.top()),
                    QPointF(0, abajo ? 1 : -1)};
}

QPainterPath curva(const Enganche& a, const Enganche& b, double minimo)
{
    QPainterPath c(a.p);
    const double lado = std::max(std::max(std::abs(b.p.x() - a.p.x()),
                                          std::abs(b.p.y() - a.p.y())) * 0.45, minimo);
    c.cubicTo(a.p + a.dir * lado, b.p + b.dir * lado, b.p);
    return c;
}

QVector<QPointF> puntos_ruta(const QRectF& ra, const QRectF& rb, bool horizontal,
                             const QVector<double>& codos)
{
    const QPointF ca = ra.center(), cb = rb.center();
    QPointF pa;
    if (horizontal) {
        const double hacia = codos.isEmpty() ? cb.x() : codos[0];
        pa = QPointF(hacia > ca.x() ? ra.right() : ra.left(), ca.y());
    } else {
        const double hacia = codos.isEmpty() ? cb.y() : codos[0];
        pa = QPointF(ca.x(), hacia > ca.y() ? ra.bottom() : ra.top());
    }
    QVector<QPointF> p{pa};
    bool h = horizontal;                         // el tramo que toca
    for (const double v : codos) {
        const QPointF q = p.back();
        p.push_back(h ? QPointF(v, q.y()) : QPointF(q.x(), v));
        h = !h;
    }
    const QPointF ult = p.back();
    if (!h) {                                    // uno vertical y otro horizontal
        const QPointF pb(ult.x() > cb.x() ? rb.right() : rb.left(), cb.y());
        p << QPointF(ult.x(), pb.y()) << pb;
    } else {                                     // uno horizontal y otro vertical
        const QPointF pb(cb.x(), ult.y() > cb.y() ? rb.bottom() : rb.top());
        p << QPointF(pb.x(), ult.y()) << pb;
    }
    return p;
}

QVector<double> codos_de(const QVector<QPointF>& p, bool horizontal)
{
    QVector<double> c;
    for (int j = 0; j + 3 < p.size(); ++j) {
        const bool h = (j % 2 == 0) == horizontal;
        c.push_back(h ? p[j + 1].x() : p[j + 1].y());
    }
    return c;
}

QPainterPath redondeada(const QVector<QPointF>& puntos, double r)
{
    QVector<QPointF> p;
    for (const QPointF& x : puntos)
        if (p.isEmpty() || QLineF(p.back(), x).length() > 1e-9) p.push_back(x);
    QPainterPath c(p.isEmpty() ? QPointF() : p.front());
    for (int i = 1; i + 1 < p.size(); ++i) {
        const QPointF a = p[i - 1], b = p[i], d = p[i + 1];
        const double l1 = QLineF(a, b).length(), l2 = QLineF(b, d).length();
        const QPointF u1 = (b - a) / l1, u2 = (d - b) / l2;
        if (std::abs(u1.x() * u2.y() - u1.y() * u2.x()) < 1e-9) {
            c.lineTo(b);
            continue;
        }
        const double rr = std::min({r, l1 / 2, l2 / 2});
        c.lineTo(b - u1 * rr);
        c.quadTo(b, b + u2 * rr);
    }
    if (p.size() > 1) c.lineTo(p.back());
    return c;
}

double distancia(const QPointF& s, const QPointF& a, const QPointF& b)
{
    const QPointF d = b - a;
    const double l2 = d.x() * d.x() + d.y() * d.y();
    double t = l2 > 0 ? ((s - a).x() * d.x() + (s - a).y() * d.y()) / l2 : 0;
    t = std::clamp(t, 0.0, 1.0);
    return QLineF(s, a + d * t).length();
}

bool eje_de_curva(const QPainterPath& c, const QPointF& pa)
{
    const QPointF d = c.elementCount() > 1 ? QPointF(c.elementAt(1)) - pa : QPointF(1, 0);
    return std::abs(d.x()) >= std::abs(d.y());
}

double codo_medio(const QPointF& pa, const QPointF& pb, bool horizontal)
{
    return horizontal ? (pa.x() + pb.x()) / 2 : (pa.y() + pb.y()) / 2;
}

QVector<double> con_codo(const QVector<QPointF>& puntos, const QPointF& s, bool horizontal)
{
    if (puntos.size() < 2) return codos_de(puntos, horizontal);
    int k = 0;
    double mejor = 1e300;
    for (int j = 0; j + 1 < puntos.size(); ++j) {
        const double d = distancia(s, puntos[j], puntos[j + 1]);
        if (d < mejor) { mejor = d; k = j; }
    }
    const QPointF a = puntos[k], b = puntos[k + 1];
    const QPointF c = QPointF(a.x() == b.x() ? a.x() : std::clamp(s.x(), std::min(a.x(), b.x()),
                                                                  std::max(a.x(), b.x())),
                              a.y() == b.y() ? a.y() : std::clamp(s.y(), std::min(a.y(), b.y()),
                                                                  std::max(a.y(), b.y())));
    QVector<QPointF> p = puntos;
    p.insert(k + 1, c);
    p.insert(k + 1, c);
    return codos_de(p, horizontal);
}

int tramo_en(const QVector<QPointF>& puntos, const QPointF& s, double tol)
{
    for (int k = 0; k + 1 < puntos.size(); ++k)
        if (distancia(s, puntos[k], puntos[k + 1]) <= tol) return k;
    return -1;
}

bool toca(const QPainterPath& camino, const QPointF& s, double tol)
{
    QPainterPathStroker borde;
    borde.setWidth(2 * tol);
    return borde.createStroke(camino).contains(s);
}

} // namespace mcusim::trazado
