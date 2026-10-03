#!/usr/bin/env bash
# =============================================================================
# ci/empaqueta-linux.sh — mcu-sim-gui en un AppImage, y el AppImage en un .tar.gz
#
#   ci/empaqueta-linux.sh [BUILD] [SALIDA]
#
# BUILD es el arbol de compilacion (por omision `build`) y SALIDA el .tar.gz
# (por omision `mcu-sim-gui-linux-x86_64.tar.gz`). Se ejecuta desde la raiz.
#
# UN APPIMAGE, Y NO EL EJECUTABLE A SECAS, porque el ejecutable a secas pide el
# Qt 6 del sistema, en una version que la distribucion del alumno puede no
# tener. El AppImage lleva Qt dentro -lo mete `linuxdeploy` con su complemento
# de Qt- y es UN fichero. Va dentro de un .tar.gz por la misma razon que en
# mcu-sim: el zip de upload-artifact, y muchas descargas, PIERDEN EL BIT DE
# EJECUCION, y un AppImage sin el no arranca.
#
# Lo que NO lleva es glibc: un binario de Linux funciona hacia adelante, no
# hacia atras, y el suelo es la distribucion donde se construye. Este script
# lo dice al final.
# =============================================================================
set -euo pipefail

BUILD=${1:-build}
SALIDA=${2:-mcu-sim-gui-linux-x86_64.tar.gz}
HERR=${HERRAMIENTAS:-$PWD/.herramientas}
APPIMAGE=mcu-sim-gui-x86_64.AppImage

[ -x "$BUILD/mcu-sim-gui" ] || { echo "  [FALLO] no hay $BUILD/mcu-sim-gui"; exit 1; }

# --- linuxdeploy y su complemento de Qt ---------------------------------------
# Son AppImages ellos mismos. APPIMAGE_EXTRACT_AND_RUN hace que se desempaqueten
# en vez de montarse, que es lo que permite usarlos sin FUSE -en un contenedor o
# en un runner donde FUSE no esta-.
export APPIMAGE_EXTRACT_AND_RUN=1
mkdir -p "$HERR"
for h in linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
         linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage; do
    f="$HERR/$(basename "$h")"
    [ -s "$f" ] || curl -fsSL -o "$f" "https://github.com/$h"
    chmod +x "$f"
    sha256sum "$f"
done
export PATH="$HERR:$PATH"

# El qmake de Qt 6 es lo que el complemento usa para saber DONDE esta Qt
export QMAKE=${QMAKE:-$(command -v qmake6 || command -v qmake)}
"$QMAKE" -query QT_VERSION
# xcb es el que usa casi todo el mundo y el que el complemento mete solo; con
# wayland arranca de forma nativa en un escritorio Wayland, y offscreen es para
# poder comprobar aqui, sin pantalla, que el AppImage arranca.
export EXTRA_PLATFORM_PLUGINS="libqwayland-generic.so;libqoffscreen.so"
export OUTPUT=$APPIMAGE LDAI_OUTPUT=$APPIMAGE

rm -rf AppDir "$APPIMAGE"
linuxdeploy-x86_64.AppImage --appdir AppDir \
    --executable "$BUILD/mcu-sim-gui" \
    --desktop-file empaquetado/mcu-sim-gui.desktop \
    --icon-file empaquetado/mcu-sim-gui.svg \
    --plugin qt --output appimage
[ -f "$APPIMAGE" ] || { echo "  [FALLO] linuxdeploy no ha dejado $APPIMAGE"; exit 1; }
chmod +x "$APPIMAGE"

# --- Que lleva Qt dentro, y que arranca ---------------------------------------
n=$(find AppDir -name 'libQt6*.so*' | wc -l)
[ "$n" -ge 4 ] || { echo "  [FALLO] el AppImage lleva $n bibliotecas de Qt"; exit 1; }
echo "  [OK  ] el AppImage lleva $n bibliotecas de Qt 6"
# Tres segundos con la plataforma `offscreen`: si sale antes es que se ha
# caido; `timeout` devuelve 124 solo si tuvo que matarlo.
rc=0
QT_QPA_PLATFORM=offscreen timeout 3 "./$APPIMAGE" || rc=$?
[ "$rc" = 124 ] || { echo "  [FALLO] el AppImage termino solo con codigo $rc"; exit 1; }
echo "  [OK  ] el AppImage arranca y sigue abierto"

# --- El suelo de glibc ----------------------------------------------------------
# `|| true`: con pipefail, un fichero que objdump no entiende tumbaria el paso
suelo=$( (find AppDir -type f \( -name '*.so*' -o -path '*/bin/*' \) \
              -exec objdump -T {} + 2>/dev/null || true) |
        grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1)
echo "  glibc minima: $suelo (la de la distribucion donde se ha construido)"

# --- El paquete -------------------------------------------------------------------
rm -rf paquete
mkdir -p paquete
cp "$APPIMAGE" paquete/
cp README.md LICENSE TERCEROS.md config.ejemplo.json paquete/
cp -r empaquetado/licencias paquete/
tar czf "$SALIDA" -C paquete .
tar tzvf "$SALIDA"
