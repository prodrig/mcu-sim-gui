#!/usr/bin/env bash
# =============================================================================
# ci/empaqueta-windows.sh — mcu-sim-gui.exe con todo lo que necesita, en un .zip
#
#   ci/empaqueta-windows.sh [BUILD] [SALIDA]        (en el shell MINGW64 de MSYS2)
#
# El plan lo dejo escrito en su §8.8, y el usuario lo sufrio en su propia
# maquina: un ejecutable de MSYS2 funciona en el shell MINGW64 y FUERA de el
# coge la DLL que encuentre primero en el PATH -«No se encuentra el punto de
# entrada hb_font_set_pterm en Qt6Gui.dll», porque habia OTRA libharfbuzz
# delante-. Windows busca las DLL primero en la carpeta del ejecutable, asi que
# la cura es que esten TODAS ahi:
#
#   1. `windeployqt6` pone las de Qt y sus complementos (la plataforma, los
#      estilos, los formatos de imagen);
#   2. pero NO las que Qt arrastra en MSYS2 -ICU, HarfBuzz, FreeType, zstd,
#      PCRE2, libpng...- ni siempre las de ejecucion de MinGW: esas se buscan
#      con `ldd` sobre todo lo que ya hay, y se repite hasta que no falte nada;
#   3. y se COMPRUEBA con un PATH que no tenga MSYS2, que es la unica prueba
#      que vale: la de la maquina del alumno.
# =============================================================================
set -euo pipefail

BUILD=${1:-build}
SALIDA=${2:-mcu-sim-gui-windows-x86_64.zip}
EXE="$BUILD/mcu-sim-gui.exe"
[ -f "$EXE" ] || { echo "  [FALLO] no hay $EXE"; exit 1; }

rm -rf paquete
mkdir -p paquete
cp "$EXE" paquete/

# --- 1. Qt --------------------------------------------------------------------
DEPLOY=$(command -v windeployqt6 || command -v windeployqt || true)
[ -n "$DEPLOY" ] || { echo "  [FALLO] no hay windeployqt6 (mingw-w64-x86_64-qt6-base)"; exit 1; }
"$DEPLOY" --release --no-translations --no-system-d3d-compiler --no-opengl-sw \
          --compiler-runtime paquete/mcu-sim-gui.exe

# --- 2. Lo que Qt arrastra en MSYS2 ---------------------------------------------
# `ldd` de MSYS2 da la ruta de cada DLL que carga. Lo que venga de /mingw64/bin
# se copia al lado del .exe; lo de Windows (C:\Windows\...) se queda donde
# esta, que es de todas las maquinas.
for vuelta in 1 2 3 4 5 6; do
    nuevas=0
    while IFS= read -r f; do
        while IFS= read -r d; do
            b=$(basename "$d")
            if [ ! -f "paquete/$b" ]; then
                cp "$d" paquete/
                echo "    + $b"
                nuevas=1
            fi
        done < <(ldd "$f" | awk '$3 ~ /^\/mingw64\/bin\// {print $3}')
    done < <(find paquete -iname '*.exe' -o -iname '*.dll')
    [ "$nuevas" = 0 ] && break
done
echo "  [OK  ] $(find paquete -iname '*.dll' | wc -l) DLL al lado del ejecutable"

# --- 3. La prueba: SIN MSYS2 en el PATH ---------------------------------------------
# Con un PATH que solo tiene Windows, nada puede venir de /mingw64: si `ldd`
# nombra alguna DLL de alli, o alguna que no encuentra, el paquete esta roto.
[ -f paquete/platforms/qwindows.dll ] ||
    { echo "  [FALLO] falta el complemento de plataforma platforms/qwindows.dll"; exit 1; }
sistema="$(cygpath -u "$SYSTEMROOT")/System32:$(cygpath -u "$SYSTEMROOT")"
deps=$(cd paquete && PATH="$sistema" /usr/bin/ldd ./mcu-sim-gui.exe)
echo "$deps"
if echo "$deps" | grep -Ei 'mingw64|not found|\?\?\?'; then
    echo "  [FALLO] con un PATH sin MSYS2, el ejecutable no encuentra lo suyo dentro del paquete"
    exit 1
fi
echo "  [OK  ] con un PATH sin MSYS2, todo lo que carga esta en el paquete o en Windows"

# --- El paquete --------------------------------------------------------------------
cp README.md LICENSE TERCEROS.md config.ejemplo.json paquete/
cp -r empaquetado/licencias paquete/
rm -f "$SALIDA"
(cd paquete && zip -qr "../$SALIDA" .)
unzip -l "$SALIDA" | tail -3
