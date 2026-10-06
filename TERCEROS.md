# Software de terceros en los paquetes de mcu-sim-gui

`mcu-sim-gui` es software libre bajo la **AGPLv3** (`LICENSE`). Los paquetes que
publica la integración continua —el `.zip` de Windows, el `.AppImage` de Linux y
la `.app` de macOS— llevan además, al lado del ejecutable, **las bibliotecas de
Qt 6** y las que Qt necesita, para que funcionen sin instalar nada. Este fichero
dice qué son y con qué licencia se redistribuyen.

## Qt 6

- **Qué**: Qt Core, Gui, Widgets y Network, y sus complementos (la plataforma
  gráfica, los estilos, los formatos de imagen); y, desde las ilustraciones de
  las placas, **Qt SVG** (`Qt6Svg` y `Qt6SvgWidgets`), que se usa como
  biblioteca y no como complemento.
- **Licencia**: **GNU LGPL versión 3** (`licencias/LGPL-3.0-only.txt`, que se
  apoya en `licencias/GPL-3.0-only.txt`).
- **De dónde sale cada paquete**: Windows, del Qt de MSYS2
  (`mingw-w64-x86_64-qt6-base` y `-qt6-svg`); Linux, del de Ubuntu 24.04
  (`qt6-base-dev` y `qt6-svg-dev`);
  macOS, del de Homebrew (`qt`). Los tres son compilaciones sin modificar de
  Qt, y su código fuente está en <https://download.qt.io/official_releases/qt/>
  y en los repositorios de esas tres distribuciones.
- **Cómo cumple el paquete la LGPL**: Qt va **enlazado dinámicamente** y sus
  bibliotecas viajan **como ficheros aparte** —las `Qt6*.dll` de Windows, las
  `libQt6*.so` dentro del AppImage, los `Qt*.framework` dentro de la `.app`—,
  así que cualquiera puede sustituirlas por otra compilación de Qt compatible.
  El código fuente de `mcu-sim-gui`, y con él todo lo necesario para volver a
  enlazarlo, es este repositorio.

## Lo que Qt arrastra

Junto a Qt viajan las bibliotecas de las que dependen sus compilaciones en cada
distribución —según la plataforma, la de expresiones regulares (PCRE2, BSD), la
de compresión (zlib, zstd, brotli), la de tipos de letra (FreeType, HarfBuzz),
la de PNG (libpng), ICU, `double-conversion` y, en Windows, las de ejecución de
MinGW-w64 (`libgcc`, `libstdc++` con su excepción de biblioteca de ejecución, y
`libwinpthread`, MIT/ZPL)—. Todas son de licencias libres que permiten
redistribuirlas en binario, y sus textos completos acompañan a los paquetes de
la distribución de la que salen.

## Lo que NO va dentro

**`mcu-sim`**. La ventana lo lanza, pero es otro programa, de otro repositorio,
con sus propios ejecutables y su propio `TERCEROS.md` (SystemC va ahí).
