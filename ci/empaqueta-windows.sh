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
#
# LA VISTA 3D, si el ejecutable la lleva (Qt Quick 3D, MCU_SIM_GUI_3D): sus
# MODULOS DE QML -QtQuick3D, QtQuick3D.Helpers y los de QtQuick que piden- los
# pone `windeployqt6` si se le dice donde estan los .qml (`--qmldir src`), en
# `qml/`; sus DLL las busca el paso 2 como las demas, y un `qt.conf` le dice a
# Qt que los busque ahi -sin el, los buscaria en el MSYS2 de quien compilo-.
# Se COMPRUEBA con `--comprueba-3d`, con el mismo PATH sin MSYS2, y mirando que
# las rutas de QML que dice son las del paquete.
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
# Con la vista 3D, si el ejecutable carga Qt6Quick3D.dll. La DLL que compila
# los sombreadores en Direct3D, d3dcompiler_47.dll, la tiene Windows 10 y
# posteriores: --no-system-d3d-compiler se queda
CON_3D=0
ldd "$EXE" | grep -qi 'Qt6Quick3D\.dll' && CON_3D=1
qml=()
[ "$CON_3D" = 1 ] && qml=(--qmldir src)
"$DEPLOY" --release --no-translations --no-system-d3d-compiler --no-opengl-sw \
          --compiler-runtime "${qml[@]}" paquete/mcu-sim-gui.exe
if [ "$CON_3D" = 1 ]; then
    for m in QtQuick3D QtQuick3D/Helpers QtQuick; do
        [ -f "paquete/qml/$m/qmldir" ] ||
            { echo "  [FALLO] windeployqt no ha puesto el modulo de QML $m en qml/"; exit 1; }
    done
    # Que Qt busque los modulos de QML en el paquete, y no en el prefijo con
    # el que se compilo (C:/msys64/mingw64/share/qt6/qml)
    printf '[Paths]\nPrefix = .\nPlugins = .\nQmlImports = qml\n' > paquete/qt.conf
    echo "  [OK  ] con la vista 3D: los modulos de QML en qml/, y un qt.conf que lo dice"
fi

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

# La vista 3D, cargada de verdad con ese mismo PATH: lo que falte -un modulo de
# QML, la DLL de un complemento de QML- solo se nota al cargar su escena
if [ "$CON_3D" = 1 ]; then
    rc=0
    # env y timeout por su ruta: con ese PATH no se encontrarian
    out=$(cd paquete && PATH="$sistema" /usr/bin/env -u QML2_IMPORT_PATH -u QML_IMPORT_PATH \
          /usr/bin/timeout 60 ./mcu-sim-gui.exe --comprueba-3d 2>&1) || rc=$?
    echo "$out"
    [ "$rc" = 0 ] || { echo "  [FALLO] la vista 3D no se carga desde el paquete ($rc)"; exit 1; }
    if echo "$out" | grep -i '^rutas de QML' | grep -Eiq 'msys64|mingw64'; then
        echo "  [FALLO] la vista 3D busca modulos de QML fuera del paquete"
        exit 1
    fi
    echo "  [OK  ] la vista 3D se carga con los modulos de QML del paquete"
fi

# --- El paquete --------------------------------------------------------------------
cp README.md LICENSE TERCEROS.md config.ejemplo.json paquete/
cp -r empaquetado/licencias paquete/
rm -f "$SALIDA"
(cd paquete && zip -qr "../$SALIDA" .)
unzip -l "$SALIDA" | tail -3
