#include "redes.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>

namespace mcusim::build {

namespace {

struct UF {
    QHash<QString, QString> padre;
    QString raiz(const QString& x) {
        if (!padre.contains(x)) padre.insert(x, x);
        QString r = x;
        while (padre.value(r) != r) r = padre.value(r);
        QString y = x;
        while (padre.value(y) != r) {
            const QString s = padre.value(y);
            padre[y] = r;
            y = s;
        }
        return r;
    }
    void une(const QString& a, const QString& b) {
        const QString ra = raiz(a), rb = raiz(b);
        if (ra != rb) padre[rb] = ra;
    }
};

QString limpio(const QString& s)
{
    QString o = s;
    o.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]")), QStringLiteral("_"));
    return o;
}

bool es_alimentacion(const QString& clase)
{
    return clase == QLatin1String("alimentacion") || clase == QLatin1String("masa");
}

} // namespace

Redes calcula_redes(const Documento& d, const Catalogo& c)
{
    Redes R;
    UF uf;
    QStringList orden;                    // los terminales en el orden en que aparecen
    auto mete = [&](const QString& t) {
        if (!uf.padre.contains(t)) orden << t;
        uf.raiz(t);
    };
    for (const Conexion& x : d.conexiones) {
        mete(x.a);
        mete(x.b);
        uf.une(x.a, x.b);
    }
    int n_mcu = 0;
    for (const Instancia& i : d.instancias) n_mcu += i.clase == Instancia::Mcu;

    QMap<QString, QStringList> clases;   // raíz -> terminales
    QStringList raices;
    for (const QString& t : std::as_const(orden)) {
        const QString r = uf.raiz(t);
        if (!clases.contains(r)) raices << r;
        clases[r] << t;
    }
    QSet<QString> bus(d.bus.begin(), d.bus.end());
    for (const QString& r : std::as_const(raices)) {
        const QStringList ts = clases.value(r);
        Red red;
        red.terminales = ts;
        // Lo que hay en la red
        QStringList pads, alims, conectores;
        QMap<QString, QString> nodo_mcu;      // terminal de MCU -> nodo
        bool es_bus = false;
        for (const QString& t : ts) {
            es_bus = es_bus || bus.contains(t);
            const Instancia* i = d.instancia(id_de(t));
            if (!i) continue;
            if (i->clase == Instancia::Mcu) {
                const FichaMcu* m = c.mcu(i->tipo);
                const Patilla* p = m ? m->patilla(terminal_de(t)) : nullptr;
                if (!p) continue;
                const QString n = n_mcu > 1 ? i->id + QLatin1Char('.') + p->nodo : p->nodo;
                nodo_mcu.insert(t, n);
                if (p->clase == QLatin1String("es")) {
                    if (!pads.contains(n)) pads << n;
                } else if (!alims.contains(n)) {
                    alims << n;
                }
            } else if (i->tipo == QLatin1String("Conector")) {
                conectores << t;
            }
        }
        if (pads.size() >= 2) {
            NodoDeclarado nd;
            nd.id = QStringLiteral("n_") + limpio(pads.first());
            nd.une = pads;
            nd.bus = es_bus;
            red.nodo = nd.id;
            R.nodos.push_back(nd);
            R.avisos << QStringLiteral("los pads %1 quedan unidos: es un puente de verdad, un "
                                       "cortocircuito entre ellos (doc/parts.md §2.1)")
                            .arg(pads.join(QStringLiteral(", ")));
        } else if (pads.size() == 1) {
            red.nodo = pads.first();
            if (!alims.isEmpty())
                R.errores << QStringLiteral("el pad %1 está unido a %2").arg(pads.first(), alims.join(QStringLiteral(", ")));
        } else if (!alims.isEmpty()) {
            red.nodo = alims.first();
            if (alims.size() > 1)
                R.errores << QStringLiteral("%1 están unidos: un cortocircuito").arg(alims.join(QStringLiteral(" y ")));
        } else if (!conectores.isEmpty()) {
            red.nodo = conectores.first();
        } else {
            NodoDeclarado nd;
            nd.id = QStringLiteral("n_") + limpio(ts.first());
            nd.externo = true;
            red.nodo = nd.id;
            R.nodos.push_back(nd);
        }
        // Un bus en una red que no declara nodo: se declara para decirlo
        if (es_bus) {
            bool declarado = false;
            for (NodoDeclarado& nd : R.nodos)
                if (nd.id == red.nodo) { nd.bus = true; declarado = true; }
            if (!declarado) {
                NodoDeclarado nd;
                nd.id = red.nodo;
                nd.bus = true;
                R.nodos.push_back(nd);
            }
        }
        for (const QString& t : ts) {
            if (nodo_mcu.contains(t)) continue;           // el MCU no escribe <pin>
            if (t == red.nodo) continue;                  // el pin del conector que da nombre
            R.nodo_de.insert(t, red.nodo);
        }
        R.redes.push_back(red);
    }
    // Los obligatorios sin conectar, con la ficha
    for (const Instancia& i : d.instancias) {
        if (i.clase == Instancia::Mcu) continue;
        const FichaPieza* f = c.pieza(i.tipo);
        if (!f) {
            R.errores << QStringLiteral("%1: la biblioteca no tiene el tipo %2").arg(i.id, i.tipo);
            continue;
        }
        auto conectado = [&](const QString& t) {
            return uf.padre.contains(i.id + QLatin1Char('.') + t);
        };
        // Por grupos de «cuando»: un obligatorio, uno de un grupo que pide al
        // menos uno, o uno de dos alternativos
        QMap<QString, QStringList> grupos;
        for (const Terminal& t : f->terminales) {
            const QString cu = t.cuando;
            if (cu.startsWith(QLatin1String("obligatorio"))) {
                if (!conectado(t.nombre)) {
                    R.sin_conectar << i.id + QLatin1Char('.') + t.nombre;
                    R.avisos << QStringLiteral("%1.%2 es obligatorio y no está conectado").arg(i.id, t.nombre);
                }
            } else if (t.alternativo || cu.startsWith(QLatin1String("al menos")) ||
                       cu.startsWith(QLatin1String("uno de"))) {
                grupos[cu] << t.nombre;
            }
        }
        for (auto it = grupos.cbegin(); it != grupos.cend(); ++it) {
            bool alguno = false;
            for (const QString& t : it.value()) alguno = alguno || conectado(t);
            if (!alguno) {
                for (const QString& t : it.value()) R.sin_conectar << i.id + QLatin1Char('.') + t;
                R.avisos << QStringLiteral("%1: hace falta conectar al menos uno de %2")
                                .arg(i.id, it.value().join(QStringLiteral(", ")));
            }
        }
    }
    return R;
}

} // namespace mcusim::build
