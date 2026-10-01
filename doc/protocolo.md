# El protocolo entre `mcu-sim` y `mcu-sim-gui`

Versión **1**. La definición ejecutable está en `src/protocolo.h`; esto es lo
que ese fichero no puede decir: el porqué, la máquina de estados y qué pasa
cuando algo va mal.

> **Las copias vendidas.** `mcu-sim` lleva su propia copia de `src/protocolo.h`
> y de `src/proto_io.h` —el código que escribe y lee el marco— en
> `src/common/`. Tienen que ser idénticas byte a byte, y `make gui-proto` lo
> exige en el CI de `mcu-sim`. Un protocolo cuyos dos extremos discrepan en un
> `uint16_t` no falla al compilar: falla en marcha y sin decir por qué. Y con
> el lector compartido, un error al leer el marco no puede estar en un extremo
> solo. Por eso un cambio aquí se sube primero a este repositorio y después a
> `mcu-sim`.

---

## 1. Las decisiones, y por qué

**Una sola conexión TCP.** Se consideró una segunda para los avisos y se
descartó: dos sockets son dos ordenaciones distintas, y entonces hay que decidir
qué pasa cuando el aviso «he aplicado la orden» llega antes que la instantánea
que la refleja. Con una sola conexión ese problema no existe, y el volumen no lo
pide: la placa del enunciado tiene del orden de cuarenta observables, que a
60 Hz y 8 bytes por muestra son **19 kB/s** [`analisis_gui.md` §6.3].

**`mcu-sim` es el CLIENTE; la GUI escucha.** Es al revés de lo que uno diría, y
tiene tres razones:

1. quien lanza el proceso es la GUI, así que **ya está escuchando** cuando el
   modelo arranca: no hay carrera de arranque ni espera con reintentos;
2. `--gui host:puerto` quiere decir *dónde está la ventana*, que es la
   información que el que arranca puede dar y el otro no;
3. y si el modelo escuchara, dos simulaciones a la vez se pelearían por el
   puerto — que es justo la incidencia de soporte que `analisis_gui.md` §18.2
   pone como principal argumento en contra de los dos procesos.

El modo contrario —el modelo escuchando, para que una GUI se enganche a una
simulación ya en marcha— es útil y está previsto en la **fase 9**, con un
argumento distinto (`--gui-escucha puerto`). No es la vía normal.

**Marco con longitud explícita.** La longitud está en la cabecera aunque el tipo
ya la determine en casi todos los mensajes. Esa redundancia es lo que permite
que un extremo **se salte** un mensaje que no conoce en vez de morirse, y es
toda la estrategia de versionado: añadir un tipo de mensaje **no** sube la
versión del protocolo.

**Little-endian, y dicho.** Los dos extremos son x86 o ARM en modo
little-endian. No hay conversión de orden de bytes y no se finge que la haya. Si
algún día hay un extremo big-endian esto hay que arreglarlo, y arreglarlo son
seis conversiones que hoy no están.

**Sin autenticación, y por eso el aviso.** Quien habla por este socket puede
leer el estado de la simulación y accionar sus mandos. Por omisión es
`localhost`, que es lo mismo que hacen los dos servidores de GDB del proyecto y
por el mismo motivo. Si `--gui` apunta a un host que no es de bucle local,
`mcu-sim` **lo dice por la salida de error** antes de conectarse. Se permite
—hace falta para una GUI en otra máquina— pero no en silencio.

---

## 2. El marco

```
+---------+---------+---------+-----------+------------+----------------+
| magia   | version | tipo    | longitud  | secuencia  | cuerpo         |
| uint32  | uint16  | uint16  | uint32    | uint32     | `longitud` B   |
+---------+---------+---------+-----------+------------+----------------+
   4         2         6          8            12           16 -> 16+n
```

* `magia` = `0x3147534D` (`MSG1` en little-endian). Un extremo que lee otra cosa
  **cierra la conexión**: no intenta resincronizar. Es el caso de «alguien
  apuntó su navegador al puerto», y seguir leyendo solo prolonga el malentendido.
* `version` es la que está **en uso en esta conexión**, no la máxima que se sabe.
  **Antes de negociarla es la 1**, en los dos sentidos: es la versión en la
  que viaja el saludo (`VERSION_SALUDO` en `proto_io.h`). Así un extremo nuevo
  puede ofrecer `protocolo_max=2` a uno viejo sin que el viejo rechace el
  mensaje que se lo ofrece por traer un 2 en la cabecera. Negociada en
  `T_VERSION`, los dos pasan a la elegida, y desde entonces un mensaje con otra
  es un error.
* `longitud` ≤ `CUERPO_MAX` (8 MiB). Más que eso se trata como corrupción y se
  cierra: sin ese techo, una longitud corrupta es una petición de memoria de dos
  gigas.
* `secuencia` es un contador propio de cada sentido, desde 0. No se usa para
  nada en marcha; sirve para que una traza diga si se perdió algo, que es la
  pregunta que uno se hace cuando dos capturas no cuadran.

**Tipos:** los del rango `0x0000-0x7FFF` van del modelo a la pantalla; los de
`0x8000-0xFFFF`, al revés. Recibir un tipo del propio rango es un error de
conexión —dos GUIs habladas entre sí— y se cierra diciéndolo.

**Tipo desconocido:** se salta por longitud y **se sigue**. No es un error.

---

## 3. El saludo, que ocurre antes de que exista la simulación

Esta es la parte que hace verdadera la frase «la simulación no empieza hasta que
la GUI lo diga»: no es que empiece y se quede quieta, es que **`sc_start()` no
se ha llamado todavía**. Todo el saludo ocurre en `sc_main`, después de
construir la placa y antes de arrancar el núcleo de SystemC, con lecturas
bloqueantes normales. No hay ni un proceso de SystemC involucrado, y por tanto
tampoco ninguna de las trampas de §4.1 del análisis.

```
mcu-sim                                   mcu-sim-gui
   |                                            |
   |  (conecta a host:puerto)                   |
   |------------------ T_HOLA ----------------->|
   |<---------------- T_VERSION ----------------|
   |----------------- T_PLACA ----------------->|   XML de --netlist
   |---------------- T_CATALOGO --------------->|   observables y mandos
   |----------------- T_LISTO ----------------->|   "elaborado y esperando"
   |<--------------- T_SUSCRIBE ----------------|   qué quiero ver, cada cuánto
   |<--------------- T_ORDENES  ----------------|   (opcional, 0..n veces)
   |<--------------- T_ARRANCA  ----------------|
   |                                            |
   |            ... sc_start() ...              |
```

**`T_HOLA`** — cuerpo de texto, una línea `clave=valor` por renglón, UTF-8:

```
protocolo_max=1
mcu_sim=0.9.0
pid=48211
placa=placas/discovery_min.xml
mcu=STM32F407VG
firmware=verif/fw/blinky/blinky.bin
argumentos=--gui localhost:3344 --tiempo-real
```

Texto y no POD a propósito: es el mensaje que más va a crecer, y crecer no debe
romper a nadie. Una clave desconocida se ignora.

**`T_VERSION`** — la GUI contesta con la versión que va a hablar, que debe ser
≤ `protocolo_max`. Mismo formato:

```
protocolo=1
gui=0.1.0
```

Si la GUI no sabe hablar ninguna versión que el modelo ofrezca, manda
`protocolo=0` y cierra; el modelo lo dice por la salida de error y termina con
código distinto de cero. **La negociación es de una sola vuelta**: el que se
conecta propone el máximo, el que escucha elige.

**`T_PLACA`** — el XML de `--netlist`, entero y tal cual. No se inventa un
formato: ese volcado ya existe, ya está versionado y la suite ya lo compara
consigo mismo (`git diff --exit-code placas/banco.xml`). La GUI construye sus
widgets a partir de él **sin conocer ni un tipo de C++**, que es todo el
argumento de §2.2 del análisis.

**`T_CATALOGO`** — lo que el XML todavía no dice: qué se puede **ver** y qué se
puede **hacer**. También XML, en el mismo estilo:

```xml
<catalogo>
  <pieza idx="0" id="LD_VERDE" tipo="Led">
    <observable idx="0" id_obs="0" nombre="encendido" unidad=""   min="0" max="1" interesante="si"/>
    <observable idx="1" id_obs="1" nombre="corriente" unidad="mA" min="0" max="25" interesante="no"/>
  </pieza>
  <pieza idx="1" id="B1" tipo="Button">
    <observable idx="0" id_obs="2" nombre="pulsado" unidad="" min="0" max="1" interesante="si"/>
    <mando idx="0" nombre="pulsar" tipo="boton" min="0" max="1"/>
  </pieza>
</catalogo>
```

Tres cosas que merecen nombre propio:

* **`idx` de pieza es el índice en el inventario** de `ExtPartBase`, y vale para
  toda la ejecución porque la elaboración de SystemC es estática: después de
  ella no se construye ni se destruye ninguna pieza. Por eso los identificadores
  no se renegocian nunca.
* **`id_obs` es plano y global**, no un par empaquetado en bits. Se probó
  `(pieza << 6) | observable` y se descartó: pone un techo de 64 observables por
  pieza que nadie va a recordar el día que se pase.
* **`interesante`** es la sugerencia de la pieza, no una orden. Un LED sugiere
  `encendido` y no `corriente`; la GUI decide, y lo que decide lo dice en
  `T_SUSCRIBE`. Sin esto la pantalla de una placa con cuarenta piezas nace
  ilegible.

**`T_LISTO`** — sin cuerpo. Quiere decir: *la placa está construida, el catálogo
es el que has recibido, y estoy parado esperándote.*

**La espera hasta `T_ARRANCA`** no tiene plazo y no gasta CPU: es un `select`
que duerme hasta que llega algo. Mientras tanto el modelo:

* contesta **`T_PING`** con `T_PONG`;
* acepta **`T_PARA`**: manda `T_FIN` con motivo `M_PARA` y el tiempo simulado
  en cero, y termina **sin haber simulado nada**;
* acepta **`T_SUSCRIBE`** y **`T_ORDENES`**, que es donde una secuencia
  enviada antes de arrancar se vuelve reproducible (§5). Del `T_SUSCRIBE` vale
  el último, y se aplica **antes de `sc_start()`**: la secuencia de
  instantáneas se repite entonces al picosegundo de una ejecución a otra.
  *`T_ORDENES` se lee y se ignora hasta la fase 5.*

**Los avisos de la placa** —lo que `mcu-sim` encuentra al validar lo eléctrico
y los puentes serie— llegan como `T_AVISO` **entre `T_CATALOGO` y `T_LISTO`**,
con origen `placa` y `t_sim_ns` = 0, también con `--valida`. Es cuando sirven:
antes de pulsar «arranca».

**Los ids casan.** El `id` de cada `<pieza>` del catálogo es el mismo que el del
`<componente>` de la placa: lo pone la placa. Es lo que permite a la ventana
juntar los dos XML sin conocer ningún tipo.

**`T_HOLA` lleva `modo=`**: `simula`, o `valida` si `mcu-sim` se lanzó con
`--valida`. Con `--valida` el saludo es más corto: tras `T_CATALOGO` llega
`T_FIN`, sin `T_LISTO` y sin esperar a nada. Sirve para que la ventana enseñe
una placa sin simularla. Una placa con errores no llega a conectarse: `mcu-sim`
termina antes, con código 2 y la explicación en la salida de error.

**Los plazos.** Hay uno solo, y lo pone el modelo: si la GUI no contesta a
`T_HOLA` en **10 s**, termina con código 2. Una GUI que no contesta al saludo no
va a contestar a nada.

---

## 4. En marcha

### 4.1 Del modelo a la pantalla

**`T_INSTANTANEA`** — `CabInstantanea` + `n` × `Muestra`. La publica un
`SC_THREAD` que despierta cada `periodo_ns` de **tiempo simulado**, recorre los
observables suscritos y escribe. Medido en §3.3 del análisis: a 60 Hz el coste
sobre la simulación no es medible.

El campo `perdidas` es la parte honesta: si el socket no traga, **las
instantáneas se tiran** —son muestras, la siguiente dice lo mismo y mejor— y
aquí se cuenta cuántas, para que la GUI pueda enseñar que va por detrás en vez
de mentir con una gráfica continua. Se tiran las **nuevas**, no las viejas: la
primera que vuelve a entrar lleva en `perdidas` cuántas faltan justo delante de
ella, y eso solo es verdad si lo que falta es lo de después.

**En qué instantes.** En los múltiplos del periodo contados **desde t = 0**, no
desde el instante en que llegó la suscripción. Así dos ejecuciones con la misma
suscripción muestrean en los mismos instantes aunque la suscripción llegue en
momentos distintos.

**Y qué ve una instantánea de su propio instante.** Si una orden y una muestra
caen en el mismo instante simulado, **la muestra ve la orden aplicada**. No ve
necesariamente su consecuencia eléctrica —un LED que se enciende porque se
pulsó un botón tarda deltas en enterarse—; esa la verá la siguiente. Hace falta
decirlo porque SystemC no fija el orden en que despierta dos procesos en el
mismo instante: sin una regla, la respuesta dependería de la versión de la
biblioteca. El modelo la cumple con un delta de espera antes de muestrear.

**`T_AVISO`** — `CabAviso` + el `id` de `SC_REPORT` + el texto. Es la tubería por
la que sale todo lo que hoy va a la consola: los avisos de «esto no está
modelado», los de placa (dos piezas conduciendo el mismo nodo, un pad no
soldado, un reloj por encima de su máximo) y los errores del modelo. **Nunca se
tira uno**: van a una cola acotada, y si esa cola se llena la conexión se cierra
con un `T_FIN` de motivo `M_ERROR`. Perder un aviso es peor que perder la
conexión, porque un aviso perdido se parece mucho a un modelo que funciona.

Las reglas exactas, desde la fase 4:

* va a la ventana **lo que iría a la consola**: lo que lleva `SC_DISPLAY`, y
  los errores y fatales. Lo silenciado (`SC_DO_NOTHING`) sigue silenciado. Y la
  consola lo sigue enseñando: el aviso se **añade**, no se desvía;
* menos los informativos del propio núcleo de SystemC —«Simulation stopped by
  user»—, que no son del modelo: para eso está `T_FIN`;
* la cola es de **1000**. Si se llena, `T_FIN` con `M_ERROR` va **delante** de
  todo lo que no ha salido, se cierra la conexión y **la simulación sigue**
  hasta su ventana, como si la ventana se hubiera ido. El código del `T_FIN` es
  0: el proceso no termina por esto.

**`T_ESTADO`** — los dos relojes, el simulado y el de pared desde `T_ARRANCA`,
la fase y los deltas; el factor entre ellos lo calcula quien lo lea. Va
**detrás de cada tanda de instantáneas**, y si en **250 ms de reloj de pared**
no ha salido ninguno —sin suscripción, por ejemplo—, uno suelto. Es lo que
contesta a «¿se ha colgado?» en el único caso donde la respuesta es no
[`analisis_gui.md` §3.4]. Al terminar, uno último con `F_TERMINADA` justo antes
de `T_FIN`.

**El atasco, en números.** Lo que no ha salido no pasa de **256 kB**. Por encima,
las instantáneas se quedan en la cola del muestreador, que cuando se llena tira
las nuevas y las cuenta en `perdidas`; los avisos esperan en la suya.

**`T_ORDEN_HECHA`** — véase §5.

**`T_FIN`** — motivo y código de salida. Después de mandarlo el modelo cierra.

### 4.2 De la pantalla al modelo

Las atiende el **enlace** (`parts/enlace_gui.h` en `mcu-sim`), un `SC_THREAD`
que lee el socket sin bloquear cada 100 µs de tiempo simulado —exactamente el
mismo patrón que los servidores de GDB (`common/gdb_rsp.h`)— y que, en la misma
vuelta, escribe lo del sentido contrario. Es un proceso más con la simulación
en marcha, así que con `--gui` hay más deltas que sin él; el modelo hace lo
mismo. Sin `--gui` el enlace se construye y no despierta nunca.

**`T_ORDENES`, `T_PAUSA`, `T_SIGUE`, `T_PASO` y `T_PARA` en marcha** se leen
enteros y se ignoran hasta las fases 5 y 6.

**Y eso trae la trampa de siempre, escrita aquí para que no sorprenda:** ese
proceso solo corre **si el tiempo simulado avanza**. Con la simulación en pausa
no hay tiempo que avance y el socket no se atiende, así que «pausado» **no
puede** ser `sc_start()` sin más: el modelo en pausa sigue troceando rodajas
cortas de tiempo simulado sin dejar correr al resto del modelo. Es lo mismo que
ya hace `sim_main.cpp` hoy con `for (;;) espera(sc_time(1, SC_MS));` cuando hay
un stub de GDB esperando.

**`T_SUSCRIBE`** puede llegar tantas veces como quiera: reemplaza a la anterior.
Cambiar de pestaña en la GUI es volver a suscribirse. Si trae un `id_obs` que no
existe se rechaza **entera** y la anterior sigue en pie: una suscripción a
medias es peor que ninguna, porque la pantalla creería estar viendo lo que
pidió. El rechazo se dice con un `T_AVISO` que nombra el id que no existe, y lo
mismo un `T_SUSCRIBE` que no mide lo que dice. Una suscripción vacía apaga las
instantáneas.

**`T_PASO`** solo tiene sentido con `RIT_DEMANDA`. Con cualquier otro ritmo se
contesta con un `T_AVISO` de nivel `N_AVISO` y se ignora.

**`T_PARA`** es un final ordenado: el modelo termina la rodaja, manda `T_FIN` y
llama a `sc_stop()`. **`sc_stop()` es definitivo** —no hay «volver a arrancar»—
y por eso `T_PAUSA` y `T_PARA` son mensajes distintos y no uno con un booleano.

---

## 5. Las órdenes, que es la parte con miga

```cpp
struct Orden { uint64_t t_sim_ns; uint16_t pieza, mando; float valor; };
```

Un `T_ORDENES` lleva **una o más** `Orden`. Dentro del mensaje:

| | `t_sim_ns` significa |
| :--- | :--- |
| la **primera**, si el mensaje llega **antes** de `T_ARRANCA` | instante **absoluto** desde el inicio de la simulación |
| la **primera**, si la simulación **ya corre** | tiempo **relativo** al instante en que el modelo la saca de la cola |
| **las demás** | tiempo **transcurrido desde la anterior** |

Como el campo no tiene signo, los instantes absolutos que resultan son
**monótonos por construcción**: no hay forma de expresar una orden que vaya
hacia atrás. Un delta de cero es legal y quiere decir «en el mismo instante
simulado, y en el orden en que vienen en el mensaje».

**El ejemplo del enunciado**, enviado antes de arrancar, con la pieza 3 un
pulsador normalmente abierto y su mando 0 el estado:

```cpp
{ {1000000000, 3, 0, 1.0},   // pulsa en t = 1,00 s
  { 500000000, 3, 0, 0.0},   // suelta en t = 1,50 s   (la pulsación dura 0,5 s)
  {2500000000, 3, 0, 1.0},   // pulsa en t = 4,00 s
  { 220000000, 3, 0, 0.0} }  // suelta en t = 4,22 s
```

y el modelo responde con cuatro `T_ORDEN_HECHA` —a medida que las aplica, no de
golpe— con los instantes absolutos 1,00 s, 1,50 s, 4,00 s y 4,22 s.

**Por qué deltas y no instantes absolutos en todas.** Porque el caso que hay que
hacer fácil es «esta secuencia, empezando ahora», y con instantes absolutos eso
obliga a la GUI a saber en qué instante simulado va el modelo —que no lo sabe
con exactitud, porque va por detrás— y a rehacer la aritmética. Con deltas, la
misma secuencia vale antes de arrancar y en marcha, y la única cuenta la hace
quien tiene el dato bueno.

**Lo que esto le hace al determinismo, que es la propiedad más valiosa del
proyecto.** Una secuencia enviada **antes** de `T_ARRANCA` es reproducible al
picosegundo: dos ejecuciones dan el mismo tiempo simulado y el mismo resultado.
Una orden enviada **en marcha** aterriza donde el reloj de pared, la carga de la
máquina y la latencia del socket decidan, y esa ejecución **no se puede
repetir**. No es un defecto que se pueda arreglar: es lo que significa tener una
persona dentro del lazo [`analisis_gui.md` §9].

Lo que sí se puede hacer, y es la razón de que `T_ORDEN_HECHA` exista, es
**grabar** lo que pasó con sus instantes reales y volver a ejecutarlo después
como una secuencia absoluta, sin GUI ninguna. Un fallo encontrado a mano se
convierte así en un caso reproducible, y de ahí en una prueba de la suite, que
es la moneda de este proyecto.

**Validación.** Una orden cuya pieza no existe, cuyo mando no existe o cuyo
valor se sale del rango declarado **no se descarta en silencio**: se contesta con
`T_ORDEN_HECHA` y el `resultado` correspondiente, y en el caso del rango se
recorta y además se manda un `T_AVISO`. El silencio es lo que convierte un error
de la GUI en una tarde perdida mirando el modelo.

---

## 6. Qué pasa cuando algo va mal

| Situación | Qué hace `mcu-sim` | Qué hace `mcu-sim-gui` |
| :--- | :--- | :--- |
| No hay nadie escuchando en `host:puerto` | lo dice por la salida de error y **termina con código 2**. No reintenta: si la GUI lo lanzó, la GUI estaba escuchando | — |
| La GUI no contesta a `T_HOLA` en 10 s, contesta `protocolo=0`, elige una versión que no se le ofreció o no empieza por `T_VERSION` | lo dice por la salida de error y **termina con código 2** | — |
| El modelo ofrece una `protocolo_max` sin ninguna versión común | — | contesta `protocolo=0` y cierra |
| La GUI cierra la conexión **antes** de `T_ARRANCA` | lo dice y **termina con código 2**, sin simular: ya no hay nadie que vaya a decir «arranca» | — |
| La GUI cierra la conexión en marcha | **sigue simulando** hasta agotar su ventana y termina con normalidad. No se muere ni se queda colgado | — |
| El modelo se rinde (`muere()`) con la GUI conectada | manda `T_FIN` con `M_ERROR` y código 2 antes de irse | lo enseña |
| `mcu-sim` muere | — | lo ve por el `QProcess` y por el socket cerrado; enseña el código de salida y lo que quedara en la salida de error |
| Magia mala, versión imposible, longitud > `CUERPO_MAX` | cierra diciendo por qué | igual |
| Tipo de mensaje desconocido | se salta por longitud y sigue | igual |
| El socket no traga instantáneas | las **tira** y cuenta cuántas en `perdidas` | enseña que va por detrás |
| El socket no traga avisos | cola de 1000; si se llena, `T_FIN` con `M_ERROR`, cierra **y sigue simulando** | — |
| `host` no es de bucle local | **avisa por la salida de error** y se conecta igual | — |

Y una regla general que vale para los dos lados: **el modelo nunca se bloquea
esperando a la pantalla.** El socket es no bloqueante en marcha, y la única
lectura bloqueante de todo el protocolo es la del saludo, que ocurre antes de
que exista simulación alguna que se pueda quedar parada.
