# Guía de usuario de mcu-sim-gui

*Lo necesario para usar el programa: elegir el circuito y el firmware, lanzar
la simulación, depurar con GDB y hablar con el puerto serie del MCU. Las
opciones de la línea de órdenes están en el [anexo A](#anexo-a-la-línea-de-órdenes),
y el resto de la documentación, en el [anexo B](#anexo-b-el-resto-de-la-documentación).*

---

## 1. Qué es, y qué hace falta

`mcu-sim-gui` es la ventana de **`mcu-sim`**, el simulador de
microcontroladores. Son **dos programas**:

* **`mcu-sim`** simula: el MCU, la placa y lo que lleva conectado, con tu
  firmware dentro;
* **`mcu-sim-gui`** lo lanza, lo enseña —un panel con cada pieza y una
  ilustración de las placas, con los LEDs que se encienden y los botones que
  se pulsan, y si la placa lo declara, una vista 3D— y deja tocarlo.

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
| **contenido** de cada memoria | El fichero de donde se carga y donde se guarda lo que guarda cada memoria no volátil (§3.3) |
| las demás | Las opciones de `mcu-sim`: depuración (§5), puerto serie (§6)... Cada una explica lo que hace al pasar el ratón por encima. La lista entera, en el [anexo A.2](#a2-las-opciones-de-mcu-sim-en-el-diálogo) |
| **A mano** | Cualquier otro argumento, tal cual, para lo que el diálogo no ofrezca |

Debajo de todo se ve **la línea de órdenes que va a salir**. «Lanzar» la
ejecuta; abajo, en la pestaña **mcu-sim**, sale todo lo que el simulador
escribe —el arranque, los avisos, un error si algo no le gusta—.

*Simulación ▸ Lanzar otra vez* (**Ctrl+R**) repite el último lanzamiento sin
diálogo, y *Detener mcu-sim* lo para: primero le pide que termine, como
**Parar**, para que guarde sus memorias (§3.3), y si en tres segundos no lo ha
hecho, lo mata.

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
| `stm32f3discovery.xml` | La STM32F3DISCOVERY (revisión E): la rosa de ocho LED, B1 y B2, el puerto serie del ST-LINK, y el giróscopo y la brújula con acelerómetro, que se mueven con los mandos de su **postura** (abajo) | `verif/fw/stm32f3/ejemplos_s4/BSP.bin`, el ejemplo de ST |
| `stm32f3discovery_d.xml` | La misma, de la revisión D: otros sensores, el mismo firmware | el mismo |
| `dos_f3discovery.xml` | Dos F3DISCOVERY cableadas, para los ejemplos de ST de dos placas | el que dice el XML |
| `stm32f3discovery_boot0.xml` | La F3DISCOVERY con BOOT0 a VDD —arranca el cargador de sistema— y un adaptador FT232RL en la USART1, para `stm32flash` | ninguno: se carga por el cargador |
| `msp_exp432p401r.xml` | La LaunchPad MSP-EXP432P401R de Texas Instruments | `verif/fw/msp432p4/p4_demo/p4_demo.out`, el ELF de Code Composer Studio |
| `msp_exp432p401r_edumkii.xml` y `msp_exp432e401y_edumkii.xml` | Las LaunchPad del MSP432P401R y del MSP432E401Y con la BoosterPack EDUMKII: joystick, pantalla, RGB, zumbador... | el que dice el XML |
| `nucleo_f446re_encoder_lcd_servo_2xmcc.xml` | La Nucleo con el servo, el encoder y la pantalla, y además un L298N con dos motorreductores con su encoder y una fuente de laboratorio, que enseña en su display lo que gastan | el que dice el XML |
| `cnc_3020.xml` | **La fresadora CNC 3020** entera: la Nucleo, la CNC Shield, tres drivers A4988, los motores, los husillos, los carros, el cabezal y la pieza (abajo) | el que dice el XML: `verif/fw/cnc/cnc.bin` |
| `nucleo_ihm07m1_db42.xml` | La Nucleo con la X-NUCLEO-IHM07M1 y un motor BLDC: seis pasos, senoidal o FOC, mandados por el puerto serie | el que dice el XML |
| `eje_canopen.xml` | Un eje de fresadora con su controlador por CANopen, y la Nucleo de maestro | el que dice el XML |

**La postura de la STM32F3DISCOVERY.** La pieza `MOV` es, en el dibujo, la
aguja de rumbo que hay entre U5 y U3 —gira con el rumbo: marca hacia dónde
apunta el borde norte de la placa, el de LD3—, y en el panel lleva tres
deslizadores: **alabeo** (el borde de LD7 hacia abajo), **cabeceo** (el borde
de LD3 hacia arriba) y **rumbo** (hacia dónde mira la placa). Debajo, el mando
de **inclinación**: un círculo con un punto que se arrastra hacia el lado que
baja —a la derecha, el este; arriba, el norte—, con el borde a 90 grados; el
doble clic deja la placa plana. No es un mando nuevo: mueve los deslizadores de
alabeo y cabeceo, y son ellos los que ordenan. Un clic en la aguja abre los tres
deslizadores encima del dibujo. Al moverlos la placa se inclina o gira hasta ahí, a 90 grados
por segundo, y eso es lo que leen el acelerómetro —la gravedad—, la brújula
—el norte— y el giróscopo —mientras gira, y solo mientras gira—. Con el
ejemplo `BSP` de ST: B1 cuando acabe la vuelta de la rosa y, al inclinar la
placa, se enciende el LED del lado que baja; B1 dos veces más, lo mismo con el
giróscopo, mientras se mueve.

**La fresadora 3020** se maneja con **G-code por su puerto serie** (§6): el
firmware de ejemplo habla como un GRBL, así que vale un terminal o un
programa como Candle, UGS o CNCjs. Por ejemplo:
* `$H` lleva los ejes a casa;
* `G0 X50 Y30` va ahí en rápido, y `G1 X80 F600`, a 600 mm/min;
* `M3 S1000` enciende el cabezal y `M5` lo apaga;
* `?` dice el estado.

En la ilustración se ve en planta y en alzado, con los carros que se mueven,
las cotas en sus displays y lo que la fresa va quitando de la pieza; y en tres
dimensiones, en la vista 3D (§4.4). Lo que entiende el firmware, al principio
de `verif/fw/cnc/main.c` de `mcu-sim`.

Para un montaje propio se copia uno parecido y se cambia: el formato está en
`doc/parts.md` de `mcu-sim` ([anexo B](#anexo-b-el-resto-de-la-documentación)).

### 3.2 El firmware

Es **la imagen binaria** que el compilador genera para el chip: un `.bin`
—o un Intel HEX, `.hex`— que se carga a partir del principio de la Flash
(0x08000000 en un STM32). También valen el **ELF** que deja el IDE —el `.out`
de Code Composer Studio, el `.elf` de STM32CubeIDE—, que dice él mismo dónde
va cada cosa, y el TI-TXT (`.txt`) de las herramientas de TI. En STM32CubeIDE se genera solo con *Project ▸ Properties ▸
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

### 3.3 Lo que guardan las memorias no volátiles

La Flash de un MCU, la EEPROM del MSP432E4, una EEPROM I2C (`I2cEeprom`), una
tarjeta SD (`SdCard`) o una NAND (`ExtNand`) guardan lo que tienen aunque se
quite la alimentación. Para que eso pase también **de una simulación a la
siguiente**, cada una puede tener un fichero **`contenido`**, en **Intel
HEX** (`.hex`): se carga al arrancar y se escribe al acabar. Si el fichero aún
no existe, la memoria arranca como siempre y el fichero se crea al acabar.
Sin fichero, la memoria se pierde al acabar, como siempre.

El diálogo le pregunta a `mcu-sim` qué memorias no volátiles lleva el circuito
y pone, bajo **«El contenido de cada memoria no volátil»**, una fila por cada
una —«N/u0 (STM32F446RE): Flash, OTP y bytes de opcion», «N/U5 (I2cEeprom):
EEPROM de 64 bytes»—,
con su fichero y su botón «…», que deja escribir el nombre de uno que todavía
no existe. **Vacía, se usa el que diga el XML** —el atributo `contenido` del
`<mcu>` o del `<componente>`, y si el sistema lo dice, el del sistema—, que sale
en gris de muestra. **Lo que se ponga aquí manda sobre los dos.**

Con la Flash de un MCU, primero se carga el contenido y **después el
firmware encima**: lo que el programa haya guardado en otras páginas se
conserva. Si no hay firmware, el MCU arranca con lo que traiga el contenido.

Con la memoria de un MCU va todo lo que guarda: en los STM32, también sus
bytes de opción, y en el F4 su OTP; en el MSP432P401R, la Flash de
información con su TLV y su BSL; en el MSP432E401Y, la EEPROM y los registros
que el firmware fija (BOOTCFG, USER_REG, FMPRE y FMPPE). Lo que sigue como de
fábrica no se escribe en el fichero. Si un STM32 queda con la lectura
protegida (RDP en el nivel 1 o 2), la opción `--restore-rdp` del diálogo
la devuelve a la de fábrica al arrancar. Y si un F4 tiene su OTP gastada
—bloques programados o bloqueados—, la opción `--restore-otp` la deja de
fábrica, todo a 0xFF, justo después de cargar su contenido: el chip de verdad
no la borra nunca, así que solo vale con un fichero de contenido (`mcu-sim`,
`doc/parts.md` §2.7).

El contenido **solo se guarda si la simulación acaba bien**: con **Parar**,
con su `--ms`, cerrando la ventana o con *Detener mcu-sim*. Si el proceso
muere de golpe —matado desde fuera—, el fichero se queda como estaba.

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
sobrecorriente, una orden que no se pudo aplicar—, la pestaña **mcu-sim** y
una por cada puente serie en memoria (§6.1).

### 4.2 El panel

Una caja por pieza, con lo que deja ver —un LED encendido, una corriente en
mA, una alarma en rojo— y lo que deja tocar: un **botón** se pulsa con el
ratón y, con el «switch» de al lado, se queda hundido hasta volver a tocarlo;
un **encoder** gira; un pulsador **rebota** como uno de verdad, y su rebote se
ajusta en marcha. *Vista ▸ Siempre encima* (**Ctrl+T**) deja la ventana a la
vista mientras se trabaja en el IDE.

### 4.3 La ilustración

*Vista ▸ Ilustración* (**Ctrl+I**) la abre en **su propia ventana**: cada
placa con su dibujo, y sobre él lo que pasa:
* los LEDs que brillan y los botones que se hunden al pulsarlos;
* el mando del encoder, que gira con la rueda del ratón, y el aspa del servo;
* la imagen de la pantalla;
* los displays de los aparatos —lo que gasta la fuente de laboratorio, las
  cotas de la fresadora—, escritos dentro;
* los carros de la fresadora, que se mueven, y la pieza, con lo mecanizado.

Ctrl+rueda acerca o aleja, y «Ajustar» vuelve a enseñarlo todo.

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

### 4.4 La vista 3D

Una placa que declara su forma en tres dimensiones —la fresadora 3020 de
`mcu-sim`, `placas/cnc_3020.xml`— enciende *Vista ▸ Vista 3D* (**Ctrl+3**),
que la abre en **su propia ventana**:
* la máquina en cajas y cilindros, con los carros que se mueven y los
  husillos que giran;
* la pieza como una superficie, con lo que la fresa va comiendo, del mismo
  color que en la ilustración.

La cámara se mueve con el ratón:
* arrastrar gira alrededor de adonde se mira;
* la rueda acerca y aleja;
* arrastrar con el botón derecho desplaza.

**«Vista inicial»** vuelve a donde dice la placa. Las alturas de la pieza solo
se piden con la ventana abierta: cerrada, no cuestan nada.

La vista 3D usa **Qt Quick 3D**, y la ventana puede compilarse sin ella
(`MCU_SIM_GUI_3D=OFF`, en el `README.md`): entonces la ventana 3D dice que esa
compilación no la lleva. Dibujarla depende de la tarjeta gráfica de cada
máquina —OpenGL en Linux, Direct3D 11 en Windows, Metal en macOS—: en una sin
nada de eso, la ventana 3D se queda en blanco o, con el Qt 6.4 de Linux, el
programa puede cerrarse al abrirla.

Los paquetes de las *Releases* la llevan, en las tres plataformas, con todo
lo que necesita dentro. `mcu-sim-gui --comprueba-3d` dice si se puede cargar
y de dónde saca sus módulos de QML: es lo primero que mirar si la ventana 3D
dice que no puede cargar su escena.

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

### 6.1 Sin terminal: la pestaña del puente

Con **`--serie VCP=memoria`** el puente no abre ningún puerto, y **la ventana
hace de terminal**. Cada puente en memoria tiene su **pestaña** abajo, con su
nombre (`VCP`, o `N/VCP` en un sistema), junto a *Avisos* y *mcu-sim*:

* **Arriba**, lo que pasa por la línea. Lo que llega del MCU va en **azul** y
  lo que se manda en **verde oliva**. Los dos iconos de la derecha (una flecha
  hacia la izquierda, lo recibido; hacia la derecha, lo enviado) cambian esos
  colores, en todas las pestañas a la vez, y la ventana los recuerda. CR, LF o
  CR+LF son un salto de línea. Un byte suelto que no se imprime sale como
  `\xNN`, y varios seguidos, juntos en un `\x{...}`, aunque lleguen en
  trozos; la barra sale doblada, `\\`. Es la notación de la entrada: lo que
  se ve se puede copiar a ella y sale lo mismo. Cuando
  se pasa de recibir a enviar a media línea, lo nuevo empieza en otra: es solo
  la vista, los bytes son los mismos. *Limpiar* borra lo que se ve.
* **Abajo**, la **entrada**. Se escribe en ASCII, y los bytes que no lo son, en
  hexadecimal, intercalados donde se quiera, de dos maneras:
  * `\xNN` es **un** byte, con dos cifras justas: lo que sigue ya es texto.
    `\x41BC` son tres bytes: 0x41, «B» y «C».
  * `\x{...}` son **varios**, con un número par de cifras entre llaves:
    `\x{41BC}` son dos, 0x41 y 0xBC, y `\x{0D0A}` es lo mismo que `\x0D\x0A`.

  Así, `AT\x0D\x0A` o `\x{0103000A0001}`. Valen también `\r`, `\n`, `\t`,
  `\0` y `\\` para la barra. Debajo se ve lo que saldría, en hexadecimal, o
  por qué no se puede.
* **Las casillas**: *Enviar CR (\r, \x0D)* y *Enviar LF (\n, \x0A)* ponen esos
  bytes al final, primero CR y luego LF. *Enviar CRC* pone el de la lista,
  calculado sobre lo escrito y **delante** de CR y LF, que no entran nunca en
  él:

  | CRC | Qué añade |
  | :--- | :--- |
  | Modbus RTU (CRC-16) | Dos bytes, el bajo primero (polinomio 0xA001, inicial 0xFFFF) |
  | Modbus ASCII (LRC) | Dos caracteres hexadecimales: el LRC de las parejas de cifras que van detrás de `:`. Lo escrito tiene que ser una trama ASCII, como `:1103006B0003` |
  | Suma módulo 256 | Un byte |

* **Enviar**, o Intro, manda. Lo escrito se queda seleccionado: Intro otra vez
  lo repite, y escribir lo sustituye. Solo se puede con la simulación en
  marcha.

Una pestaña que no se está mirando y recibe algo pone su nombre en azul. La
barra entre el panel y las pestañas se arrastra.

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
| *Vista ▸ Vista 3D* está apagado | La placa no declara su forma en 3D: hoy solo la lleva la fresadora (§4.4) |
| La ventana 3D dice que no puede cargar su escena | `mcu-sim-gui --comprueba-3d` dice qué falta y dónde lo ha buscado (§4.4); o esa compilación no lleva la vista 3D |

---

## Anexo A. La línea de órdenes

### A.1 Las opciones de mcu-sim-gui

```
mcu-sim-gui [--config FICHERO] [--puerto N] [--lanza] [--edit]
mcu-sim-gui --comprueba-3d
```

| Opción | Qué hace |
| :--- | :--- |
| `--config FICHERO` | La configuración, en JSON (la plantilla es [`config.ejemplo.json`](../config.ejemplo.json)). Por omisión, `config.json` en el directorio actual si lo hay, y si no, en el de configuración del usuario. Si no existe, se crea al lanzar por primera vez |
| `--puerto N` | Dónde escucha la ventana, por encima de la configuración; 3344 por omisión. Si está cogido, escucha en otro sin decir nada y se lo pasa a `mcu-sim` |
| `--lanza` | Lanza `mcu-sim` nada más abrir, con la configuración, sin diálogo |
| `--edit` | Deja **editar la ilustración**: colocar, girar y escalar las placas, el lienzo, las líneas en tramos rectos, guardarlo en el XML y abrir otro dibujo. Sin ella, solo se enseñan y se esconden las conexiones |
| `--comprueba-3d` | Carga la vista 3D, sin abrir la ventana, dice si se puede y de dónde saca sus módulos de QML, y sale: 0 si se carga, 1 si no, 3 si esta compilación no la lleva (§4.4) |
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
| `--contenido ID=FICHERO` | El fichero Intel HEX de la memoria no volátil `ID` —`N/u0`, `N/U5`—; lo escriben las filas del contenido (§3.3) |
| `--restore-otp[=ID]` | La OTP de ese STM32F4 —sin ID, de todos los que tienen contenido— a la de fábrica, bloqueos incluidos, tras cargar su contenido. Sin contenido es un error (§3.3) |
| `--restore-rdp[=ID]` | El RDP de ese STM32 —sin ID, de todos— a su valor de fábrica tras cargar su contenido, aunque esté en el nivel 2 (§3.3) |
| `--ms` | Cuánto tiempo simular, en ms. Con la ventana, sin decirlo no hay fin: se para con **Parar** |
| `--mcu TIPO` | El MCU, si la placa no lo dice; si lo dice, manda sobre él |
| `--gdb` | El servidor de GDB, por los pines SWD (§5) |
| `--gdb-dap` | El servidor de GDB, directo al núcleo: más rápido (§5) |
| `--openocd` | Una sonda SWD para OpenOCD (`remote_bitbang`) en los pines; puerto 3335 si no se dice `--port` |
| `--cmsis-dap` | Una sonda CMSIS-DAP por TCP para OpenOCD (`cmsis-dap tcp`); puerto 4441 si no se dice `--port` |
| `--port N` | El puerto del servidor de GDB o de la sonda; 3333 por omisión |
| `--traza-gdb` | Escribe cada paquete que llega del depurador: para ver qué pasa cuando no conecta |
| `--serie ID=DESTINO` | Adónde da cada puente serie (§6); varios, separados por «;» |
| `--espera-terminal` | No arranca el MCU hasta que haya un terminal en cada puente serie por red |
| `--usb [ID=]DESTINO` | El USB de un MSP432E4 o de un STM32F303 como dispositivo: un PC que enumera el puerto serie USB del firmware y lo lleva a un puerto, como `--serie` |
| `--swo DESTINO` y `--swo-bitrate N` | Adónde va la traza SWO de la sonda XDS110 de la placa, y a cuántos bit/s |
| `--rom [ID=]VALOR` | La ROM de DriverLib de un MSP432: la sustituta (`r1`, por omisión), `ninguna`, o la de TI de un fichero |
| `--bsl [ID=]VALOR` | El cargador de arranque de un MSP432P4: el sustituto, ninguno o el de TI |
| `--tlv [ID=]FICHERO` | La tabla TLV de calibración de un MSP432P4, volcada de una placa |
| `--eth [ID=]DESTINO` | El Ethernet de un MSP432E4: en memoria, o hacia un servidor con el protocolo de red de QEMU |
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
* [`doc/analisis_cnc.md`](../../mcu-sim/doc/analisis_cnc.md): la fresadora
  CNC, fase a fase: los motores, sus drivers, la mecánica, el firmware, la
  pieza y la vista 3D;
* [`doc/compilacion.md`](../../mcu-sim/doc/compilacion.md) y
  [`doc/ejecutables.md`](../../mcu-sim/doc/ejecutables.md): compilarlo y sus
  paquetes.
