# mcu-sim-gui

La **contraparte de visualización gráfica de
[`mcu-sim`](../mcu-sim)**, el modelo SystemC de microcontroladores STM32.
`mcu-sim` simula; esto lo enseña y deja tocarlo.

> **Estado: fase 0.** Hay un esqueleto que compila y abre una ventana vacía, el
> protocolo escrito y el plan por fases. Todavía no habla con nadie.

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

### En Windows con MSYS2, dos cosas que hay que hacer bien

Las dos están comprobadas a base de tropezar con ellas, así que van aquí y no
en un comentario que nadie lee.

**1. No compiles dentro de la carpeta sincronizada.** Si el árbol de
compilación está en Dropbox, OneDrive o Drive, el cliente de sincronización —y
el antivirus detrás— abre cada `.exe` recién creado para subirlo, y mientras lo
tiene abierto CMake no puede leerlo ni borrarlo. Sale esto:

```
file STRINGS file ".../CompilerIdCXX/a.exe" cannot be read.
file failed to open for reading (Permission denied)
The file ".../cmTC_32e80.exe" could not be removed: Permission denied
```

y como la detección del compilador **reintenta**, puede acabar diciendo
`Check for working CXX compiler - works` **después** de haber fallado la
detección de ABI. Un fallo intermitente que además se contradice, que es la
peor clase. `CMakeLists.txt` avisa si detecta el caso, pero la solución es
sacar el árbol de ahí —y de paso se deja de sincronizar un directorio con miles
de ficheros objeto—:

```bash
cmake -G Ninja -B /c/build/mcu-sim-gui -S .
cmake --build /c/build/mcu-sim-gui
```

**2. Usa el CMake de MinGW, no el de MSYS.** Si el error cita
`/usr/share/cmake/...` mientras el compilador es `/mingw64/bin/c++.exe`, están
mezclados dos entornos distintos. MSYS2 lo dice en su documentación: *«When
building projects for Windows with CMake […] make sure to install the MinGW
version of CMake»*, y recomienda Ninja como generador. Desde el shell
**MINGW64**:

```bash
pacman -S --needed mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
                   mingw-w64-x86_64-gcc  mingw-w64-x86_64-qt6-base
which cmake     # tiene que decir /mingw64/bin/cmake
```

Con el CMake correcto las rutas salen como `C:/Users/...` y no como
`/c/Users/...`, que es la señal de que se está usando el de MSYS.

### Qué está verificado y qué no

| Plataforma | Estado |
| :--- | :--- |
| Linux, g++ 13, **Qt 6.4.2** | **Verificado**: configura, compila, enlaza y arranca (`QT_QPA_PLATFORM=offscreen`) |
| Windows, MSYS2 / MinGW-w64 | **⚠ sin verificar todavía.** Primer intento **fallido por el entorno, no por el código**: árbol de compilación dentro de Dropbox y el CMake de MSYS en vez del de MinGW. Las dos trampas, y su salida, están arriba |
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
compilándose sin Qt en cualquier máquina, que es lo que hace que su suite de
2 074 comprobaciones valga en todas partes.
