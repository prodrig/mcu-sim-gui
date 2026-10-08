# mcu-sim-gui

La **contraparte de visualización gráfica de
[`mcu-sim`](../mcu-sim)**, el modelo SystemC de microcontroladores STM32.
`mcu-sim` simula; esto lo enseña y deja tocarlo.

> **Estado: fase 7.** La ventana escucha, `mcu-sim --gui` se conecta, se
> saludan, y la ventana **se construye sola** a partir de la placa y el
> catálogo que le manda el modelo: un recuadro por pieza, con sus patillas, un
> indicador por cada cosa que la pieza sugiere mirar y un control por mando.
> El modelo no simula hasta que se pulsa «Arrancar», y **con la simulación en
> marcha los indicadores enseñan lo que pasa**: el LED se enciende y se apaga,
> arriba van los dos relojes, y abajo los avisos —los de la placa, antes de
> arrancar, y los del modelo—. **Y los controles mandan**: el botón de B1
> pulsa mientras está hundido, y lo que se toque antes de arrancar se aplica
> en t = 0. Cada orden vuelve con su eco, y las que el modelo no puede aplicar
> lo dicen en la lista de avisos. **Y la ventana lleva la simulación**: el
> ritmo se elige antes de arrancar —tiempo real, a la mitad, libre o a
> demanda—, y en marcha hay Pausa / Sigue, Paso a demanda y Parar. **Y la
> ventana lanza el modelo ella misma**: *Simulación ▸ Lanzar mcu-sim* (Ctrl+L)
> abre un diálogo construido con lo que dice `mcu-sim --argumentos`, y abajo,
> en la pestaña *mcu-sim*, sale todo lo que el simulador escribe. **Y se
> puede depurar con un botón pulsado**: cada botón lleva al lado un
> «switch» que lo deja hundido hasta que se vuelva a tocar, y *Vista ▸
> Siempre encima* (Ctrl+T) deja la ventana a la vista mientras se trabaja
> en el IDE; los pulsadores del modelo rebotan, y en marcha se ajusta
> cuánto dura el rebote —un deslizador que enseña su valor— y cuántas veces
> rebota —un desplegable— (plan §16). **Y una placa puede no llevar MCU**:
> una `Fuente` y una `Gnd` de `mcu-sim`, con su límite de corriente, se ven
> aquí con la corriente en mA y la **sobrecorriente como alarma** —«⚠ SI» en
> rojo, y el título de la pieza también—; arriba, «sin MCU» (plan §18). **Y
> varias placas enchufadas** —una Nucleo con un shield encima, un `<sistema>`
> de `mcu-sim`— se ven en un recuadro por placa, cada conector diciendo con
> qué está enchufado (plan §19, protocolo v2), también en pila, como en
> PC/104; y la ventana sabe ya todo lo necesario para dibujar el sistema
> algún día (plan §20). Lo que falta: grabar y reproducir sesiones (fase 8).
>
> Para verlo, con las dos cosas compiladas y una al lado de la otra:
>
> ```bash
> cp config.ejemplo.json config.json      # apunta a ../mcu-sim y al blinky
> ./build/mcu-sim-gui                     # Ctrl+L, Lanzar, y luego Arrancar
> ./build/mcu-sim-gui --lanza             # o lanzarlo nada mas abrir
> ```
>
> Lanzarlo a mano desde otra consola sigue valiendo: `mcu-sim placa.xml
> firmware.bin --gui`.

---

## Qué es, y por qué es un programa aparte

`mcu-sim` es un simulador didáctico para quien desarrolla en STM32CubeIDE y no
tiene la tarjeta delante: se compila el firmware igual que para el chip y se
ejecuta contra el modelo igual que contra una placa. Hoy tiene **dos familias y
29 referencias**, y todo lo que se ve de él es texto en una consola.

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
una con qué deja hecho, cómo se comprueba y qué NO entra.

| | Fase | Dónde toca |
| :--- | :--- | :--- |
| 0 | Los dos esqueletos y la cadena de herramientas | los dos |
| 1 | `Observable` / `Mando`, instantánea y cola de órdenes | `mcu-sim` |
| 2 | La capa de transporte, probada sin SystemC | los dos |
| 3 | El saludo y el arranque diferido | los dos |
| 4 | Instantáneas, avisos y estado en marcha | los dos |
| 5 | Las órdenes | los dos |
| 6 | El control de la simulación | los dos |
| 7 | La ventana de verdad y el lanzamiento automático | `mcu-sim-gui` |
| 8 | Grabación y reproducción de sesiones | los dos |
| 9 | Que el precio de los dos procesos no lo pague el alumno | los dos |

Y, aparte del plan, **las ilustraciones**
([`doc/analisis-uso-ilustraciones.md`](doc/analisis-uso-ilustraciones.md), plan
§21 a §37): cada placa con su **dibujo SVG** —el que manda `mcu-sim` con la
placa, o uno que la ventana genera si no trae ninguno—, los LEDs que brillan,
los botones que se pulsan, los mandos que giran sobre él —la rueda gira el
encoder del KY-040—, el aspa de un servo que gira con su ángulo y las
pantallas que enseñan lo que enseñaría la de verdad —el TFT de 128x160, por
`T_IMAGEN`—, y en un sistema todas las placas una al lado de otra con una
línea por cada conector enchufado o hilo, que cada placa enseña o esconde con
un botón. Con el botón **Edición**, las placas se colocan con el ratón: se
arrastran, se giran de 90 en 90 grados y se agrandan o se achican; el lienzo
puede tener un tamaño en milímetros, y Ctrl+rueda acerca o aleja. Lo
colocado se guarda en la configuración y sale igual la próxima vez que se abre
el mismo sistema. Cada hilo va de su color —negro la masa, rojo la
alimentación— y las líneas se pueden enrutar en tramos horizontales y
verticales con las esquinas redondeadas, que también se guardan
([`doc/analisis_disposicion_ilustracion.md`](doc/analisis_disposicion_ilustracion.md)). La ilustración va en
**su propia ventana** (Vista ▸ Ilustración, Ctrl+I; plan §27), para ponerla
donde se quiera; el panel de siempre se queda en la principal.

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

Es la misma regla que sigue `mcu-sim` y por el mismo motivo: decir lo que se ha
probado y lo que no, en vez de dar por bueno lo que parece obvio.

**La integración continua** (`.github/workflows/ci.yml`) compila y pasa las
pruebas en las tres plataformas de esta tabla en cada push a `main` y en cada
pull request: Linux con el Qt del sistema (Ubuntu 24.04), Windows con MSYS2 y
macOS con Homebrew. Pasó en las tres por primera vez el 2026-10-01, y de ahí
las dos filas de arriba. Lo que el CI no ve es una ventana abierta en una
pantalla de verdad: eso sigue siendo a mano.

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
buscado y dónde la guardará al lanzar. Si la encontró, de qué fichero la leyó. El puerto es una
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
—2 118 comprobaciones solo la del F407— valgan en todas partes.

### Los ejecutables

**Una etiqueta `v*` publica los ejecutables** como Release, con Qt dentro, para
que quien solo quiera usar la ventana no tenga que compilar nada ni instalar
Qt: un `.zip` para Windows, un AppImage para Linux y una `.app` para cada Mac.
Los construye la integración continua con `ci/empaqueta-*.sh`, en **cada**
ejecución —los paquetes de un push a `main` se pueden bajar de la página de esa
ejecución—, y la etiqueta solo añade publicarlos, y solo si todo pasó:

```bash
git tag v0.2.0 && git push origin v0.2.0
```

Lo que comprueba cada paquete antes de publicarse: que **arranca**; en Windows,
además, que con un `PATH` sin MSYS2 todo lo que carga está en el paquete o en
Windows —la DLL equivocada en el `PATH` es justo lo que pasaba fuera del shell
de MSYS2—; y en macOS, que nada dentro de la `.app` apunta a Homebrew.
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
