# mcu-sim-gui

La **contraparte de visualización gráfica de
[`mcu-sim`](../mcu-sim)**, el modelo SystemC de microcontroladores STM32.
`mcu-sim` simula; esto lo enseña y deja tocarlo.

> **Estado: fase 1.** Hay un esqueleto que compila y abre una ventana vacía, el
> protocolo escrito y el plan por fases. Del lado de `mcu-sim` ya existe la
> frontera —lo que cada pieza deja ver y tocar, el catálogo, el muestreador y el
> aplicador—, probada sin GUI. Todavía no habla con nadie: la conexión llega
> con las fases 2 y 3.

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

---

## Compilar

Hace falta **Qt 6.3 o posterior** (`Widgets` y `Network`) y un compilador con
C++17. Nada más: ni SystemC, ni una sola cabecera de `mcu-sim`.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/mcu-sim-gui
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
                   mingw-w64-x86_64-gcc  mingw-w64-x86_64-qt6-base
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
| Windows, MSYS2 / MinGW-w64 | **⚠ sin verificar todavía**, pero el vecino sí: `mcu-sim.exe` se construye y **se ejecuta** allí. Los intentos de esta GUI fallaron **por el entorno y no por el código** —la causa era que **ESET pone en cuarentena los ejecutables recién compilados**—. Las tres trampas están arriba, y hay una cuarta que llega al repartirlo: las DLL (plan §8.8) |
| macOS, clang, Qt 6 | **⚠ sin verificar** |

Es la misma regla que sigue `mcu-sim` y por el mismo motivo: decir lo que se ha
probado y lo que no, en vez de dar por bueno lo que parece obvio.

---

## Configuración

`config.ejemplo.json` es la plantilla: se copia a `config.json` —que no se
versiona, porque las rutas son de cada máquina— y se ajusta. Lleva dónde está
`mcu-sim`, qué placa y qué firmware cargar, y **todos** los demás argumentos del
simulador.

Nada de eso es obligatorio por fichero: **todo se puede poner también desde el
diálogo de lanzamiento de la ventana**. El fichero está para no tener que
hacerlo cada vez.

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
