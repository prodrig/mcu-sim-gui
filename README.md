# mcu-sim-gui

La **contraparte de visualización gráfica de
[`mcu-sim`](../mcu-sim)**, el modelo SystemC de microcontroladores.
`mcu-sim` simula; esto lo enseña y deja tocarlo.

> **Para usarlo**: la [guía de usuario](doc/guia%20de%20usuario.md) —elegir
> el circuito y el firmware, lanzar, depurar con GDB y el puerto serie, y
> todas las opciones de la línea de órdenes—.

> **Estado (2026-11-03).** Las **fases 0 a 7** del plan están hechas, y
> **de la 9, los ejecutables**; falta la **8**, grabar y reproducir sesiones.
> Lo que hay hoy:
>
> * **La ventana se construye sola** con la placa y el catálogo que le manda
>   `mcu-sim`: un recuadro por pieza, con sus patillas, sus indicadores y un
>   control por mando. **Nada simula hasta pulsar «Arrancar»**, y con la
>   simulación en marcha los indicadores enseñan lo que pasa: los LED, los
>   dos relojes, y abajo los avisos de la placa y del modelo.
> * **Los controles mandan**: un botón pulsa mientras está hundido —o hasta
>   volver a tocarlo, con el «switch» de al lado—, lo tocado antes de
>   arrancar se aplica en t = 0, y cada orden vuelve con su eco. Los
>   pulsadores rebotan, y el rebote se ajusta en marcha (plan §16).
> * **La ventana lleva la simulación**: el ritmo —tiempo real, a la mitad,
>   libre o a demanda—, Pausa / Sigue, Paso y Parar.
> * **Y lanza el modelo ella misma**: *Simulación ▸ Lanzar mcu-sim* (Ctrl+L)
>   abre un diálogo construido con lo que dice `mcu-sim --argumentos`, con el
>   firmware de cada MCU; lo que el simulador escribe sale en la pestaña
>   *mcu-sim*.
> * **Placas sin MCU**, con la sobrecorriente como alarma (plan §18), y
>   **varias placas enchufadas** —un `<sistema>`—, también en pila
>   (plan §19 y §20).
> * **La ilustración** (plan §21 a §46): cada placa con su dibujo, y sobre él
>   lo que pasa. Se ven:
>   * los LED que brillan y los botones que se hunden;
>   * los mandos que giran y el aspa de un servo;
>   * las pantallas;
>   * los displays —el de una fuente de laboratorio, las cotas de una
>     fresadora—;
>   * los carros que se desplazan y la pieza mecanizada.
>
>   Con `--edit`, además, se colocan las placas y se guarda en el XML.
> * **Otros chips**: los MSP432 de Texas Instruments y los STM32F3 de ST se
>   ven sin código nuevo en la ventana (plan §47 y §48).
> * **La fresadora CNC** de `mcu-sim` (plan §50 a §52): en la ilustración, en
>   planta y alzado; y en la **vista 3D** (*Vista ▸ Vista 3D*, Ctrl+3), con
>   Qt Quick 3D, que es opcional al compilar y va en los paquetes de las tres
>   plataformas.
>
> Para verlo, con las dos cosas compiladas y una al lado de la otra:
>
> ```bash
> cp config.ejemplo.json config.json      # apunta a ../mcu-sim y al blinky
> ./build/mcu-sim-gui                     # Ctrl+L, Lanzar, y luego Arrancar
> ./build/mcu-sim-gui --lanza             # o lanzarlo nada mas abrir
> ./build/mcu-sim-gui --edit              # y para colocar las placas en la ilustracion
> ```
>
> Lanzarlo a mano desde otra consola sigue valiendo: `mcu-sim placa.xml
> firmware.bin --gui`.

---

## Qué es, y por qué es un programa aparte

`mcu-sim` es un simulador didáctico para quien desarrolla en el IDE de su
microcontrolador —STM32CubeIDE, Code Composer Studio— y no tiene la tarjeta
delante: se compila el firmware igual que para el chip y se ejecuta contra el
modelo igual que contra una placa. Hoy modela **46 referencias**:
* de ST, los STM32F405/407, F415/417 y F446, y los STM32F302/303/358;
* de Texas Instruments, el MSP432P401R y el MSP432E401Y.

Todo lo que se ve de él, sin esta ventana, es texto en una consola.

Lo que falta es lo que un alumno espera ver: **el LED encendiéndose**. Eso es
esto.

La forma de conectarlos estaba analizada antes de escribir una línea, en
[`mcu-sim/doc/analisis_gui.md`](../mcu-sim/doc/analisis_gui.md) —1 188 líneas,
21 secciones y las medidas hechas, no supuestas—. El análisis compara tres
escenarios:

| | Qué es |
| :--- | :--- |
| 1 | Un programa Qt, un solo hilo: la ventana da cuerda a la simulación |
| 2 | Un programa Qt, la simulación en su propio hilo |
| **3** | **Dos procesos**, hablando por un socket ← **es el que se sigue aquí** |

**Este proyecto es la opción de dos procesos.** `mcu-sim` sigue siendo lo que
era —C++17 y `<systemc>`, sin una cabecera de Qt, sin `moc`, compilable en
cualquier sitio— y gana un argumento, `--gui host:puerto`, que le dice dónde
está la ventana. Sin ese argumento **se comporta exactamente igual que hoy**.

### Lo que eso compra

* **Aislamiento de verdad.** Un firmware raro de un alumno que tumbe el modelo
  no se lleva la ventana, y al revés. Y `mcu-sim` se puede correr bajo ASan con
  la ventana encima, sin que los sanitizers vean el código de Qt.
* **El modelo no aprende Qt.** El ejecutable que tiene que compilar en Linux,
  Windows y macOS sigue teniendo **un solo fichero** que sabe en qué sistema
  operativo corre.
* **El protocolo se convierte en un activo**, igual que pasó con el netlist en
  XML: un guion, una prueba automática o un panel web pueden ser clientes sin
  tocar el simulador.
* **El IDE del alumno ya es un tercer proceso** hablando por TCP. Procesos
  separados no son una rareza en este producto: son lo normal.

### Y el argumento en contra, que no se esconde

El análisis, en su §18.2, **recomienda el escenario 2** y no este, por una razón
que sigue siendo buena: *la distribución.* «Un alumno tiene que instalar una
cosa y pulsar un icono. Dos ejecutables que se buscan por un puerto es una
fuente de incidencias de soporte —cortafuegos, puertos ocupados, uno que arranca
y el otro no— que consume el tiempo del profesor, que es el recurso escaso.»

**Ese requisito se convierte aquí en diseño en vez de en excusa.** El alumno
lanza *una* cosa: `mcu-sim-gui`. Es la GUI la que **escucha primero y después
lanza `mcu-sim`** como proceso hijo, con todos sus argumentos. De ahí:

* **no hay carrera de arranque**: cuando el modelo se conecta, el escuchador
  lleva puesto desde antes de existir el proceso;
* **no hay puerto ocupado**: si el de la configuración lo está, la GUI toma otro
  y se lo pasa al hijo. El alumno no se entera;
* **no hay cortafuegos**: por omisión es `localhost`. El `host` del argumento
  está para la GUI en otra máquina, que es un caso de profesor.

Lo que queda en pie del precio, y hay que decirlo: **son dos ciclos de
compilación y dos sitios donde mirar** cuando algo no cuadra.

Y el consuelo, que también es del análisis: las tres opciones comparten la parte
cara —los observables, los mandos, la frontera— y se diferencian en la barata.
Volver al escenario 2 sería tirar tres fases de nueve, no el proyecto.

---

## Cómo hablan los dos programas

Una conexión TCP, marco binario con la longitud en la cabecera, y el saludo en
el XML que `mcu-sim --netlist` ya produce. La especificación completa está en
**[`doc/protocolo.md`](doc/protocolo.md)** y la definición ejecutable en
[`src/protocolo.h`](src/protocolo.h), que es **el mismo fichero** que
`mcu-sim/src/common/protocolo.h` y hay una comprobación que lo exige.

Lo esencial en cuatro líneas:

* **la GUI escucha y `mcu-sim` se conecta**, porque quien lanza el proceso es la
  GUI y así no hay carrera;
* **la simulación no empieza hasta que la GUI lo diga**, y no en el sentido
  flojo: `sc_start()` no se ha llamado. Todo el saludo —quién soy, la placa, el
  catálogo de lo observable y lo accionable, la suscripción— ocurre antes;
* del modelo a la pantalla van **instantáneas** de los observables suscritos a
  ritmo fijo, **avisos** de texto y **estado**;
* de la pantalla al modelo van **órdenes** —`{t_sim_ns, pieza, mando, valor}`,
  sueltas o en secuencias programadas por adelantado— y el **control** de la
  simulación.

La GUI **no conoce ni un tipo de C++ del modelo**: construye su pantalla a
partir del XML de la placa y del catálogo. Añadir una pieza nueva al simulador
la hace aparecer aquí **sin recompilar esto**.

---

## El plan

**[`doc/plan_dos_procesos.md`](doc/plan_dos_procesos.md)**: nueve fases, cada
una con qué deja hecho, cómo se comprueba y qué NO entra; y después, una
sección por cada cosa que se ha añadido a la ventana.

| | Fase | Dónde toca | Estado |
| :--- | :--- | :--- | :--- |
| 0 | Los dos esqueletos y la cadena de herramientas | los dos | Hecha |
| 1 | `Observable` / `Mando`, instantánea y cola de órdenes | `mcu-sim` | Hecha |
| 2 | La capa de transporte, probada sin SystemC | los dos | Hecha |
| 3 | El saludo y el arranque diferido | los dos | Hecha |
| 4 | Instantáneas, avisos y estado en marcha | los dos | Hecha |
| 5 | Las órdenes | los dos | Hecha |
| 6 | El control de la simulación | los dos | Hecha |
| 7 | La ventana de verdad y el lanzamiento automático | `mcu-sim-gui` | Hecha |
| 8 | Grabación y reproducción de sesiones | los dos | Pendiente |
| 9 | Que el precio de los dos procesos no lo pague el alumno | los dos | En parte: los ejecutables, con Qt dentro (plan §17); faltan la firma, un instalador con los dos programas y `--gui-escucha` |

Y, aparte del plan, **las ilustraciones**
([`doc/analisis-uso-ilustraciones.md`](doc/analisis-uso-ilustraciones.md), plan
§21 a §45): cada placa con su **dibujo SVG** —el que manda `mcu-sim` con la
placa, o uno que la ventana genera si no trae ninguno—, los LEDs que brillan,
los botones que se pulsan, los mandos que giran sobre él —la rueda gira el
encoder del KY-040—, el aspa de un servo que gira con su ángulo y las
pantallas que enseñan lo que enseñaría la de verdad —el TFT de 128x160, por
`T_IMAGEN`—, y en un sistema todas las placas una al lado de otra con una
línea por cada conector enchufado o hilo —que llega al centro de un pin
derecho y al codo de uno acodado, con un círculo (plan §44 y §46)—, que cada placa enseña o esconde con
su botón «Conexiones» —o todas a la vez, y en la edición se enseñan solas—.
**Arrancando con `--edit`** (plan §45; sin él, la ilustración solo se mira),
con el botón **Edición** las placas se colocan con el ratón: se
arrastran, se giran de 90 en 90 grados y se agrandan o se achican; el lienzo
puede tener un tamaño en milímetros, y Ctrl+rueda acerca o aleja. Lo
colocado se guarda en la configuración y sale igual la próxima vez que se abre
el mismo sistema. Cada hilo va de su color —negro la masa, rojo la
alimentación— y las líneas se pueden enrutar en tramos horizontales y
verticales con las esquinas redondeadas, que también se guardan. Y todo eso
puede ir al XML del sistema, que lo lleva consigo: «Copiar como XML» da las
líneas para pegarlas, y «Guardar en el XML» las escribe en el fichero si la
ventana lanzó `mcu-sim` (plan §38;
[`doc/analisis_disposicion_ilustracion.md`](doc/analisis_disposicion_ilustracion.md)). La ilustración va en
**su propia ventana** (Vista ▸ Ilustración, Ctrl+I; plan §27), para ponerla
donde se quiera; el panel de siempre se queda en la principal.

Sobre el dibujo, además, la tabla de enlaces de la placa puede pedir efectos
que no salen solos (plan §49 y §50):

| Efecto | Qué hace | Dónde se usa |
| :--- | :--- | :--- |
| `display` | Escribe los valores de la pieza dentro del elemento, uno por fila, en cifras rojas | La fuente de laboratorio; las cotas de una fresadora, en mm con su signo |
| `desplaza` | Traslada el elemento con uno o varios observables, cada uno con su vector | Los carros de una fresadora, en planta y alzado |

Una pantalla también se puede desplazar: la pieza mecanizada va con la mesa. Y
su imagen llega a trozos (`T_IMAGEN_RECT`), solo lo que cambió.

Y **la vista 3D** (plan §51 y §52): una placa que declara su forma con un
`<vista3d>` —la fresadora 3020 de `mcu-sim`— se ve en tres dimensiones, en
otra ventana (Vista ▸ Vista 3D, Ctrl+3):
* la máquina en cajas y cilindros, con los carros que se mueven y los
  husillos que giran;
* la pieza como una superficie, con lo que la fresa va comiendo.

La cámara gira, se acerca y se desplaza con el ratón. Usa Qt Quick 3D, y se
puede compilar sin ella (abajo).

---

## Compilar

Hace falta **Qt 6.3 o posterior** (`Widgets`, `Network` y, desde las
ilustraciones, `Svg` y `SvgWidgets`) y un compilador con C++17. Nada más: ni
SystemC, ni una sola cabecera de `mcu-sim`. En Ubuntu son `qt6-base-dev` y
`qt6-svg-dev`; en MSYS2, `qt6-base` y `qt6-svg`; el `qt` de Homebrew trae los
dos.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/mcu-sim-gui                             # en macOS: open build/mcu-sim-gui.app
ctest --test-dir build --output-on-failure      # las pruebas, sin ventana
```

Si CMake no encuentra Qt, se le dice dónde está:

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/ruta/a/Qt/6.7.0/gcc_64
```

**La vista 3D** (*Vista ▸ Vista 3D*, la fresadora de `mcu-sim` en tres
dimensiones) usa además **Qt Quick 3D**:
* los módulos `Quick`, `QuickWidgets` y `Quick3D`;
* en tiempo de ejecución, los módulos de QML `QtQuick3D` y
  `QtQuick3D.Helpers`.

En Ubuntu son los paquetes `qt6-declarative-dev`, `qt6-quick3d-dev`,
`qt6-shadertools-dev`, `qml6-module-quick3d`, `qml6-module-quick3d-helpers`,
`qml6-module-qtquick` y `qml6-module-qtquick-window`. Para empaquetar, además,
`qt6-declarative-dev-tools` (`qmlimportscanner`) y `qt6-wayland`.

La opción `MCU_SIM_GUI_3D` dice qué se hace:

| Valor | Qué hace |
| :--- | :--- |
| `AUTO` (por omisión) | La compila si encuentra Qt Quick 3D |
| `ON` | La exige |
| `OFF` | No la compila |

Sin ella, todo lo demás es igual:

```bash
cmake -B build -DMCU_SIM_GUI_3D=OFF
```

Los paquetes de las tres plataformas (`ci/empaqueta-*.sh`) la llevan, con sus
módulos de QML dentro:

| Plataforma | Cómo se meten | Dónde van |
| :--- | :--- | :--- |
| Linux | `linuxdeploy-plugin-qt`, con `QML_SOURCES_PATHS=src` | `usr/qml` del AppImage |
| Windows | `windeployqt6 --qmldir src`, y un `qt.conf` | `qml/`, junto al `.exe` |
| macOS | A mano, con `qmlimportscanner`: con el Qt de Homebrew partido en fórmulas, `macdeployqt -qmldir` no los encuentra | `Contents/Resources/qml`, con un `qt.conf` |

Cada script lo comprueba cargando la vista 3D desde el paquete, con
`mcu-sim-gui --comprueba-3d`. Esa opción dice:
* si Qt Quick 3D y sus módulos de QML se cargan, y de dónde;
* con el código de salida, 0 si sí, 1 si no, y 3 si esa compilación no lleva
  la vista 3D.

Con una etiqueta, el CI hace además, en cada plataforma, el mismo paquete
**sin la vista 3D**, y la Release publica los dos:
* se compila con `-DMCU_SIM_GUI_3D=OFF`, y `--comprueba-3d` confirma con un 3
  que no la lleva;
* sus ficheros terminan en `-sin-3d`.

Para probar los dos sin etiquetar, el CI se lanza a mano (*Actions ▸ ci ▸ Run
workflow*).

Para compilar con Qt Quick 3D en MSYS2 hacen falta, además,
`mingw-w64-x86_64-qt6-declarative`, `mingw-w64-x86_64-qt6-quick3d` y
`mingw-w64-x86_64-qt6-shadertools`. En Homebrew, `qt` ya los trae.

### En Windows con MSYS2: tres trampas, y la tercera es la que muerde

Las tres están comprobadas a base de tropezar con ellas. Van en el orden en que
aparecieron, que no es el orden de importancia: **la que de verdad rompe la
compilación es la tercera.**

**1. Usa el CMake de MinGW, no el de MSYS.** Si el error cita
`/usr/share/cmake/...` mientras el compilador es `/mingw64/bin/c++.exe`, están
mezclados dos entornos distintos. MSYS2 lo documenta: *«When building projects
for Windows with CMake […] make sure to install the MinGW version of CMake»*, y
recomienda Ninja como generador. Desde el shell **MINGW64**:

```bash
pacman -S --needed mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
                   mingw-w64-x86_64-gcc  mingw-w64-x86_64-qt6-base \
                   mingw-w64-x86_64-qt6-svg
which cmake     # tiene que decir /mingw64/bin/cmake
```

**2. No compiles dentro de la carpeta sincronizada.** Un árbol de compilación
dentro de Dropbox, OneDrive o Drive sincroniza miles de ficheros objeto que no
le importan a nadie, y el cliente abre cada fichero recién creado. No es fatal,
pero no cuesta nada evitarlo —y `CMakeLists.txt` avisa si lo detecta—:

```bash
cmake -G Ninja -B C:/build/mcu-sim-gui -S .
cmake --build C:/build/mcu-sim-gui
```

**3. El antivirus pone en cuarentena lo que acabas de compilar.** Esta es la que
para la compilación en seco, y la que no se parece en nada a su causa. El
síntoma, en **ESET**, es este:

```
file STRINGS file ".../CompilerIdCXX/a.exe" cannot be read.
file failed to open for reading (Permission denied)
ninja: error: '.../testCXXCompiler.cxx', missing and no known rule to make it
-- Check for working CXX compiler: /mingw64/bin/c++.exe - broken
```

Y la prueba que lo desenmascara no necesita CMake ninguno:

```bash
mkdir -p /c/build/hola && cd /c/build/hola
printf 'int main(){return 0;}\n' > t.cpp
g++ t.cpp -o t.exe && ls -la t.exe && ./t.exe
ls -l                       # ¿sigue estando t.exe?
```

Si el `.exe` se crea, **no se deja ejecutar** («Permission denied») y unos
segundos después **ha desaparecido del directorio**, no hay más que hablar: un
fichero bloqueado da permiso denegado, pero un fichero que se esfuma está en
cuarentena. La confirmación está en el propio antivirus —**ESET → Herramientas
→ Cuarentena**, donde aparece el `t.exe`— y el registro dice qué módulo lo hizo.

**Por qué pasa, y por qué no tiene arreglo desde el proyecto:** un compilador
produce ejecutables **nuevos, sin firmar y desconocidos**, que es literalmente
la definición de lo que un antivirus heurístico caza. Da igual el compilador,
el generador o el IDE.

**El arreglo** son dos exclusiones de rendimiento en ESET:

> **Configuración avanzada** (`F5`) → **Motor de detección** → **Exclusiones** →
> **Exclusiones de rendimiento** → **Editar** → **Agregar**

```
C:\msys64\*
C:\build\*
```

Antes de tocar la configuración, si quieres confirmarlo sin compromiso: **pausa
la protección diez minutos** desde el icono de ESET y repite la prueba de
arriba. Si el `t.exe` sobrevive y se ejecuta, ya está identificado.

Si después de las exclusiones sigue pasando, el que bloquea no es el escáner en
tiempo real sino **HIPS** o **ESET LiveGuard** —el que manda a la nube los
ejecutables desconocidos y los retiene hasta tener veredicto—; el registro de
ESET dice cuál, y cada uno tiene su propia lista de exclusiones. Y si el equipo
lo administra tu organización, esto es una petición de una línea para quien
lleve la consola: desbloquea cualquier trabajo de compilación en esa máquina, no
solo este proyecto.

**Y una consecuencia para el producto, que no es una anécdota.** `mcu-sim-gui`
se va a distribuir a alumnos como un `.exe` sin firmar. Lo que acaba de pasar
aquí les va a pasar a ellos, en sus máquinas, con sus antivirus, el día que
descarguen el simulador. Eso es trabajo de la **fase 9** y está anotado allí.

### Qué está verificado y qué no

| Plataforma | Estado |
| :--- | :--- |
| Linux, g++ 13, **Qt 6.4.2** | **Verificado**: configura, compila, enlaza y arranca (`QT_QPA_PLATFORM=offscreen`) |
| Windows, MSYS2 / MinGW-w64 | **Verificado por el CI** desde el 2026-10-01: compila con el Qt de MSYS2 y pasa `ctest`. **A mano, en una máquina, no**: los intentos fallaron **por el entorno y no por el código** —la causa era que **ESET pone en cuarentena los ejecutables recién compilados**—. Las tres trampas están arriba, y hay una cuarta que llega al repartirlo: las DLL (plan §8.8) |
| macOS, clang, Qt 6 | **Verificado por el CI** desde el 2026-10-01, con el Qt de Homebrew: compila y pasa `ctest`. A mano, no |
| La vista 3D | **Linux, verificada** con Qt 6.4.2: se dibuja en X11 (xvfb) y en Wayland (weston), también desde el AppImage. **Windows y macOS**: el código y los scripts de empaquetado están, pero **sin verificar hasta su primer CI** con Qt Quick 3D |

Es la misma regla que sigue `mcu-sim` y por el mismo motivo: decir lo que se ha
probado y lo que no, en vez de dar por bueno lo que parece obvio.

**La integración continua** (`.github/workflows/ci.yml`) compila y pasa las
pruebas en las tres plataformas de esta tabla en cada push a `main` y en cada
pull request: Linux con el Qt del sistema (Ubuntu 24.04), Windows con MSYS2 y
macOS con Homebrew. Pasó en las tres por primera vez el 2026-10-01, y de ahí
las dos filas de arriba.

Compila con la vista 3D en las tres (`-DMCU_SIM_GUI_3D=ON`). En Linux,
además, la dibuja en una pantalla virtual (xvfb). Lo que el CI no ve es una
ventana abierta en una pantalla de verdad: eso sigue siendo a mano.

---

## Configuración

`config.ejemplo.json` es la plantilla: se copia a `config.json` —que no se
versiona, porque las rutas son de cada máquina— y se ajusta. Lleva dónde está
`mcu-sim` y desde qué directorio lanzarlo, sus argumentos **por nombre**
(`placa`, `firmware`, `--ms`, `--ondas`...: los que diga `mcu-sim
--argumentos`, sin lista escrita aquí), lo que se escriba a mano, dónde escucha
la ventana, el ritmo con el que arrancar y si la ventana va siempre encima.

Nada de eso es obligatorio por fichero: **todo se puede poner también desde el
diálogo de lanzamiento**, que la guarda al lanzar —respetando los comentarios
que tuviera—. `mcu-sim-gui` la busca en el directorio actual y, si no está, en
el de configuración del usuario; `--config FICHERO` dice otra. Si no
encuentra ninguna, el diálogo sale vacío y lo dice arriba: en qué sitios ha
buscado y dónde la guardará al lanzar. Si la encontró, de qué fichero la leyó.
El firmware va **por chip**: el diálogo le pregunta a `mcu-sim placa --mcus`
qué MCUs lleva la placa y pone una fila por cada uno, con su fichero —vacío, el
del XML— y una casilla «sin firmware» (plan §42; en la configuración,
`firmware:N/u0` y `sin-firmware:N/u0`). El puerto es una
preferencia: si está cogido, la ventana escucha en otro y se lo pasa al hijo,
sin decir nada.

---

## Distribución

```
repos/
├── mcu-sim/       el modelo. C++17 + SystemC. No sabe que esto existe
└── mcu-sim-gui/   esto. Qt 6. Sabe lanzar al otro
```

Dos repositorios a propósito: `mcu-sim` tiene que seguir clonándose y
compilándose sin Qt en cualquier máquina, que es lo que hace que sus suites
—2 151 comprobaciones solo la del F407— valgan en todas partes.

### Los ejecutables

**Una etiqueta `v*` publica los ejecutables** como Release, con Qt dentro, para
que quien solo quiera usar la ventana no tenga que compilar nada ni instalar
Qt: un `.zip` para Windows, un AppImage para Linux y una `.app` para cada Mac.
De cada uno hay **dos**: con la vista 3D y sin ella (`-sin-3d`), que es más
pequeño.
Los construye la integración continua con `ci/empaqueta-*.sh`, en **cada**
ejecución —los paquetes de un push a `main` se pueden bajar de la página de esa
ejecución—, y la etiqueta añade los de sin la vista 3D y publicarlos, y solo
si todo pasó:

```bash
git tag v0.2.0 && git push origin v0.2.0
```

Lo que comprueba cada paquete antes de publicarse: que **arranca**; en Windows,
además, que con un `PATH` sin MSYS2 todo lo que carga está en el paquete o en
Windows —la DLL equivocada en el `PATH` es justo lo que pasaba fuera del shell
de MSYS2—; en macOS, que nada dentro de la `.app` apunta a Homebrew; y en los
tres, con la vista 3D, que se carga con los módulos de QML del paquete
(`--comprueba-3d`).
Las instrucciones para quien lo descarga están en `doc/notas_release.md`, que
es el texto de la propia Release; las licencias, en `TERCEROS.md`. `mcu-sim`
va aparte, en sus propias Release.

---

## Licencia

**GNU Affero General Public License, versión 3** (`LICENSE`), la misma que
[`mcu-sim`](../mcu-sim).

Puedes usarlo, estudiarlo, modificarlo y repartirlo libremente, y si lo haces
**tus cambios tienen que quedar disponibles bajo esta misma licencia**, también
cuando el programa se ofrezca por la red en lugar de distribuirse. Las razones
son las de `mcu-sim` y están explicadas en su README.

**Qt conserva la suya.** Se enlaza bajo LGPLv3, que es compatible con AGPLv3
en esta dirección: el conjunto se reparte como AGPLv3 y las bibliotecas de Qt
siguen siendo sustituibles por el usuario.
