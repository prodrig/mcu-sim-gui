#include "lanzador.h"

#include <QFileInfo>
#include <QProcessEnvironment>

#include "argumentos.h"

namespace mcusim {

namespace {
// Sin la cabecera de copyright de SystemC: sale por la salida de ERROR, y en la
// consola de la ventana parecería un fallo de cada arranque. Lo que la apaga es
// la variable que la propia biblioteca mira.
QProcessEnvironment entorno()
{
    QProcessEnvironment e = QProcessEnvironment::systemEnvironment();
    e.insert(QStringLiteral("SYSTEMC_DISABLE_COPYRIGHT_MESSAGE"), QStringLiteral("DISABLE"));
    return e;
}
} // namespace

Lanzador::Lanzador(QObject* padre) : QObject(padre)
{
    p_.setProcessEnvironment(entorno());
    connect(&p_, &QProcess::readyReadStandardOutput, this,
            [this] { lee(QProcess::StandardOutput, resto_out_, false); });
    connect(&p_, &QProcess::readyReadStandardError, this,
            [this] { lee(QProcess::StandardError, resto_err_, false); });
    connect(&p_, &QProcess::finished, this, [this](int codigo, QProcess::ExitStatus st) {
        lee(QProcess::StandardOutput, resto_out_, true);
        lee(QProcess::StandardError, resto_err_, true);
        emit termino(codigo, st == QProcess::CrashExit);
    });
    connect(&p_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            emit fallo(tr("no se puede lanzar '%1': %2").arg(p_.program(), p_.errorString()));
    });
}

Lanzador::~Lanzador()
{
    // Un hijo no sobrevive a su ventana: se quedaría simulando para nadie.
    if (corriendo()) {
        p_.kill();
        p_.waitForFinished(2000);
    }
}

QString Lanzador::resuelve(const QString& ejecutable)
{
    if (ejecutable.isEmpty()) return {};
    const QFileInfo f(ejecutable);
    if (f.isFile()) return f.absoluteFilePath();
    const QFileInfo g(ejecutable + QStringLiteral(".exe"));
    if (g.isFile()) return g.absoluteFilePath();
    return {};
}

bool Lanzador::lanza(const QString& ejecutable, const QString& directorio,
                     const QStringList& argumentos)
{
    if (corriendo()) return false;
    const QString exe = resuelve(ejecutable);
    if (exe.isEmpty()) {
        emit fallo(tr("no hay ningun ejecutable en '%1'").arg(ejecutable));
        return false;
    }
    if (!directorio.isEmpty() && !QFileInfo(directorio).isDir()) {
        emit fallo(tr("el directorio de trabajo '%1' no existe").arg(directorio));
        return false;
    }
    resto_out_.clear();
    resto_err_.clear();
    orden_ = como_texto(exe, argumentos);
    p_.setWorkingDirectory(directorio);
    p_.start(exe, argumentos);
    return true;
}

void Lanzador::detiene(int plazo_ms)
{
    if (!corriendo()) return;
    p_.terminate();            // en Windows no llega a un proceso de consola: se mata
    if (!p_.waitForFinished(plazo_ms)) {
        p_.kill();
        p_.waitForFinished(plazo_ms);
    }
}

void Lanzador::lee(QProcess::ProcessChannel canal, QByteArray& resto, bool vacia)
{
    p_.setReadChannel(canal);
    resto += p_.readAll();
    int i;
    while ((i = resto.indexOf('\n')) >= 0) {
        QByteArray l = resto.left(i);
        resto.remove(0, i + 1);
        if (l.endsWith('\r')) l.chop(1);
        emit linea(QString::fromLocal8Bit(l), canal == QProcess::StandardError);
    }
    if (vacia && !resto.isEmpty()) {
        emit linea(QString::fromLocal8Bit(resto), canal == QProcess::StandardError);
        resto.clear();
    }
}

QByteArray Lanzador::argumentos_de(const QString& ejecutable, const QString& directorio,
                                   QString& error, int plazo_ms)
{
    const QString exe = resuelve(ejecutable);
    if (exe.isEmpty()) {
        error = tr("no hay ningun ejecutable en '%1'").arg(ejecutable);
        return {};
    }
    QProcess p;
    p.setProcessEnvironment(entorno());
    if (!directorio.isEmpty() && QFileInfo(directorio).isDir()) p.setWorkingDirectory(directorio);
    p.start(exe, {QStringLiteral("--argumentos")});
    if (!p.waitForStarted(plazo_ms)) {
        error = tr("no se puede ejecutar '%1': %2").arg(exe, p.errorString());
        return {};
    }
    if (!p.waitForFinished(plazo_ms)) {
        p.kill();
        p.waitForFinished(1000);
        error = tr("'%1 --argumentos' no ha terminado en %2 s").arg(exe).arg(plazo_ms / 1000);
        return {};
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        error = tr("'%1 --argumentos' ha terminado con codigo %2: es un mcu-sim anterior a "
                   "la fase 7, o no es un mcu-sim")
                    .arg(exe).arg(p.exitCode());
        return {};
    }
    return p.readAllStandardOutput();
}

} // namespace mcusim
