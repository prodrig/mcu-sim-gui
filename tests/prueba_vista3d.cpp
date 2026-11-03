// =============================================================================
// prueba_vista3d.cpp — la vista 3D (F8 de la CNC de mcu-sim, su
// doc/analisis_cnc.md §9): `escena3d.h` y, si se ha compilado con Qt Quick 3D,
// `vista3d.h`
//
//   D1  un <vista3d> en T_PLACA, sin mcu-sim: lo que se lee, lo que se
//       resuelve contra el catálogo y lo que no -un observable que la pieza no
//       tiene, una imagen que no existe- y se dice
//   D2  la escena se mueve con las muestras: cada observable por su vector, y
//       el giro de un cilindro
//   D3  la imagen de alturas: centésimas de mm en el rojo y el verde, la fila
//       de arriba la de más Y, y la fresa en vacío en el azul
//   D4  con Qt Quick 3D, la escena dibujada: una imagen que no es un color
//       liso. Si no hay con qué dibujar -sin OpenGL-, se dice y se salta
//   D5  con MCU_SIM y MCU_SIM_SRC, la fresadora 3020 de verdad, con la ventana
//       entera: un bolsillo de 20 x 10 x 2 mm, y la superficie de la vista 3D
//       con su hueco, donde va y tan hondo como es
//
// Con CAPTURAS=directorio, deja ahí las imágenes de D4 y D5.
// =============================================================================
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QPushButton>
#include <QAction>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>

#include <cmath>
#include <cstdio>

#include "comun.h"
#include "escena3d.h"
#include "placa.h"
#include "sesion.h"
#include "ventana_principal.h"
#ifdef MCU_SIM_GUI_3D
#include "vista3d.h"
#endif

using namespace mcusim;
using prueba::comprueba;
using prueba::espera;

namespace {

// Una pieza del catálogo, con sus observables -los id_obs, consecutivos- y sus
// imágenes
PiezaGui pieza(const QString& id, const QStringList& obs, int& id_obs,
               const QStringList& imagenes = {}, int ancho = 0, int alto = 0)
{
    PiezaGui p;
    p.id = id;
    p.id_local = id.contains(QLatin1Char('/')) ? id.section(QLatin1Char('/'), 1) : id;
    for (int k = 0; k < obs.size(); ++k) {
        ObservableGui o;
        o.idx = k;
        o.id_obs = id_obs++;
        o.nombre = obs[k];
        p.observables.push_back(o);
    }
    for (int k = 0; k < imagenes.size(); ++k) {
        ImagenGui im;
        im.idx = k;
        im.id_obs = 1000 + id_obs++;
        im.nombre = imagenes[k];
        im.ancho = ancho;
        im.alto = alto;
        p.imagenes.push_back(im);
    }
    return p;
}

int colores(const QImage& i)
{
    QSet<QRgb> c;
    for (int y = 0; y < i.height(); y += 3)
        for (int x = 0; x < i.width(); x += 3) c.insert(i.pixel(x, y) & 0xF0F0F0u);
    return int(c.size());
}

QString lee(const QString& ruta)
{
    QFile f(ruta);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}
bool escribe(const QString& ruta, const QString& t)
{
    QFile f(ruta);
    return f.open(QIODevice::WriteOnly) && f.write(t.toUtf8()) == t.toUtf8().size();
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    const QString capturas = qEnvironmentVariable("CAPTURAS");

    // -------------------------------------------------------------------------
    std::printf("D1 Un <vista3d> en T_PLACA, contra el catalogo\n");
    int id = 0;
    QVector<PiezaGui> cat;
    cat << pieza("M/CX", {"posicion", "velocidad"}, id)
        << pieza("M/HERR", {"x", "y", "z"}, id)
        << pieza("M/HX", {"angulo", "rpm"}, id)
        << pieza("M/PZA", {"quitado", "mesa"}, id, {"pieza", "alturas"}, 4, 3);
    const QByteArray xml = R"(<sistema nombre="prueba">
  <placa id="M" nombre="fresadora" piezas="4">
    <vista3d camara="500 -500 400" mira="150 0 40">
      <solido forma="caja" tam="400 300 20" en="150 0 -10" color="#37474f"/>
      <solido pieza="CX" forma="caja" tam="90 12 110" en="0 48 140" observable="posicion" vector="1 0 0"/>
      <solido pieza="HERR" forma="cilindro" tam="45 90" eje="z" en="0 0 67" color="#eceff1" observable="x z" vector="1 0 0  0 0 1"/>
      <solido pieza="HX" forma="cilindro" tam="8 400" eje="x" en="150 58 140" gira="angulo"/>
      <solido pieza="HX" forma="cilindro" tam="8 400" eje="x" en="150 58 140" gira="giro"/>
      <solido pieza="CX" forma="caja" tam="10 10 10" en="0 0 0" observable="nada" vector="1 0 0"/>
      <superficie pieza="PZA" imagen="alturas" tam="100 60" en="100 70 0" color="#c8a27a" observable="mesa" vector="0 -1 0"/>
      <superficie pieza="PZA" imagen="profundidad" tam="10 10" en="0 0 0"/>
    </vista3d>
  </placa>
</sistema>)";
    PlacaGui placa;
    QString error;
    comprueba(junta_placa(xml, cat, placa, error) && placa.vistas3d.size() == 1 &&
                  placa.vistas3d[0].placa == "M" && placa.vistas3d[0].elementos.size() == 8,
              "T_PLACA con un <vista3d> en su <placa id=\"M\">: ocho elementos " +
                  error.toStdString());
    Escena3d esc(placa);
    comprueba(esc.solidos.size() == 6 && esc.superficies.size() == 2 && esc.hay_camara &&
                  std::abs(esc.camara.x - 500) < 1e-9 && std::abs(esc.mira.z - 40) < 1e-9,
              "seis solidos, dos superficies, y la camara que dice la placa");
    const Solido3d& herr = esc.solidos[2];
    comprueba(herr.cilindro && herr.eje == 'z' && herr.obs.size() == 2 &&
                  herr.pieza == "M/HERR" && esc.solidos[3].gira >= 0,
              "el cabezal, un cilindro que mueven x y z de M/HERR; el husillo gira con su angulo");
    const QString av = esc.avisos.join(QStringLiteral(" | "));
    comprueba(esc.avisos.size() == 3 && av.contains("giro") && av.contains("nada") &&
                  av.contains("profundidad") && esc.superficies[1].id_img < 0,
              "y lo que no casa, dicho: un giro, un observable y una imagen que la pieza no "
              "tiene (" + av.toStdString() + ")");
    const QVector<quint16> obs = esc.observados(false), con = esc.observados(true);
    comprueba(obs.size() == 5 && con.size() == 6 &&
                  con.contains(quint16(esc.superficies[0].id_img)),
              "lo que pide: cinco numericos -posicion, x, z, angulo y mesa- y, al verse, la "
              "imagen de alturas");

    // -------------------------------------------------------------------------
    std::printf("D2 La escena se mueve con las muestras\n");
    int movidas = 0;
    QObject::connect(&esc, &Escena3d::movida, [&] { ++movidas; });
    const PiezaGui& pherr = cat[1];
    esc.pon_valor(quint16(cat[0].observables[0].id_obs), 120.f);       // CX posicion
    esc.pon_valor(quint16(pherr.observables[0].id_obs), 120.f);        // HERR x
    esc.pon_valor(quint16(pherr.observables[2].id_obs), 30.f);         // HERR z
    esc.pon_valor(quint16(cat[2].observables[0].id_obs), 90.f);        // HX angulo
    esc.pon_valor(quint16(pherr.observables[1].id_obs), 55.f);         // HERR y: no es suyo
    const Vec3 pc = esc.posicion(esc.solidos[1]), ph = esc.posicion(herr);
    comprueba(std::abs(pc.x - 120) < 1e-9 && std::abs(pc.z - 140) < 1e-9 &&
                  std::abs(ph.x - 120) < 1e-9 && std::abs(ph.z - 97) < 1e-9 &&
                  std::abs(esc.giro(esc.solidos[3]) - 90) < 1e-9 && movidas == 4,
              "el carro X a 120, el cabezal a (120, 0, 97) con x y z, el husillo girado 90 "
              "grados; y la y de HERR, que no mueve nada, no avisa");

    // -------------------------------------------------------------------------
    std::printf("D3 La imagen de alturas\n");
    {
        // 4 x 3: la fila de arriba es la de mas Y. Doce mm todo, salvo un hueco
        // de 2 mm en (1, 2) -arriba- y la mesa en (3, 0) -abajo-, y la fresa en
        // vacio sobre (0, 1)
        QByteArray rgb(4 * 3 * 3, 0);
        auto pon = [&](int fila, int col, int centesimas, bool paso) {
            const int q = 3 * (fila * 4 + col);
            rgb[q] = char(centesimas >> 8);
            rgb[q + 1] = char(centesimas & 0xFF);
            rgb[q + 2] = char(paso ? 255 : 0);
        };
        for (int f = 0; f < 3; ++f)
            for (int c = 0; c < 4; ++c) pon(f, c, 1200, false);
        pon(0, 1, 1000, false);
        pon(2, 3, 0, false);
        pon(1, 0, 1200, true);
        int llegadas = 0;
        QObject::connect(&esc, &Escena3d::superficie, [&](int k) { llegadas += k == 0; });
        esc.pon_imagen(quint16(esc.superficies[0].id_img), 4, 3, rgb);
        const Superficie3d& s = esc.superficies[0];
        comprueba(llegadas == 1 && s.nx == 4 && s.ny == 3 &&
                      std::abs(s.alturas[2 * 4 + 1] - 10.f) < 1e-6 &&
                      std::abs(s.alturas[0 * 4 + 3] - 0.f) < 1e-6 &&
                      std::abs(s.alturas[1 * 4 + 0] - 12.f) < 1e-6 && s.paso[1 * 4 + 0] &&
                      !s.paso[0] && std::abs(s.maxima - 12.f) < 1e-6,
                  "12 mm, un hueco de 2 arriba, la mesa abajo a la derecha y la fresa en "
                  "vacio: la fila j = 0 es la de menos Y");
        esc.pon_valor(quint16(cat[3].observables[1].id_obs), 70.f);    // PZA mesa
        const Vec3 p = esc.posicion(s);
        comprueba(std::abs(p.x - 100) < 1e-9 && std::abs(p.y - 0) < 1e-9,
                  "y la superficie va con la mesa: con la Y del carro a 70, su esquina en y = 0");
    }

    // -------------------------------------------------------------------------
    std::printf("D4 La escena, dibujada con Qt Quick 3D\n");
#ifdef MCU_SIM_GUI_3D
    {
        Vista3d v(&esc);
        v.resize(800, 600);
        v.show();
        QImage img;
        espera([&] {
            img = v.captura();
            return v.error().isEmpty() && !img.isNull() && colores(img) > 20;
        }, 4000);
        if (!v.error().isEmpty() || img.isNull() || colores(img) <= 1) {
            std::printf("  [SALTA] no hay con que dibujar en 3D aqui (%s)\n",
                        v.error().isEmpty() ? "una imagen lisa" : v.error().toUtf8().constData());
        } else {
            comprueba(colores(img) > 20,
                      "la escena se ve: " + std::to_string(colores(img)) + " colores");
            if (!capturas.isEmpty()) img.save(QDir(capturas).filePath("vista3d_prueba.png"));
        }
    }
#else
    std::printf("  [SALTA] compilada sin Qt Quick 3D\n");
#endif

    // -------------------------------------------------------------------------
    const QString sim = qEnvironmentVariable("MCU_SIM");
    const QString src = qEnvironmentVariable("MCU_SIM_SRC");
    if (sim.isEmpty() || src.isEmpty()) {
        std::printf("D5 [SALTA] sin MCU_SIM y MCU_SIM_SRC, la fresadora de verdad no\n");
        return prueba::resultado();
    }
    std::printf("D5 La fresadora 3020 de verdad: un bolsillo, en 3D\n");
    {
        // Como `corta()` de verif/gui/cnc.py de mcu-sim: el sistema con su
        // firmware, el G-code tecleado por el VCP, en memoria, y los carros
        // donde empieza el programa
        QTemporaryDir tmp;
        const QString pl = QDir(src).filePath("placas");
        const double r = 3.175 / 2, x0 = 120 + r, x1 = 140 - r, y0 = 85 + r, y1 = 95 - r;
        QStringList g{QStringLiteral("G92 X%1 Y%2 Z20").arg(x0, 0, 'f', 4).arg(y0, 0, 'f', 4),
                      "M3 S1000", "G4 P1", "G1 Z13 F120"};
        double y = y0;
        int d = 1;
        for (;;) {
            g << QStringLiteral("G1 X%1 F600").arg(d > 0 ? x1 : x0, 0, 'f', 4);
            d = -d;
            if (y >= y1 - 1e-9) break;
            y = std::min(y + 1.5, y1);
            g << QStringLiteral("G1 Y%1").arg(y, 0, 'f', 4);
        }
        g << QStringLiteral("G1 X%1 Y%2").arg(x0, 0, 'f', 4).arg(y0, 0, 'f', 4)
          << QStringLiteral("G1 X%1").arg(x1, 0, 'f', 4) << QStringLiteral("G1 Y%1").arg(y1, 0, 'f', 4)
          << QStringLiteral("G1 X%1").arg(x0, 0, 'f', 4) << QStringLiteral("G1 Y%1").arg(y0, 0, 'f', 4)
          << "G0 Z20" << "M5";
        QString n = lee(QDir(pl).filePath("nucleo_f446re.xml"));
        n.replace("host=\"rfc2217:3355\"\n              baudios=\"host\"",
                  "host=\"memoria\" guion_ms=\"20\" guion=\"" + g.join("\\n") +
                      "\\n\"\n              baudios=\"115200\"");
        QString m = lee(QDir(pl).filePath("fresadora_3020.xml"));
        m.replace("posicion_mm=\"150\"", QStringLiteral("posicion_mm=\"%1\"").arg(x0, 0, 'f', 4));
        m.replace("posicion_mm=\"100\"", QStringLiteral("posicion_mm=\"%1\"").arg(y0, 0, 'f', 4));
        m.replace("posicion_mm=\"50\" vertical", "posicion_mm=\"20\" vertical");
        QString x = lee(QDir(pl).filePath("cnc_3020.xml"));
        x.replace("fichero=\"nucleo_f446re.xml\"", "fichero=\"" + tmp.filePath("nucleo.xml") + "\"");
        x.replace("fichero=\"fresadora_3020.xml\"", "fichero=\"" + tmp.filePath("fresadora.xml") + "\"");
        for (const char* f : {"cnc_shield_v3", "pololu_a4988", "modulo_mosfet", "motor_775"})
            x.replace(QStringLiteral("fichero=\"%1.xml\"").arg(f),
                      QStringLiteral("fichero=\"%1/%2.xml\"").arg(pl, f));
        x.replace(QRegularExpression("(<mcu ref=\"N/u0\" firmware=)\"[^\"]*\""),
                  "\\1\"" + QDir(src).filePath("verif/fw/cnc/cnc.bin") + "\"");
        const bool listos = !n.isEmpty() && !m.isEmpty() && !x.isEmpty() &&
                            escribe(tmp.filePath("nucleo.xml"), n) &&
                            escribe(tmp.filePath("fresadora.xml"), m) &&
                            escribe(tmp.filePath("cnc.xml"), x);
        VentanaPrincipal v(quint16(0));
        v.resize(1000, 700);
        v.show();
        QProcess p;
        p.setWorkingDirectory(src);
        p.setProcessChannelMode(QProcess::MergedChannels);
        if (listos)
            p.start(sim, {tmp.filePath("cnc.xml"), "--ms=23000",
                          "--gui", QStringLiteral("127.0.0.1:%1").arg(v.sesion().puerto())});
        auto* arrancar = v.findChild<QPushButton*>("arrancar");
        auto* ritmo = v.findChild<QComboBox*>("ritmo");
        auto* ver3d = v.findChild<QAction*>("ver_3d");
        comprueba(listos && espera([&] { return arrancar && arrancar->isEnabled(); }, 30000) &&
                      v.escena3d() && ver3d && ver3d->isEnabled(),
                  "la ventana con la 3020: Vista 3D, porque la fresadora declara su <vista3d>");
        if (v.escena3d() && ver3d) {
            ver3d->setChecked(true);
            ritmo->setCurrentIndex(ritmo->findText("libre"));
            arrancar->click();
            const bool acabo = espera([&] { return p.state() == QProcess::NotRunning; }, 600000);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 300);
            const Superficie3d* s = v.escena3d()->superficies.isEmpty()
                                        ? nullptr : &v.escena3d()->superficies[0];
            // El bolsillo: X 120-140 e Y 85-95 de la maquina; en la pieza, 20-40 y 15-25
            float minimo = 1e9f, fondo_fuera = 1e9f;
            double u0 = 1e9, u1 = -1e9, w0 = 1e9, w1 = -1e9;
            if (s && s->nx > 0)
                for (int j = 0; j < s->ny; ++j)
                    for (int i = 0; i < s->nx; ++i) {
                        const float h = s->alturas[j * s->nx + i];
                        const double u = (i + 0.5) * s->largo / s->nx, w = (j + 0.5) * s->ancho / s->ny;
                        if (h < s->maxima - 0.5f) {
                            minimo = std::min(minimo, h);
                            u0 = std::min(u0, u); u1 = std::max(u1, u);
                            w0 = std::min(w0, w); w1 = std::max(w1, w);
                        } else {
                            fondo_fuera = std::min(fondo_fuera, h);
                        }
                    }
            comprueba(acabo && s && s->nx == 400 && s->ny == 240 &&
                          std::abs((s->maxima - minimo) - 2.0) < 0.03 &&
                          std::abs(u0 - 20.125) < 1e-6 && std::abs(u1 - 39.875) < 1e-6 &&
                          std::abs(w0 - 15.125) < 1e-6 && std::abs(w1 - 24.875) < 1e-6,
                      QStringLiteral("la superficie de la vista 3D, con su hueco: %1 mm de hondo, "
                                     "de X %2 a %3 y de Y %4 a %5 en la pieza")
                          .arg(s ? s->maxima - minimo : 0, 0, 'f', 3).arg(u0, 0, 'f', 3)
                          .arg(u1, 0, 'f', 3).arg(w0, 0, 'f', 3).arg(w1, 0, 'f', 3).toStdString());
#ifdef MCU_SIM_GUI_3D
            Vista3d* v3 = v.ventana3d() ? v.ventana3d()->findChild<Vista3d*>() : nullptr;
            if (v3 && v3->error().isEmpty() && !capturas.isEmpty()) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 500);
                v3->captura().save(QDir(capturas).filePath("vista3d_bolsillo.png"));
            }
#endif
        }
        if (p.state() != QProcess::NotRunning) p.kill();
        p.waitForFinished(5000);
    }
    return prueba::resultado();
}
