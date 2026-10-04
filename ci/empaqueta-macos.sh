#!/usr/bin/env bash
# =============================================================================
# ci/empaqueta-macos.sh — mcu-sim-gui.app con Qt dentro, en un .zip
#
#   ci/empaqueta-macos.sh [BUILD] [SALIDA]
#
# `qt_standard_project_setup()` ya hace de mcu-sim-gui una .app. `macdeployqt`
# le mete dentro los frameworks de Qt, sus complementos y las bibliotecas de
# Homebrew de las que dependen, y reescribe las rutas para que apunten dentro
# del paquete. Luego se COMPRUEBA que ninguna se ha quedado apuntando a
# Homebrew -en la maquina del alumno no hay Homebrew, o hay otro Qt- y que la
# .app arranca.
#
# SIN FIRMA de Apple: como mcu-sim, la descarga queda en cuarentena y hay que
# quitarsela (`xattr -dr com.apple.quarantine mcu-sim-gui.app`). Lo que si se
# hace es la firma AD HOC, que en Apple Silicon no es opcional: macdeployqt
# modifica los binarios, eso invalida su firma, y un binario arm64 con la firma
# rota no lo deja ni arrancar el sistema.
# =============================================================================
set -euo pipefail

BUILD=${1:-build}
SALIDA=${2:-mcu-sim-gui-macos-$(uname -m).zip}
APP="$BUILD/mcu-sim-gui.app"
[ -d "$APP" ] || { echo "  [FALLO] no hay $APP"; exit 1; }

QT=${QT_PREFIX:-$(brew --prefix qt)}
"$QT/bin/macdeployqt" "$APP" -always-overwrite -verbose=1

# --- Que nada apunte fuera del paquete -------------------------------------
fuera=0
while IFS= read -r f; do
    if file "$f" | grep -q 'Mach-O'; then
        if otool -L "$f" | tail -n +2 | grep -E '/opt/homebrew|/usr/local' ; then
            echo "    ^ en $f"
            fuera=1
        fi
    fi
done < <(find "$APP" -type f \( -perm -u+x -o -name '*.dylib' \))
[ "$fuera" = 0 ] || { echo "  [FALLO] hay binarios que siguen apuntando a Homebrew"; exit 1; }
echo "  [OK  ] nada dentro de la .app apunta a Homebrew"

codesign --force --deep --sign - "$APP"
codesign --verify --deep "$APP"
echo "  [OK  ] firmada ad hoc, y la firma se verifica"

# --- Que arranca ----------------------------------------------------------------
# Cinco segundos: si sale antes, es que se ha caido.
"$APP/Contents/MacOS/mcu-sim-gui" & pid=$!
sleep 5
if ! kill -0 "$pid" 2>/dev/null; then
    wait "$pid" || true
    echo "  [FALLO] la .app termino sola"
    exit 1
fi
kill "$pid"; wait "$pid" 2>/dev/null || true
echo "  [OK  ] la .app arranca y sigue abierta"

# --- El paquete --------------------------------------------------------------------
# `ditto` y no `zip`: conserva los enlaces simbolicos de los frameworks y los
# permisos, que un zip corriente rompe.
rm -rf paquete
mkdir -p paquete
ditto "$APP" paquete/mcu-sim-gui.app
cp README.md LICENSE TERCEROS.md config.ejemplo.json paquete/
cp -r empaquetado/licencias paquete/
rm -f "$SALIDA"
ditto -c -k --sequesterRsrc paquete "$SALIDA"
unzip -l "$SALIDA" | tail -3
