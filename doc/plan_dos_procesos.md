# Plan por fases: `mcu-sim` + `mcu-sim-gui`, dos procesos

Cómo se construye la contraparte gráfica de `mcu-sim` siguiendo la **opción de
dos procesos** —el escenario 3 de `mcu-sim/doc/analisis_gui.md`— y cómo se
conectan los dos programas.

---

## 0. Cómo leer esto, y qué crédito darle

Las fuentes de este plan son tres, y conviene saber cuál es cuál:

| | |
| :--- | :--- |
| `[AG]` | `mcu-sim/doc/analisis_gui.md`, el análisis previo: 1 188 líneas, 21 secciones, con medidas reproducibles |
| `[CÓDIGO]` | El árbol de `mcu-sim` leído para escribir esto: `top/sim_main.cpp` (615 líneas, el ejecutable del producto), `parts/part_base.h`, `common/red.h`, `common/gdb_rsp.h` |
| **⚠ SIN VERIFICAR** | Lo que no se ha podido probar todavía; se dice qué lo cerraría |

**Estado del árbol al escribir esto**, que es la línea de la que se parte y
contra la que se mide cualquier regresión:

| | |
| :--- | ---: |
| Suite del F407 | **2074**, en `2336217899213 ps` |
| Suite del F446 | **203**, en `1033367277932 ps` |
| Banco del F417 | **164**, en `718988288 ps` |
| Capa de red, sin SystemC | `make red` 13/13 |
| Placas que validan | 6, con 0 avisos |
| Referencias en el catálogo | 29 |

> **El invariante del F407 es el criterio de aceptación de las fases 1 a 6.**
> Vale `2336217899213 ps` al picosegundo y lleva once fases sin moverse. Si un
> cambio de este plan lo mueve, ha cambiado el comportamiento del modelo aunque
> las 2074 sigan pasando, y hay que entender por qué **antes** de seguir.

---

## 1. La decisión, y en qué se aparta del análisis

`[AG]` compara tres escenarios y recomienda el **2** —un solo ejecutable Qt con
la simulación en su propio hilo—. Este plan hace el **3**, dos procesos. La
diferencia no es un descuido, así que conviene dejar escrito el razonamiento
completo, incluido el argumento que se está descartando.

**Lo que `[AG]` pone a favor de los dos procesos** (§6.3, §18.2) y sigue siendo
cierto:

* **aislamiento de verdad**: un firmware raro de un alumno que tumbe el modelo
  no se lleva la ventana, y al revés;
* **`mcu-sim` no aprende Qt**: ni una cabecera, ni una dependencia, ni un `moc`.
  El ejecutable que tiene que compilar en cualquier sitio sigue siendo C++17 y
  `<systemc>`, y la única parte que sabe en qué sistema operativo corre sigue
  siendo un solo fichero, `common/red.h`;
* **el IDE del alumno ya es un tercer proceso** hablando por TCP, así que una
  arquitectura de procesos separados no es una rareza en este producto: es lo
  normal;
* **el protocolo se convierte en un activo**, igual que pasó con el netlist en
  XML: un guion, una prueba automática o un panel web pueden ser clientes sin
  tocar el simulador;
* se puede correr `mcu-sim` bajo ASan con la ventana encima, sin que los
  sanitizers vean el código de Qt.

**Lo que `[AG]` §18.2 pone en contra, y es un argumento serio:** *la
distribución.* «Un alumno tiene que instalar una cosa y pulsar un icono. Dos
ejecutables que se buscan por un puerto es una fuente de incidencias de soporte
—cortafuegos, puertos ocupados, uno que arranca y el otro no— que consume el
tiempo del profesor, que es el recurso escaso.»

**Y ese es exactamente el requisito que este plan convierte en diseño.** El
alumno no lanza dos cosas: lanza `mcu-sim-gui`, y es la GUI la que **escucha
primero y después lanza `mcu-sim`** como proceso hijo, con todos sus argumentos.
De ahí salen tres consecuencias que desactivan la objeción una por una:

1. **no hay carrera de arranque**: cuando el modelo intenta conectarse, el
   escuchador lleva puesto desde antes de existir el proceso;
2. **no hay puerto ocupado**: la GUI pide el puerto al sistema y, si el de la
   configuración está cogido, usa otro y se lo pasa al hijo en `--gui`. El
   alumno no se entera;
3. **no hay cortafuegos**: por omisión es `localhost`, que ningún cortafuegos
   doméstico filtra. El `host` del argumento está para la GUI en otra máquina,
   que es un caso de profesor, no de alumno.

Queda en pie, y hay que decirlo, lo que **no** se desactiva: **son dos ciclos de
compilación y dos sitios donde mirar** cuando algo no cuadra. Es el precio, se
paga a cambio de lo de arriba, y la fase 9 se dedica entera a que ese precio no
lo pague el alumno.

**Y la parte tranquilizadora**, que también es de `[AG]`: las tres columnas
comparten la parte cara —los observables, los mandos, la frontera de §5— y se
diferencian en la barata. Nada de las fases 1, 5 y 8 de este plan cambiaría si
mañana se decidiera volver al escenario 2.

---

## 2. Quién sabe qué

```
   +----------------------+                    +------------------------+
   |     mcu-sim-gui      |   TCP, marco       |        mcu-sim         |
   |  (Qt 6, escucha)     |<==== binario =====>|  (C++17 + SystemC,     |
   |                      |   localhost:3344   |   conecta, --gui)      |
   |  - lanza el hijo     |                    |                        |
   |  - dibuja la placa   |--- QProcess ------>|  - la placa, el MCU,   |
   |  - manda ordenes     |                    |    el firmware         |
   |  - ensena avisos     |                    |  - los dos servidores  |
   +----------------------+                    |    de GDB (sin tocar)  |
              ^                                +------------------------+
              |                                            ^
      el alumno pulsa UN icono                             |
                                                  el IDE del alumno,
                                                  por TCP, como hoy
```

**La GUI no sabe nada del modelo salvo lo que el modelo le cuenta.** No conoce
ni un tipo de C++ de `mcu-sim`: construye su pantalla a partir del XML de la
placa y del catálogo de observables y mandos. **Añadir una pieza nueva al
simulador no recompila la GUI**, que es la diferencia entre una herramienta y
una demo `[AG]` §2.2.

**El modelo no sabe nada de la GUI salvo que hay un socket.** Sin `--gui` no
abre nada, no construye nada y se comporta **exactamente** como hoy.

Lo que **no** cruza la frontera está en `[AG]` §2.3 y repetido en la cabecera de
`protocolo.h`: punteros, tensiones de pin sueltas y registros del MCU. Lo
tercero merece insistencia: **para ver registros están los dos servidores de
GDB**, que ya existen, ya están verificados y hablan un protocolo que los IDE
entienden. Duplicar eso en la ventana sería reinventar mal lo que ya está hecho.

---

## 3. El argumento nuevo de `mcu-sim`: `--gui host:puerto`

```
mcu-sim placa.xml [firmware.bin] [ms] --gui[=host:puerto]
```

| Forma | Quiere decir |
| :--- | :--- |
| *(ausente)* | **Todo sigue exactamente igual que hoy.** Ni un socket, ni un proceso nuevo, ni un picosegundo distinto |
| `--gui` | `localhost:3344` |
| `--gui 7000` | `localhost:7000` |
| `--gui otra-maquina:3344` | ese host, y **un aviso por la salida de error** por no ser de bucle local |
| `--gui [::1]:3344` | IPv6, con la dirección entre corchetes |
| `--gui=...` | lo mismo, en la forma con igual que el resto de opciones del programa ya admite |

**Con `--gui`, la simulación no empieza hasta que la GUI lo diga.** Y no en el
sentido flojo de «empieza y se queda quieta»: en el sentido de que
**`sc_start()` no se ha llamado**. El saludo entero —quién soy, la placa, el
catálogo, la suscripción, incluso una secuencia de órdenes programada por
adelantado— ocurre en `sc_main`, con lecturas bloqueantes normales, antes de que
exista un solo proceso de SystemC corriendo. Los detalles están en
`doc/protocolo.md` §3.

Esto tiene una consecuencia que conviene ver: **el diálogo previo no cuesta
tiempo simulado**, porque no hay tiempo simulado todavía. Sea lo que sea que la
GUI y el modelo se cuenten antes de arrancar, el invariante no se entera.

**Los otros argumentos no cambian.** `--gui` se combina con `--gdb`,
`--gdb-dap`, `--tiempo-real`, `--ondas`, `--mcu`, `--ms` y `--valida` sin
sorpresas, con una excepción que hay que decidir en la fase 3: `--valida` no
simula, así que con `--gui` manda el catálogo, un `T_FIN` y termina. Es útil
—es como la GUI puede enseñar una placa sin ejecutarla— y hay que escribirlo, no
dejarlo pasar.

---

## 4. Las fases

Nueve fases. Cada una dice **qué deja hecho**, **cómo se comprueba** y **qué NO
entra**, que es la parte que impide que una fase se coma a la siguiente.

El orden no es el de `[AG]` §20 porque aquel ordenaba el producto entero y este
ordena una pieza de él; lo que sí se respeta es su regla: **primero lo que puede
invalidar el trabajo de abajo.**

---

### Fase 0 — Los dos esqueletos, y que la cadena de herramientas exista

**Por qué va primero.** Porque si Qt 6 no compila en las tres plataformas de
destino, o si el `--gui` mínimo mueve el invariante, todo lo que hay debajo
cambia de forma. Es una tarde y decide el resto.

**Qué se escribe.**

* `mcu-sim-gui`: repositorio, `CMakeLists.txt` con Qt 6 (`Widgets` y `Network`),
  `main.cpp`, una ventana vacía, `.gitignore`, `README.md` y este plan.
* `mcu-sim`: `--gui host:puerto` **reconocido y parseado**, y nada más —ni
  socket—: imprime a dónde se conectaría y sigue como hoy.
* `src/protocolo.h` en los dos árboles, idéntico.

**Cómo se comprueba.**

1. `make test407 && make test446 && make test417` → **2074 / 203 / 164, 0
   fallos**, y `2336217899213 ps` al picosegundo. Sin `--gui`, nada ha cambiado.
2. `mcu-sim placas/discovery_min.xml --gui` imprime `localhost:3344` y simula
   igual que sin el argumento.
3. Una comprobación nueva en la suite que valide el parseo de las cinco formas
   de `--gui` **sin abrir un socket ni gastar tiempo simulado** — es una función
   pura sobre una cadena, así que se prueba como tal.
4. La GUI compila y abre una ventana en Linux. **⚠ SIN VERIFICAR** en Windows y
   macOS; lo cierra construirla allí, y es el riesgo R-1 de §5.

**Qué NO entra.** Ningún socket. Ninguna pieza con observables. Ninguna ventana
con contenido.

---

### Fase 1 — La frontera dentro del modelo, todavía sin socket

Esta es la fase cara y la que se reaprovecha entera, dijera lo que dijera el
escenario elegido `[AG]` §5. **Se puede escribir y verificar sin GUI ninguna.**

**Qué se escribe.**

```cpp
// En parts/part_base.h, junto a los terminales y el inventario que ya están.
struct Observable {
    const char* nombre;      // "encendido", "corriente", "angulo"
    const char* unidad;      // "", "mA", "grados"
    float       min, max;    // para la escala de la GUI; iguales = sin escala
    bool        interesante; // lo que la pieza SUGIERE pintar
};
struct Mando {
    const char* nombre;
    enum Tipo { Boton, Interruptor, Continuo } tipo;
    float       min, max;
};

class ExtPartBase {
    // ...lo de hoy, intacto...
    virtual unsigned   n_observables() const { return 0; }
    virtual Observable observable(unsigned) const { return {"", "", 0, 0, false}; }
    virtual float      valor_observable(unsigned) const { return 0.f; }
    virtual unsigned   n_mandos() const { return 0; }
    virtual Mando      mando(unsigned) const { return {"", Mando::Boton, 0, 0}; }
    virtual void       acciona(unsigned, float) {}
};
```

Con valores por omisión **las 21 piezas que hay hoy compilan sin tocarlas**, que
es lo que permite que esta fase no sea un terremoto. Se implementan las tres que
el enunciado necesita de verdad: `Led` (`encendido`, `corriente`), `Button`
(`pulsado`, y el mando `pulsar`) y `Crystal` (`frecuencia`).

Y las dos mitades de la frontera:

* el **muestreador**: un `SC_THREAD` que despierta cada `periodo_ns` de tiempo
  simulado, recorre los observables suscritos y publica una `Instantanea` plana;
* el **aplicador**: un `SC_THREAD` que vacía una cola de `Orden` y llama a
  `acciona()`.

**La trampa del invariante, y cómo se sortea.** La elaboración de SystemC es
estática: estos dos procesos se construyen siempre, haya `--gui` o no. Un
proceso que se construye y despierta **sí** mueve el tiempo simulado. La salida
ya está probada en este proyecto: **un módulo cuyos procesos nunca despiertan no
mueve el invariante** — se demostró en la fase 4 del plan del F415/F417, donde
se añadieron el CRYP y el HASH enteros a la elaboración del F407 sin que
`2336217899213 ps` se moviera un picosegundo. Sin `--gui`, los dos procesos
esperan sobre un `sc_event` que nadie notifica nunca. Eso es todo.

**Cómo se comprueba.** Un grupo nuevo en la suite del F407 que, **sin gastar
tiempo simulado**, pregunte al modelo: que un `Led` declara dos observables con
sus unidades, que un `Button` declara un mando, que `acciona()` sobre un botón
cambia su `pulsado`, que una orden con pieza inexistente devuelve `R_PIEZA`. La
doctrina del proyecto aquí es conocida: **se pregunta al modelo, no se pasa por
el bus**, porque una sola lectura de bus cuesta 62 500 ps y mueve el invariante.

Y la comprobación que de verdad importa: **2074 / 203 / 164 y
`2336217899213 ps`.**

**Qué NO entra.** El socket. Las piezas nuevas (`PwmMeter`, `Servo`, `Encoder`,
`StepperDriver`, `DcMotor`): son `[AG]` §8, son la parte grande y van después de
todo esto.

---

### Fase 2 — La capa de transporte, probada sin SystemC

**Qué se escribe.**

* En `common/red.h`, lo que hoy no hay: **`conecta(host, puerto)` y
  `escucha(host, puerto)`**. Hoy solo existen `conecta_local` y `escucha_local`,
  de bucle local a propósito «porque esto es un depurador con acceso total a la
  memoria del objetivo». La versión con host se añade **al lado**, no en lugar
  de, y las de GDB siguen usando la local: el cambio de política tiene que ser
  visible en el código, no heredado.
* El codificador y el decodificador del marco, en `common/proto_io.h`: leer una
  cabecera, validar magia, versión y longitud, y entregar el cuerpo. Sin
  dependencias y sin asignar memoria por mensaje pequeño.
* El mismo par en la GUI, sobre `QTcpServer`/`QTcpSocket`.

**Cómo se comprueba.** Con el precedente que ya existe: `make red` son trece
comprobaciones de la capa de sockets **sin SystemC de por medio**, y sirven para
validar una plataforma nueva antes de pelearse con la biblioteca. Se añade
`make gui-proto` en la misma línea: dos hilos, un socket, todos los tipos de
mensaje de ida y vuelta, y los casos feos —magia mala, versión imposible,
longitud de 4 GB, tipo desconocido que hay que **saltar** y seguir, mensaje
partido en dos `recv`, dos mensajes en un `recv`—. Nada de esto necesita
SystemC ni Qt, así que cruza a Windows y a macOS con el resto de `make red`.

**Qué NO entra.** Mensajes con significado. Esta fase mueve bytes.

---

### Fase 3 — El saludo y el arranque diferido

Aquí es donde `--gui` empieza a hacer algo.

**Qué se escribe.** En `sc_main`, después de construir la placa y **antes de
`sc_start()`**: conectar, `T_HOLA`, esperar `T_VERSION`, mandar `T_PLACA` con el
volcado de `--netlist` —que ya existe y no hay que inventar— y `T_CATALOGO` con
los observables y mandos de la fase 1, `T_LISTO`, y quedarse leyendo hasta
`T_ARRANCA`.

En la GUI: escuchar, aceptar, y **construir la pantalla a partir de los dos
XML**. Un panel por pieza, un indicador por observable interesante, un control
por mando. Sin conocer un tipo.

**Cómo se comprueba.** Un cliente de prueba **sin Qt** —el equivalente de
`verif/gdb_client.h`, que es como la suite prueba hoy los servidores de GDB
contra sí misma— que hace el saludo completo, comprueba que el catálogo declara
las piezas de `placas/discovery_min.xml` y **no manda `T_ARRANCA`**: el modelo
tiene que quedarse esperando para siempre sin consumir CPU y sin avanzar un
picosegundo de tiempo simulado. Ese «para siempre sin avanzar» es la prueba de
que el arranque diferido es real.

Y el caso que hay que decidir aquí y no después: `--valida --gui`.

**Qué NO entra.** Instantáneas, órdenes, control. Al llegar `T_ARRANCA` el
modelo simula su ventana como hoy y termina.

---

### Fase 4 — Instantáneas, avisos y estado en marcha

**Qué se escribe.** El `SC_THREAD` que atiende el socket en marcha, con el mismo
patrón que `common/gdb_rsp.h`: no bloqueante, cada 100 µs de tiempo simulado.
El muestreador de la fase 1 conectado a `T_INSTANTANEA`. El desvío de
`sc_report_handler` a `T_AVISO`, que es lo que pone delante del alumno los
avisos de placa que hoy solo ve quien ejecuta `--valida`. `T_ESTADO` con los dos
relojes.

**La política de contrapresión, escrita antes y no después:** las instantáneas
**se tiran** cuando el socket no traga y se cuenta cuántas; los avisos **no se
tiran nunca** y si su cola se llena se cierra la conexión con `M_ERROR`. El
motivo está en el protocolo: un aviso perdido se parece mucho a un modelo que
funciona, y eso es lo peor que puede pasar en una herramienta de enseñanza.

**Cómo se comprueba.** El cliente sin Qt arranca la simulación de
`placas/discovery_min.xml` con el `blinky`, se suscribe a `encendido` del LED y
comprueba que **el LED parpadea en las instantáneas** con el periodo que el
firmware programa. Es la primera prueba de extremo a extremo del protocolo, y no
necesita ventana.

**Qué NO entra.** Órdenes y control: esto es el sentido modelo → pantalla
entero, y nada más.

---

### Fase 5 — Las órdenes

**Qué se escribe.** `T_ORDENES` con la semántica de deltas de
`doc/protocolo.md` §5, la validación de pieza, mando y rango, y `T_ORDEN_HECHA`
con el instante real. El aplicador de la fase 1 conectado al otro extremo.

**Cómo se comprueba.** Con **el ejemplo del enunciado, ejecutado sin GUI**: la
secuencia de cuatro órdenes sobre un pulsador enviada antes de `T_ARRANCA`, y
comprobar que los cuatro `T_ORDEN_HECHA` traen exactamente 1,00 s, 1,50 s,
4,00 s y 4,22 s de tiempo simulado. Si esa prueba pasa, la parte con miga del
protocolo está bien.

Y las tres que hacen falta al lado: pieza inexistente → `R_PIEZA`; valor fuera
de rango → recorte, `R_RANGO` y un `T_AVISO`; delta 0 → dos órdenes en el mismo
instante, en el orden del mensaje.

**Qué NO entra.** La grabación de sesiones. Es la fase 8 y depende de esta.

---

### Fase 6 — El control de la simulación

**Qué se escribe.** `T_PAUSA`, `T_SIGUE`, `T_PASO` y `T_PARA`, y las tres
políticas de ritmo de `[AG]` §5.3: tiempo real (la de por omisión), libre y a
demanda.

**La trampa que hay que resolver aquí**, y está dicha en `[AG]` §4.1 y §18.1:
el socket lo atiende un proceso de SystemC, así que **solo se atiende si el
tiempo simulado avanza**. «Pausado» no puede ser dejar de llamar a `sc_start()`,
porque entonces el modelo se queda sordo y no oye el `T_SIGUE`. La forma
correcta es la que `sim_main.cpp` ya usa cuando hay un stub de GDB esperando
—trocear rodajas de tiempo simulado corto— con el resto del modelo quieto.

Y el aviso que va con ella: **`sc_stop()` es definitivo**. Por eso `T_PARA` y
`T_PAUSA` son mensajes distintos y no uno con un booleano.

**Cómo se comprueba.** El cliente sin Qt: arrancar, pausar, comprobar que el
tiempo simulado **deja de avanzar** pero el modelo **sigue contestando a
`T_PING`** —que es justo la propiedad que se acaba de describir—, seguir,
comprobar que avanza, y parar.

---

### Fase 7 — La ventana de verdad, y el lanzamiento automático

**Qué se escribe.**

* El **lanzador**: la GUI escucha primero, pide el puerto, lanza `mcu-sim` con
  `QProcess` pasándole `--gui localhost:<puerto>` más el resto de argumentos, y
  recoge su salida estándar y de error en un panel. Si el puerto de la
  configuración está cogido, coge otro **sin decir nada**: esa es la mitad de la
  objeción de `[AG]` §18.2, desactivada.
* La **configuración**, en JSON (`QJsonDocument`, que Qt trae; ninguna
  dependencia nueva): ruta al ejecutable, placa, firmware, ms, mcu, modo de GDB
  y puerto, `--ondas`, `--tiempo-real`, y el host y puerto propios.
* El **diálogo de lanzamiento**, donde **todos** los argumentos de `mcu-sim` se
  pueden poner a mano.

**Y aquí una decisión que evita una deuda desde el primer día.** Que el diálogo
conozca los argumentos de `mcu-sim` quiere decir que hay **dos sitios** que
saben la lista, y el segundo envejece. La salida es la que este proyecto ya ha
usado dos veces —el netlist y `--help`, programas que se describen a sí
mismos—: `mcu-sim --argumentos` vuelca su lista de opciones en XML (nombre,
forma, tipo, valor por omisión, una línea de ayuda) y **la GUI construye el
diálogo a partir de ella**. Con eso, añadir una opción al simulador la hace
aparecer sola en la ventana, y no hay segundo sitio que envejezca.

**Cómo se comprueba.** Aquí sí hace falta una persona: abrir la GUI, elegir
`placas/discovery_min.xml` y el `blinky`, pulsar arrancar y **ver el LED
parpadear a la velocidad correcta** con el reloj simulado y el de pared en
pantalla. Más la lista de fallos que se prueban a mano: matar `mcu-sim` desde
fuera, cerrar la GUI con el modelo corriendo, un puerto ocupado a propósito, una
ruta de ejecutable mala.

**Qué NO entra.** Nada bonito. Esta fase es que funcione.

---

### Fase 8 — Grabación y reproducción de sesiones

`[AG]` §9 dice que esto **no es opcional**, y lleva razón: una GUI interactiva
sin grabación convierte un simulador determinista en uno que a veces falla, y
ese cambio es difícil de deshacer una vez la gente se acostumbra.

**Qué se escribe.** La GUI guarda los `T_ORDEN_HECHA` en un XML de sesión; y
`mcu-sim` gana `--sesion fichero.xml`, que aplica esas órdenes en sus instantes
simulados exactos **sin GUI ninguna**.

```xml
<sesion placa="placas/discovery_min.xml" firmware="verif/fw/blinky/blinky.bin">
  <orden t_ns="1250000000" pieza="btn_marcha" mando="pulsar" valor="1"/>
  <orden t_ns="1310000000" pieza="btn_marcha" mando="pulsar" valor="0"/>
</sesion>
```

**Cómo se comprueba.** Ejecutar una sesión dos veces y comprobar que el tiempo
simulado final es **idéntico al picosegundo**. Con eso, un fallo encontrado a
mano se convierte en un caso reproducible, y de ahí en una prueba de la suite,
que es la moneda de este proyecto.

---

### Fase 9 — Que el precio de los dos procesos no lo pague el alumno

La fase que salda la objeción de `[AG]` §18.2 del todo.

**Qué se escribe.** El repaso completo de lo que puede salir mal: cada fila de
la tabla de `doc/protocolo.md` §6 con una comprobación detrás. El empaquetado
—un instalador por plataforma que lleve los dos ejecutables— y el mensaje de
error para cuando no se encuentra `mcu-sim`, que tiene que decir dónde ha
mirado. Y el modo inverso, `--gui-escucha puerto`, para engancharse a una
simulación ya en marcha: es de profesor, no de alumno, pero es barato aquí y
caro después.

**Cómo se comprueba.** Construir el instalador en las tres plataformas y
ejecutarlo en una máquina limpia. **⚠ SIN VERIFICAR** hasta que exista: hoy el
proyecto está verificado en Linux con g++ y clang, cruza a Windows con MinGW-w64
sin avisos, y **falta construir SystemC allí y ejecutarlo**, además de lo mismo
en un Mac.

---

## 5. Los riesgos, y qué los cierra

| | Riesgo | Cómo se cierra | Cuándo se sabe |
| :--- | :--- | :--- | :--- |
| **R-1** | SystemC no se construye para MinGW, o el modelo no corre en Windows | Construirlo y ejecutar la suite allí. Es el riesgo heredado más grande del proyecto (`[AG]` §19.3) y no lo crea este plan | fase 0 |
| **R-2** | Los procesos nuevos mueven el invariante del F407 | Que no despierten sin `--gui`. Probado en la fase 4 del plan del F415/F417 | fase 1 |
| **R-3** | El socket solo se atiende si el tiempo simulado avanza, y en pausa no avanza | Rodajas cortas en pausa, como ya hace `sim_main.cpp` con un stub de GDB esperando | fase 6 |
| **R-4** | La interactividad rompe el determinismo | No se puede evitar; se compensa grabando la sesión con sus instantes reales | fase 8 |
| **R-5** | La lista de argumentos envejece en la GUI | `mcu-sim --argumentos`: el programa se describe a sí mismo | fase 7 |
| **R-6** | Las dos copias de `protocolo.h` divergen | Una comprobación en la suite que las compara byte a byte | fase 2 |
| **R-7** | La tentación de meter lógica en el cliente, «que es como estos diseños se pudren» (`[AG]` §6.3) | La regla: la GUI no calcula nada que el modelo pueda decir. Se revisa en cada fase | siempre |

---

## 6. Lo que este plan NO hace, dicho a propósito

* **Las piezas que no existen.** `PwmMeter`, `Servo`, `Encoder`,
  `StepperDriver` y `DcMotor` son `[AG]` §8 y son la parte grande de verdad —el
  motor de continua es un modelo continuo dentro de un simulador de eventos
  discretos y necesita una decisión propia sobre cuánta física se quiere—. Se
  escriben y se verifican **sin GUI**, con el banco de siempre, exactamente como
  se escribieron las 21 que hay. Este plan solo se asegura de que cuando
  existan, aparezcan solas en la pantalla.
* **Registros del MCU en la ventana.** Están los dos servidores de GDB.
* **Sustituir a `--valida`.** La GUI enseña sus avisos; la orden sigue estando.
* **Una GUI bonita.** La fase 7 es que funcione.

---

## 7. Dependencias entre fases

```
  0 ──┬── 1 ──┬── 3 ── 4 ── 5 ── 6 ── 7 ── 8
      └── 2 ──┘                          └── 9
```

La 1 y la 2 son independientes entre sí y se pueden hacer en cualquier orden o a
la vez: una es el modelo por dentro, la otra son bytes por un socket. De la 3 en
adelante es una cadena, porque cada una necesita que la anterior hable.

**La 1 es la única que se queda si se cambia de idea**: si mañana se volviera al
escenario 2 de `[AG]` —un solo ejecutable con la simulación en un hilo—, las
fases 2, 3 y 9 sobran y todo lo demás vale igual. Por eso está donde está.
