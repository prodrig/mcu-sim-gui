#!/usr/bin/env bash
# =============================================================================
# ci/empaqueta-macos.sh — mcu-sim-gui.app con Qt dentro, en un .zip
#
#   ci/empaqueta-macos.sh [BUILD] [SALIDA]
#
# CMakeLists.txt hace de mcu-sim-gui una .app (MACOSX_BUNDLE). `macdeployqt`
# le mete dentro los frameworks de Qt y las bibliotecas de Homebrew de las que
# dependen, y reescribe las rutas para que apunten dentro del paquete; los dos
# complementos que hacen falta se le dan a mano, y lo que deja a medias se
# remata con install_name_tool. Luego se COMPRUEBA que nada apunta fuera de la
# .app -en la maquina del alumno no hay Homebrew, o hay otro Qt- y que
# arranca.
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

# --- 1. Los complementos: SOLO los que hacen falta --------------------------
# El Qt de Homebrew viene partido en una formula por modulo (qtbase, qtsvg,
# qtpdf, qtvirtualkeyboard...), y macdeployqt, si se le deja, mete TODOS los
# complementos que encuentra: el de SVG, el de PDF, el teclado virtual, WebP...
# Cada uno arrastra un framework de OTRA formula que macdeployqt no sabe
# encontrar -«Cannot resolve rpath @rpath/QtPdf.framework...», en el CI del
# 2026-10-04-, y deja el paquete a medias. Esta ventana necesita dos: la
# plataforma (cocoa) y el estilo de macOS. Se copian a mano, y macdeployqt se
# ejecuta con -no-plugins y con -executable= para cada uno, que es lo que le
# hace desplegar sus dependencias y reescribir sus rutas.
PLUG=$("$QT/bin/qtpaths" --query QT_INSTALL_PLUGINS 2>/dev/null ||
       "$QT/bin/qmake" -query QT_INSTALL_PLUGINS 2>/dev/null || true)
[ -d "$PLUG/platforms" ] || PLUG="$QT/share/qt/plugins"
echo "  complementos de Qt en $PLUG"
mkdir -p "$APP/Contents/PlugIns/platforms" "$APP/Contents/PlugIns/styles"
cp "$PLUG/platforms/libqcocoa.dylib" "$APP/Contents/PlugIns/platforms/"
[ -f "$PLUG/styles/libqmacstyle.dylib" ] &&
    cp "$PLUG/styles/libqmacstyle.dylib" "$APP/Contents/PlugIns/styles/"
extra=()
while IFS= read -r f; do extra+=("-executable=$f"); done \
    < <(find "$APP/Contents/PlugIns" -name '*.dylib')
# Sin qt.conf, Qt buscaria los complementos donde estaban al compilar
mkdir -p "$APP/Contents/Resources"
printf '[Paths]\nPlugins = PlugIns\n' > "$APP/Contents/Resources/qt.conf"

"$QT/bin/macdeployqt" "$APP" -no-plugins -always-overwrite -verbose=1 "${extra[@]}"

# --- 2. Lo que macdeployqt deja a medias, rematado a mano --------------------
# Con el Qt partido en formulas, alguna biblioteca de Homebrew (en el CI,
# libbrotlicommon, que pide libbrotlidec por @rpath) acaba copiada en
# Frameworks pero con su NOMBRE -su LC_ID_DYLIB- y alguna referencia
# apuntando todavia a /opt/homebrew o /usr/local. Se corrige con
# install_name_tool: cada referencia absoluta a Homebrew cuya biblioteca este
# en Frameworks pasa a @rpath/<nombre>, cada dylib de Frameworks se llama
# @rpath/<nombre>, y el ejecutable lleva el rpath que lo resuelve. Lo que no
# este en Frameworks no se puede arreglar aqui: lo cazara la comprobacion.
FW="$APP/Contents/Frameworks"
# Todo binario Mach-O de la .app, por su contenido y no por su nombre ni su
# bit de ejecucion: el binario de un framework no se llama .dylib, y lo que
# viene de Homebrew puede no tener el bit puesto.
macho() { find "$APP" -type f | while IFS= read -r f; do
              case "$(file -b "$f")" in *Mach-O*) echo "$f" ;; esac; done; }
# Lo que viene de Homebrew es de solo lectura, e install_name_tool escribe
chmod -R u+w "$APP"
while IFS= read -r f; do
    while IFS= read -r dep; do
        b=$(basename "$dep")
        if [ -e "$FW/$b" ]; then
            install_name_tool -change "$dep" "@rpath/$b" "$f"
            echo "    $(basename "$f"): $dep -> @rpath/$b"
        fi
    done < <(otool -L "$f" | tail -n +2 | awk '{print $1}' | grep -E '^(/opt/homebrew|/usr/local)/' || true)
done < <(macho)
for d in "$FW"/*.dylib; do
    [ -e "$d" ] && install_name_tool -id "@rpath/$(basename "$d")" "$d"
done
EXE="$APP/Contents/MacOS/mcu-sim-gui"
otool -l "$EXE" | grep -q '@executable_path/../Frameworks' ||
    install_name_tool -add_rpath '@executable_path/../Frameworks' "$EXE"

# --- 3. Que nada apunte fuera del paquete -------------------------------------
# Ni por ruta absoluta a Homebrew, ni por un @rpath que no este dentro
fuera=0
while IFS= read -r f; do
    if otool -L "$f" | tail -n +2 | grep -E '/opt/homebrew|/usr/local' ; then
        echo "    ^ en $f"
        fuera=1
    fi
    while IFS= read -r dep; do
        rel=${dep#@rpath/}
        if [ ! -e "$FW/$rel" ]; then
            echo "    $dep, que no esta en Frameworks"
            echo "    ^ en $f"
            fuera=1
        fi
    done < <(otool -L "$f" | tail -n +2 | awk '{print $1}' | grep '^@rpath/' || true)
done < <(macho)
[ "$fuera" = 0 ] || { echo "  [FALLO] hay binarios que apuntan fuera de la .app"; exit 1; }
echo "  [OK  ] nada dentro de la .app apunta fuera de ella"

# --- 4. La firma -----------------------------------------------------------------
# macdeployqt intenta firmar el mismo y, con lo de arriba, su firma ya no vale:
# install_name_tool la rompe. Se firma al final, una vez, todo.
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
