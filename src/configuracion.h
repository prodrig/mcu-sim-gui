// =============================================================================
// configuracion.h — lo que la ventana recuerda entre una vez y otra, en JSON
//
// Fase 7 del plan. Con `QJsonDocument`, que Qt trae: ninguna dependencia nueva.
// Lo que se guarda:
//
//   * dónde está `mcu-sim` y desde qué directorio se lanza;
//   * los ARGUMENTOS, como un mapa por nombre —"placa", "firmware", "--ms",
//     "--ondas"...—, igual que los lleva el diálogo. Sin una lista de opciones
//     escrita aquí: la lista la dice `mcu-sim --argumentos`, y lo que se
//     guarda es solo lo que alguien puso;
//   * lo que se escribió a mano, tal cual;
//   * el host y el puerto donde escucha la ventana, que son una PREFERENCIA:
//     si el puerto está cogido se usa otro, sin decir nada;
//   * el ritmo con el que arrancar y el periodo de las instantáneas.
//
// Las rutas relativas del ejecutable y del directorio de trabajo se toman
// respecto al fichero de configuración; la placa y el firmware, respecto al
// directorio de trabajo, que es como las lee `mcu-sim`. La plantilla versionada
// es `config.ejemplo.json`.
// =============================================================================
#ifndef MCU_SIM_GUI_CONFIGURACION_H
#define MCU_SIM_GUI_CONFIGURACION_H

#include <QString>

#include "argumentos.h"
#include "protocolo.h"

namespace mcusim {

struct Configuracion {
    QString    ruta;              // de dónde se leyó, y adónde se guarda
    bool       existia = false;   // si `ruta` existía al leerla
    // Dónde se buscó antes de quedarse con `ruta`: para decirlo cuando no se
    // encontró ninguna y el diálogo sale vacío.
    QStringList buscadas;
    QString    ejecutable;        // relativa al fichero, o absoluta
    QString    directorio;        // ídem
    ValoresCli argumentos;
    QString    a_mano;
    QString    host = QStringLiteral("localhost");
    quint16    puerto = proto::PUERTO_OMISION;
    QString    ritmo = QStringLiteral("real");     // real, mitad, libre, demanda
    double     periodo_ms = 1000.0 / 60.0;         // de las instantáneas, SIMULADO

    // Lee `ruta`. Si no existe, la configuración por omisión con esa ruta —para
    // guardarla ahí— y true; false solo si existe y no se puede leer.
    static bool lee(const QString& ruta, Configuracion& c, QString& error);
    bool guarda(QString& error) const;

    // Una ruta de la configuración, absoluta: respecto a donde está el fichero.
    QString absoluta(const QString& r) const;
    QString ejecutable_absoluto() const { return absoluta(ejecutable); }
    QString directorio_absoluto() const { return absoluta(directorio); }

    // Dónde buscarla si nadie dice otra: `config.json` en el directorio actual
    // si lo hay, y si no, en el de configuración del usuario.
    static QString ruta_por_omision();
    // Esos dos sitios, en ese orden.
    static QStringList rutas_candidatas();
};

} // namespace mcusim

#endif // MCU_SIM_GUI_CONFIGURACION_H
