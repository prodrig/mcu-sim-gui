#include "valida.h"

#include <QDir>
#include <QProcess>
#include <QTemporaryDir>

#include "exporta_placa.h"

namespace mcusim::build {

Validacion valida_fichero(const QString& mcu_sim, const QString& ruta_xml, int ms)
{
    Validacion v;
    if (mcu_sim.isEmpty()) {
        v.lineas << QStringLiteral("No se sabe dónde está mcu-sim: dilo en Biblioteca > mcu-sim.");
        return v;
    }
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(mcu_sim, {ruta_xml, QStringLiteral("--valida")});
    if (!p.waitForStarted(10000)) {
        v.lineas << QStringLiteral("No se puede ejecutar %1").arg(mcu_sim);
        return v;
    }
    if (!p.waitForFinished(ms)) {
        p.kill();
        p.waitForFinished(2000);
        v.lineas << QStringLiteral("mcu-sim no ha terminado en %1 s").arg(ms / 1000);
        return v;
    }
    v.lanzado = true;
    v.codigo = p.exitCode();
    v.valida = p.exitStatus() == QProcess::NormalExit && v.codigo == 0;
    for (const QString& l : QString::fromUtf8(p.readAll()).split(QLatin1Char('\n'))) {
        const QString t = l.trimmed();
        if (t.isEmpty() || t.contains(QLatin1String("SystemC")) || t.contains(QLatin1String("Copyright")) ||
            t.contains(QLatin1String("ALL RIGHTS")) || t.startsWith(QLatin1String("Info:")))
            continue;
        v.lineas << l;
    }
    return v;
}

Validacion valida(const QString& mcu_sim, const Documento& d, const Catalogo& c)
{
    QTemporaryDir dir;
    if (!dir.isValid()) {
        Validacion v;
        v.lineas << QStringLiteral("No se puede crear una carpeta temporal");
        return v;
    }
    const QString ruta = QDir(dir.path()).filePath(d.nombre + QStringLiteral(".xml"));
    const QString e = exporta(d, c, ruta);
    if (!e.isEmpty()) {
        Validacion v;
        v.lineas << e;
        return v;
    }
    return valida_fichero(mcu_sim, ruta);
}

} // namespace mcusim::build
