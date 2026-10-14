# Guía de usuario de mcu-sim-gui

*Lo necesario para usar el programa: elegir el circuito y el firmware, lanzar
la simulación, depurar con GDB y hablar con el puerto serie del MCU. Las
opciones de la línea de órdenes están en el [anexo A](#anexo-a-la-línea-de-órdenes),
y el resto de la documentación, en el [anexo B](#anexo-b-el-resto-de-la-documentación).*

---

## 1. Qué es, y qué hace falta

`mcu-sim-gui` es la ventana de **`mcu-sim`**, el simulador de
microcontroladores STM32. Son **dos programas**:

* **`mcu-sim`** simula: el MCU, la placa y lo que lleva conectado, con tu
  firmware dentro;
* **`mcu-sim-gui`** lo lanza, lo enseña —un panel con cada pieza y una
  ilustración de las placas, con los LEDs que se encienden y los botones que
  se pulsan— y deja tocarlo.

Hacen falta los dos. Se descargan de sus propias *Releases* —cada paquete
dice cómo arrancarlo en [`notas_release.md`](notas_release.md)— o se
compilan (el `README.md` de cada uno). Lo cómodo es tenerlos **uno al lado
del otro**:

```
repos/
  mcu-sim/         el simulador: src/build/mcu-sim, src/placas/, src/verif/fw/
  mcu-sim-gui/     esta ventana
```

## 2. Arrancar la ventana

```
mcu-sim-gui
```

La ventana se abre **vacía y escuchando**: espera a que un `mcu-sim` se
conecte. Lo normal es que lo lance ella misma (§3). Con `--lanza` lo lanza
nada más abrir, con lo que se usó la última vez; con `--edit` deja además
colocar las placas en la ilustración (§4.3). Todas las opciones, en el
[anexo A](#anexo-a-la-línea-de-órdenes).

## 3. Lanzar una simulación

*Simulación ▸ Lanzar mcu-sim...* (**Ctrl+L**) abre el diálogo de lanzamiento.
Arriba dice de dónde salen los valores: el fichero de configuración que se
leyó o, la primera vez, que no había ninguno y dónde se guardará. **Al
lanzar se guarda todo**, y la próxima vez el diálogo sale relleno.

| Campo | Qué poner |
| :--- | :--- |
| **Ejecutable** | La ruta de `mcu-sim` (`mcu-sim.exe` en Windows): `../mcu-sim/src/build/mcu-sim`, por ejemplo. En cuanto se dice, el diálogo le pide sus opciones y se rellena con ellas |
| **Directorio** | Desde dónde se lanza: el `src/` de `mcu-sim`, donde están `placas/` y `verif/`. **La placa y el firmware se buscan aquí** |
| **placa** | El circuito (§3.1) |
| **firmware** | Lo que se carga en la Flash (§3.2) |
| las demás | Las opciones de `mcu-sim`: depuración (§5), puerto serie (§6)... Cada una explica lo que hace al pasar el ratón por encima. La lista entera, en el [anexo A.2](#a2-las-opciones-de-mcu-sim-en-el-diálogo) |
| **A mano** | Cualquier otro argumento, tal cual, para lo que el diálogo no ofrezca |

Debajo de todo se ve **la línea de órdenes que va a salir**. «Lanzar» la
ejecuta; abajo, en la pestaña **mcu-sim**, sale todo lo que el simulador
escribe —el arranque, los avisos, un error si algo no le gusta—.

*Simulación ▸ Lanzar otra vez* (**Ctrl+R**) repite el último lanzamiento sin
diálogo, y *Detener mcu-sim* lo para.

### 3.1 Elegir el circuito

El circuito es un **fichero XML**: o una **placa** —un MCU con lo que lleva
soldado— o un **sistema** —varias placas enchufadas o cableadas—. Los de
ejemplo están en `placas/` de `mcu-sim`:

| Fichero | Qué es | Firmware de ejemplo |
| :--- | :--- | :--- |
| `discovery_min.xml` | Una Discovery del STM32F407 mínima: sus cuatro LEDs y sus botones | `verif/fw/blinky/blinky.bin` |
| `nucleo_f446re.xml` | La NUCLEO-F446RE: LED, botón y el puerto serie del ST-LINK (§6) | el tuyo |
| `nucleo_f446re_vcp.xml` | La Nucleo para probar el puerto serie (§6) | `verif/fw/vcp_demo/vcp_demo.bin` |
| `nucleo_f446re_tft.xml` | La Nucleo con una pantalla TFT de 1,8" | el que dice el XML |
| `nucleo_f446re_servo.xml` | La Nucleo con un servo, un encoder rotativo y la pantalla | el que dice el XML |
| `nucleo_f446re_barra8ac_azul.xml` | La Nucleo con una barra de 8 LEDs y un adaptador USB-serie | el que dice el XML |
| `nucleo_y_shield.xml` | La Nucleo con un shield de LEDs enchufado | el que dice el XML |

Para un montaje propio se copia uno parecido y se cambia: el formato está en
`doc/parts.md` de `mcu-sim` ([anexo B](#anexo-b-el-resto-de-la-documentación)).

### 3.2 El firmware

Es **la imagen binaria** que el compilador genera para el chip: un `.bin`
—o un Intel HEX, `.hex`— que se carga a partir del principio de la Flash
(0x08000000). En STM32CubeIDE se genera solo con *Project ▸ Properties ▸
C/C++ Build ▸ Settings ▸ MCU Post build outputs ▸ Convert to binary file*, y
sale junto al `.elf`, en `Debug/`.

El campo **firmware** depende de los MCUs que lleve el circuito; el diálogo
se lo pregunta a `mcu-sim` al escribir la placa:

* **Con un MCU**, la fila dice de cuál es —«firmware de N/u0 (STM32F446RE)»—
  y enseña en gris el que trae el XML, si trae uno. **Vacía, se usa ese.** Al
  lado, la casilla **«sin firmware»** lo lanza sin ninguno, aunque el XML lo
  diga: el núcleo se queda parado sin hacer nada, que es lo que se quiere
  cuando el firmware lo va a cargar el depurador (§5).
* **Con varios MCUs**, una fila **por cada uno**, con lo mismo.

Las rutas son relativas al **Directorio**; el botón «…» las pone así solo.

## 4. Durante la simulación

Cuando `mcu-sim` se conecta, la ventana **se construye sola** con lo que le
manda: un recuadro por pieza en el **panel** y, si las placas traen dibujo,
la **ilustración**. Nada se mueve hasta pulsar **Arrancar**.

### 4.1 Los controles

| Control | Qué hace |
| :--- | :--- |
| **Ritmo** (antes de arrancar) | *tiempo real*: un segundo simulado por segundo de reloj, el de trabajar; *a la mitad*; *libre*: todo lo deprisa que se pueda; *a demanda*: arranca en pausa y solo avanza con *Paso* |
| **Arrancar** | Empieza la simulación. Lo tocado antes —un botón pulsado— se aplica en t = 0 |
| **Pausa / Sigue** | Para el tiempo simulado y lo reanuda |
| **Paso** | En pausa, avanza los milisegundos de al lado |
| **Parar** | Termina la simulación |

Arriba van **los dos relojes**, el simulado y el de pared. Abajo, la pestaña
**Avisos** —los de la placa antes de arrancar, y los del modelo: una
sobrecorriente, una orden que no se pudo aplicar— y la pestaña **mcu-sim**.

### 4.2 El panel

Una caja por pieza, con lo que deja ver —un LED encendido, una corriente en
mA, una alarma en rojo— y lo que deja tocar: un **botón** se pulsa con el
ratón y, con el «switch» de al lado, se queda hundido hasta volver a tocarlo;
un **encoder** gira; un pulsador **rebota** como uno de verdad, y su rebote se
ajusta en marcha. *Vista ▸ Siempre encima* (**Ctrl+T**) deja la ventana a la
vista mientras se trabaja en el IDE.

### 4.3 La ilustración

*Vista ▸ Ilustración* (**Ctrl+I**) la abre en **su propia ventana**: cada
placa con su dibujo, y sobre él los LEDs que brillan, los botones que se
hunden al pulsarlos, el mando del encoder —que gira con la rueda del ratón—,
el aspa del servo y la imagen de la pantalla. Ctrl+rueda acerca o aleja, y
«Ajustar» vuelve a enseñarlo todo.

En un sistema, las **líneas entre placas** —un conector enchufado, un
hilo— nacen escondidas: el botón **«Conexiones»** de cada placa enseña las
suyas, y **«Todas las conexiones»**, las de todas. Cada hilo va de su color:
negro la masa, rojo la alimentación. Un hilo que llega a un pin dibujado
acaba en un círculo donde se enchufa el cable: en el centro del pin si sale
hacia arriba, y en su codo —el pad, donde entra en la placa— si es acodado y
sale por el borde, como los del FT232RL.

**Colocar las placas** —moverlas, girarlas, escalarlas, el tamaño del
lienzo, llevar las líneas en tramos rectos, guardarlo en el XML— y **abrir
otro dibujo** para una placa solo se puede con la ventana arrancada con
**`--edit`**: entonces aparece el botón **«Edición»**, y lo colocado se
recuerda la próxima vez. Sin `--edit`, la ilustración solo se mira. Cómo se
edita, en `analisis_disposicion_ilustracion.md`
([anexo B](#anexo-b-el-resto-de-la-documentación)).

## 5. Depurar con GDB o con STM32CubeIDE

`mcu-sim` lleva dentro **un servidor de GDB**: el depurador se conecta a él
como a una sonda ST-LINK, y se pone puntos de ruptura, se ejecuta paso a
paso, se miran variables y se carga el programa con `load`.

1. **En el diálogo**, marca **`--gdb`** —el depurador habla con el núcleo por
   los pines SWD, como una sonda de verdad— o **`--gdb-dap`** —directo al
   núcleo, **mucho más rápido** paso a paso—. **`--port`** es el puerto, 3333
   si no se dice. Si el IDE va a cargar el programa, marca **«sin firmware»**
   en el firmware (§3.2).
2. **Lanzar** y, con el ritmo *tiempo real*, **Arrancar**. En la pestaña
   **mcu-sim** sale `[gdb] escuchando en localhost:3333` —`[gdb-dap]` con
   `--gdb-dap`—.
3. **Conectar el depurador** a `localhost:3333`:
   * con `arm-none-eabi-gdb`: `target extended-remote localhost:3333`,
     `monitor reset`, `monitor halt`, `load`, `break main`, `continue`;
   * con **STM32CubeIDE**: una configuración **GDB Hardware Debugging** —no
     la de ST-LINK—, con *Generic TCP/IP*, `localhost` y `3333`. En `mcu-sim`
     hay una lista para importar, `src/cubeide/simulador.launch`, y su
     `README.md` explica cada casilla y los errores típicos.

**El servidor de GDB vive en el tiempo simulado.** Antes de *Arrancar*, o con
la ventana en *Pausa*, el depurador no recibe respuesta: arranca y no pauses
desde la ventana mientras depuras —la pausa la hace el depurador—.

Un *step over* sobre la línea de un `while (1) { f(); }` no acaba nunca: la
llamada y el salto de vuelta son de la misma línea, y GDB sigue hasta salir
de ella, como con una placa. Desde ahí, *step into* o un punto de ruptura.

## 6. El puerto serie

Un **puente serie** lleva una USART del MCU a **un puerto de red de tu
ordenador**, y un terminal se conecta a él: lo que el firmware manda con
`printf` sale en el terminal, y lo que se teclea le llega. La NUCLEO-F446RE
trae el suyo, como la placa de verdad: el **VCP** del ST-LINK, la **USART2**
(PA2/PA3), en el puerto **3355** con el protocolo RFC 2217.

1. **Lanzar** con una placa que lo tenga —`nucleo_f446re_vcp.xml` con
   `verif/fw/vcp_demo/vcp_demo.bin`, por ejemplo— y **Arrancar**. La pestaña
   **mcu-sim** dice dónde está: `serie VCP: RFC 2217 en localhost:3355,
   115200 8N1` (en un sistema, `serie N/VCP: ...`).
2. **Abrir un terminal**:
   * con Python: `python3 -m serial.tools.miniterm rfc2217://127.0.0.1:3355 115200`
     (en Windows, `py -m ...`); lleva los baudios del terminal al MCU;
   * con PuTTY, CoolTerm o `nc`, que no hablan RFC 2217: en el diálogo,
     **`--serie`** = `VCP=tcp:3355`, y en PuTTY una conexión *Raw* a
     `127.0.0.1`, puerto 3355, con el eco local desactivado.
3. **Para no perder lo primero** que imprime el firmware, marca
   **`--espera-terminal`**: el MCU no arranca hasta que haya un terminal
   conectado a cada puente.

**`--serie`** dice adónde da cada puente —`ID=memoria`, `tcp:PUERTO`,
`rfc2217:PUERTO`, o como cliente de un servidor—; en el diálogo, varios
separados por «;». Un **puerto COM** de verdad para otro programa, los
problemas típicos y cómo añadir un puente a una placa propia están en
`doc/puente_serie.md` de `mcu-sim` ([anexo B](#anexo-b-el-resto-de-la-documentación)).

## 7. Si algo no va

| Síntoma | Qué mirar |
| :--- | :--- |
| El diálogo dice «No se pueden leer las opciones de mcu-sim» | La ruta del **Ejecutable**. Se puede lanzar igual con la placa, el firmware y lo escrito a mano |
| «mcu-sim ha terminado sin llegar a conectarse» | La pestaña **mcu-sim**: dice el porqué —una placa que no está, un error en el XML, un firmware que no se encuentra—. Las rutas son relativas al **Directorio** |
| No pasa nada en la ilustración ni en el panel | Falta **Arrancar**, o está en **Pausa** |
| El depurador no conecta o no responde | Que la pestaña **mcu-sim** diga `escuchando en localhost:3333`, que la simulación esté arrancada y sin pausa, y en STM32CubeIDE, *GDB Hardware Debugging* con el `arm-none-eabi-gdb` de CubeIDE (§5) |
| Un LED parpadea demasiado deprisa para verlo | El ritmo: *tiempo real* o *a la mitad* |
| El terminal no recibe lo primero que imprime el programa | `--espera-terminal` (§6) |
| No se puede mover ninguna placa en la ilustración | Hay que arrancar la ventana con `--edit` (§4.3) |

---

## Anexo A. La línea de órdenes

### A.1 Las opciones de mcu-sim-gui

```
mcu-sim-gui [--config FICHERO] [--puerto N] [--lanza] [--edit]
```

| Opción | Qué hace |
| :--- | :--- |
| `--config FICHERO` | La configuración, en JSON (la plantilla es [`config.ejemplo.json`](../config.ejemplo.json)). Por omisión, `config.json` en el directorio actual si lo hay, y si no, en el de configuración del usuario. Si no existe, se crea al lanzar por primera vez |
| `--puerto N` | Dónde escucha la ventana, por encima de la configuración; 3344 por omisión. Si está cogido, escucha en otro sin decir nada y se lo pasa a `mcu-sim` |
| `--lanza` | Lanza `mcu-sim` nada más abrir, con la configuración, sin diálogo |
| `--edit` | Deja **editar la ilustración**: colocar, girar y escalar las placas, el lienzo, las líneas en tramos rectos, guardarlo en el XML y abrir otro dibujo. Sin ella, solo se enseñan y se esconden las conexiones |
| `-h`, `--help` | La ayuda; `--help-all`, también las opciones de Qt |
| `-v`, `--version` | La versión |

`mcu-sim` también se puede lanzar a mano desde otra consola, y se conecta a
una ventana que esté escuchando:

```
mcu-sim placa.xml firmware.bin --gui            # o --gui localhost:N
```

### A.2 Las opciones de mcu-sim en el diálogo

El diálogo **no lleva una lista escrita**: se la pide al propio `mcu-sim`
(`mcu-sim --argumentos`), así que ofrece exactamente las de la versión que
se use. Las de hoy:

| Opción | Qué hace |
| :--- | :--- |
| `placa` | El circuito, en XML (§3.1). Obligatoria |
| `firmware` | La imagen para la Flash (§3.2) |
| `--firmware ID=FICHERO` | El firmware de un MCU concreto —`N/u0`—, también con varios; lo escriben las filas del firmware |
| `--sin-firmware[=ID]` | Ese MCU —sin ID, todos— sin firmware; lo escribe la casilla «sin firmware» |
| `--ms` | Cuánto tiempo simular, en ms. Con la ventana, sin decirlo no hay fin: se para con **Parar** |
| `--mcu TIPO` | El MCU, si la placa no lo dice; si lo dice, manda sobre él |
| `--gdb` | El servidor de GDB, por los pines SWD (§5) |
| `--gdb-dap` | El servidor de GDB, directo al núcleo: más rápido (§5) |
| `--port N` | El puerto del servidor de GDB; 3333 por omisión |
| `--traza-gdb` | Escribe cada paquete que llega del depurador: para ver qué pasa cuando no conecta |
| `--serie ID=DESTINO` | Adónde da cada puente serie (§6); varios, separados por «;» |
| `--espera-terminal` | No arranca el MCU hasta que haya un terminal en cada puente serie por red |
| `--valida` | Solo comprueba el circuito, sin simular |
| `--ondas` | Con la onda cuadrada de los relojes internos |

Con la ventana no se ofrecen `--gui` —la pone ella— ni `--tiempo-real` —el
ritmo lo dice ella—. Lo que no esté, en **A mano**. La referencia completa:
`mcu-sim --help`, y `mcu-sim --help COMPONENTE` para una pieza.

## Anexo B. El resto de la documentación

**De esta ventana**, en este repositorio:

* [`README.md`](../README.md): qué es, por qué son dos programas, cómo se
  compila —también en Windows con MSYS2— y la configuración;
* [`notas_release.md`](notas_release.md): los paquetes, y cómo se arrancan
  en cada sistema;
* [`config.ejemplo.json`](../config.ejemplo.json): la configuración,
  comentada;
* [`plan_dos_procesos.md`](plan_dos_procesos.md): el plan, fase a fase, con
  el detalle de cada cosa que hace la ventana;
* [`analisis-uso-ilustraciones.md`](analisis-uso-ilustraciones.md): la
  ilustración, los dibujos de las placas y cómo se hacen;
* [`analisis_disposicion_ilustracion.md`](analisis_disposicion_ilustracion.md):
  colocar las placas, el lienzo, las líneas y guardarlo (`--edit`);
* [`protocolo.md`](protocolo.md): cómo hablan la ventana y el simulador.

**Del simulador**, en el repositorio de [`mcu-sim`](../../mcu-sim):

* [`README.md`](../../mcu-sim/README.md) y [`src/README.md`](../../mcu-sim/src/README.md):
  qué simula, las placas de ejemplo y la línea de órdenes;
* [`doc/parts.md`](../../mcu-sim/doc/parts.md): el formato de las placas y
  los sistemas, y el catálogo de piezas;
* [`doc/puente_serie.md`](../../mcu-sim/doc/puente_serie.md): el puerto serie,
  los terminales y los puertos COM;
* [`src/cubeide/README.md`](../../mcu-sim/src/cubeide/README.md): depurar con
  STM32CubeIDE, paso a paso y error a error;
* [`doc/multi_mcu.md`](../../mcu-sim/doc/multi_mcu.md): placas con varios MCUs;
* [`doc/compilacion.md`](../../mcu-sim/doc/compilacion.md) y
  [`doc/ejecutables.md`](../../mcu-sim/doc/ejecutables.md): compilarlo y sus
  paquetes.
