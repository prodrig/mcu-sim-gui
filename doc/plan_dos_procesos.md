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

**Estado del árbol**, que es la línea de la que se parte y contra la que se
mide cualquier regresión. La primera columna es la de cuando se escribió este
plan y se queda como estaba; la que vale es la segunda:

| | Al escribir el plan | **Hoy** (2026-10-01, `mcu-sim` `0e449d0`) |
| :--- | ---: | ---: |
| Suite del F407 | 2074, en `2336217899213 ps` | **2118**, `resto` **`2240553274213 ps`** (total `2337219149213 ps`) |
| Suite del F446 | 203, en `1033367277932 ps` | **204**, en `1033367277932 ps` |
| Banco del F417 | 164, en `718988288 ps` | **165**, en `718988288 ps` |
| Banco del puente UART (`testserie`) | — | **189**, en `400677589564 ps` |
| Capa de red, sin SystemC | `make red` 13/13 | `make red` 13/13 |
| Placas en `placas/` | 6, que validan con 0 avisos | 10: las 6 de antes y las 4 del puente UART |
| Referencias en el catálogo | 29 | 29 |

Lo que ha cambiado por el camino, para que nadie lo tome por una regresión:

* **+43 en el F407**: las comprobaciones de `--gui` de la fase 0 (§8.4).
* **+1 en cada suite**: `T00`/`A0`, que contrastan las huellas de los
  firmwares antes de simular nada (**T-22** de `mcu-sim/doc/todo.md`).
* **El F407 se mide por `resto` y no por el total.** El total lleva dentro lo
  que tarda un GDB de verdad en contestar por un socket de verdad (T96 y T97),
  y eso depende de la máquina (**T-16**). `resto` es el total menos esos dos
  grupos.
* **El invariante del F407 se movió una vez, a propósito**: al corregir que
  el IWDG reseteaba un tick antes de su plazo (**T-23**, 2026-09-23), `resto`
  pasó de `2239552024213` a `2240553274213 ps`, 1,00125 ms más. Las otras
  suites no se movieron.
* **`testserie` es un banco nuevo**, el del puente UART (**P-14**). Monta la
  pieza `PuenteSerie`, que heredará los métodos nuevos de `ExtPartBase` de la
  fase 1 igual que las demás, así que **entra en el criterio**.

> **El criterio de aceptación de las fases 1 a 6 es `mcu-sim/src/verif/invariantes.txt`.**
> Es la única fuente de esas cifras: una línea por suite, con el número de
> comprobaciones y el tiempo simulado al picosegundo, y
> `ci/comprueba_invariante.sh` la contrasta en las cuatro plataformas del CI.
> Si un cambio de este plan mueve un tiempo simulado, ha cambiado el
> comportamiento del modelo aunque todas las comprobaciones sigan pasando, y
> hay que entender por qué **antes** de seguir. **El número de comprobaciones
> sí puede crecer**: las fases añaden grupos. Cuando crezca, se actualiza la
> cifra de su línea en `invariantes.txt` en el mismo commit, y la columna de
> picosegundos no se toca.

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

> **EJECUTADA.** Lo hecho y lo medido está en §8, al final de este documento.

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

Con valores por omisión **las 22 piezas que hay hoy compilan sin tocarlas**, que
es lo que permite que esta fase no sea un terremoto. Eran 21 al escribir el
plan; la 22.ª es `PuenteSerie`, la del puente UART (P-14). El catálogo al día
está en `mcu-sim/doc/parts.md`, y `mcu-sim --help` lista las piezas que conoce
la factoría. Se implementan las tres que
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
cambia su `pulsado`, que una orden con pieza inexistente devuelve `RES_PIEZA`. La
doctrina del proyecto aquí es conocida: **se pregunta al modelo, no se pasa por
el bus**, porque una sola lectura de bus cuesta 62 500 ps y mueve el invariante.

Y la comprobación que de verdad importa: **`ci/pasa_suites.sh` y
`ci/comprueba_invariante.sh` en verde para las cuatro suites de
`verif/invariantes.txt`**, con los picosegundos de hoy intactos:

| Suite | Comprobaciones | Tiempo simulado |
| :--- | ---: | ---: |
| `test407` | **2118 + las del grupo nuevo** | `resto` **`2240553274213 ps`** |
| `test446` | 204 | `1033367277932 ps` |
| `test417` | 165 | `718988288 ps` |
| `testserie` | 189 | `400677589564 ps` |

Solo cambia una cifra, el recuento del F407, y se actualiza en
`invariantes.txt` en el mismo commit que añade el grupo. Si cualquier otra se
mueve, la fase no está cerrada. `testserie` está en la lista porque
`PuenteSerie` también deriva de `ExtPartBase`.

**Qué NO entra.** El socket. Las piezas nuevas (`PwmMeter`, `Servo`, `Encoder`,
`StepperDriver`, `DcMotor`): son `[AG]` §8, son la parte grande y van después de
todo esto.

> **Ejecutada el 2026-10-01** (§9), con dos desviaciones de lo de arriba, las
> dos a propósito: `Crystal` declara `presente` y no `frecuencia`, porque la
> pieza no tiene frecuencia (§9.2); y **ninguna comprobación va en `test407`**:
> todas están en un banco aparte, `testgui`, y la de `test407` es que su línea
> de `invariantes.txt` no cambie ni en el recuento (§9.3).

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

> **Ejecutada el 2026-10-01** (§10). Una cosa más de lo que pedía: el lector y
> el emisor del marco no están escritos dos veces, una con sockets de Berkeley
> y otra con Qt. Son **el mismo fichero**, `proto_io.h`, copiado en los dos
> repositorios como `protocolo.h`; cada extremo pone encima su manera de mover
> bytes (§10.1).

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

> **Ejecutada el 2026-10-01** (§11). `--valida --gui` queda decidido: placa,
> catálogo y `T_FIN`, sin `T_LISTO` y sin esperar (§11.2). Y destapó un fallo
> de la fase 1 que nadie podía ver hasta juntar los dos XML: las piezas que no
> son `sc_module` tenían en el catálogo un nombre distinto del de la placa
> (§11.3).

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

> **Ejecutada el 2026-10-01** (§12). Tres cosas que el plan no decía: el
> enlace no habla con un socket sino con un **canal**, para que `testgui`
> pruebe la contrapresión sin sockets (§12.1); los avisos de **placa** salen
> en el saludo, antes de arrancar (§12.2); y destapó un fallo de
> `--tiempo-real` con ventana finita, que ya estaba en `mcu-sim` (§12.3).

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

Y las tres que hacen falta al lado: pieza inexistente → `RES_PIEZA`; valor fuera
de rango → recorte, `RES_RANGO` y un `T_AVISO`; delta 0 → dos órdenes en el mismo
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
mirado.

**Y una decisión que la §8.7 acaba de convertir en obligatoria: la FIRMA de los
ejecutables.** Un binario nuevo, sin firmar y desconocido es exactamente lo que
un antivirus heurístico pone en cuarentena — le pasó a este proyecto en su
propia máquina de desarrollo, con ESET, y le va a pasar a cada alumno que
descargue el simulador. Las salidas son tres y hay que elegir una a propósito:
firmar con un certificado de firma de código (cuesta dinero y hay que
renovarlo), distribuir por un canal que dé reputación al binario, o **documentar
la exclusión** y aceptar las incidencias de soporte que eso genera. La tercera
es gratis y es la peor, porque consume el tiempo del profesor, que es el recurso
escaso: exactamente el argumento de `[AG]` §18.2 reapareciendo por otra puerta. Y el modo inverso, `--gui-escucha puerto`, para engancharse a una
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
| **R-1** | SystemC no se construye para MinGW, o el modelo no corre en Windows | **Medio cerrado, y por el lado bueno: SystemC 2.3.4 SÍ se construye para MinGW y `mcu-sim.exe` arranca y responde.** Y desde el 2026-10-01 **el CI de los dos repositorios lo cierra en la práctica**: las suites de `mcu-sim` pasan en MSYS2 con los invariantes al picosegundo, y `mcu-sim-gui` compila y pasa `ctest` allí (§10.3). Lo que queda abierto es lo que el CI no ve: una ventana en una pantalla de Windows de verdad, y repartir el ejecutable (fase 9). El relato, con los dos diagnósticos equivocados, en §8.7; la trampa de `libwinpthread`, en §8.8 | fase 0; **cerrado por el CI** salvo lo que se ve a mano |
| **R-2** | Los procesos nuevos mueven el invariante del F407 | Que no despierten sin `--gui`. Probado en la fase 4 del plan del F415/F417 | **cerrado en la fase 1**: con la frontera construida y sin activar, `test407` da 2118 y `resto` en `2240553274213 ps`, y `sim` los mismos deltas (§9.4) |
| **R-3** | El socket solo se atiende si el tiempo simulado avanza, y en pausa no avanza | Rodajas cortas en pausa, como ya hace `sim_main.cpp` con un stub de GDB esperando | fase 6 |
| **R-4** | La interactividad rompe el determinismo | No se puede evitar; se compensa grabando la sesión con sus instantes reales | fase 8 |
| **R-5** | La lista de argumentos envejece en la GUI | `mcu-sim --argumentos`: el programa se describe a sí mismo | fase 7 |
| **R-6** | Las dos copias de `protocolo.h` divergen | Una comprobación en la suite que las compara byte a byte | **cerrado en la fase 2**: `make gui-proto` compara `protocolo.h` y `proto_io.h` con las de este repositorio, y el CI de `mcu-sim` lo clona para eso (§10.3) |
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

---

## 8. Fase 0, ejecutada

### 8.1 Qué se ha escrito

**En `mcu-sim-gui`** (commit inicial): el repositorio, `CMakeLists.txt` con
Qt 6.3+ (`Widgets` y `Network`), `main.cpp`, una `VentanaPrincipal` vacía,
`src/protocolo.h`, `config.ejemplo.json`, el `.gitignore`, el README y estos dos
documentos.

**En `mcu-sim`**, y son cuatro ficheros:

| Fichero | Qué es |
| :--- | :--- |
| `src/common/protocolo.h` | La copia vendida, **idéntica byte a byte** a la de `mcu-sim-gui/src/` |
| `src/common/gui_destino.h` | El parseo de `--gui host:puerto`. Una función pura: no abre nada, no resuelve nombres, no avanza el reloj |
| `src/top/sim_main.cpp` | Reconoce `--gui`, `--gui X` y `--gui=X`, dice a dónde apuntaría, avisa si el host no es la propia máquina, y lo pone en `--help` |
| `src/top/sc_main.cpp` | **T130**, el grupo de comprobaciones del argumento |

**Por qué el parseo es un fichero y no cuatro líneas dentro de `sc_main`.**
Porque así se puede probar. Las formas que se equivocan no son `--gui 7000`
—esa se ve a ojo la primera vez— sino el puerto 0, el 65536, el IPv6 sin
corchetes y el corchete sin cerrar, que nadie escribe a mano en una prueba
manual. Y un parseo que falla ahí **no da error**: se conecta a otro sitio, y
eso se depura mal.

### 8.2 Las seis formas, y las siete que se rechazan

```
--gui                 ->  localhost:3344
--gui 7000            ->  localhost:7000
--gui maquina         ->  maquina:3344
--gui maquina:9000    ->  maquina:9000
--gui=[::1]:5000      ->  [::1]:5000
--gui=[::1]           ->  [::1]:3344
```

y los mensajes de las malas, que se han mirado uno a uno porque un mensaje de
error es lo único que alguien lee cuando se equivoca:

```
--gui=host:99999  ->  --gui: '99999' no es un puerto (1..65535)
--gui=host:0      ->  --gui: '0' no es un puerto (1..65535)
--gui=::1:5000    ->  --gui: '::1:5000' parece IPv6: hacen falta corchetes, como en [::1]:3344
--gui=[::1        ->  --gui: falta el corchete de cierre en '[::1'
--gui=[]:80       ->  --gui: direccion IPv6 vacia en '[]:80'
--gui=:5000       ->  --gui: falta el host antes de ':'
--gui=host:       ->  --gui: falta el puerto despues de ':'
```

**El IPv6 exige corchetes, y no es capricho:** `::1:5000` es una dirección IPv6
perfectamente válida, así que sin corchetes no hay forma de saber si esos dos
puntos finales separan un puerto o son parte de la dirección. Se rechaza en vez
de adivinar, y el error lo dice.

**Y `--gui --ondas` sigue siendo la ventana por omisión y las ondas**, no un
host llamado `--ondas`: lo que sigue al argumento se toma como destino solo si
no empieza por guion. Es la misma regla que `--help` ya usaba.

### 8.3 La política del host, que es una decisión

`--gui otra-maquina:3344` **funciona** —hace falta para una GUI remota— pero
saca un aviso por la salida de error:

```
AVISO: 'otra-maquina' no es la propia maquina. Este enlace NO esta
       autenticado: quien lo alcance podra ver el estado de la simulacion y
       accionar sus mandos. Los servidores de GDB de este programa solo
       escuchan en bucle local por esto mismo.
```

`es_bucle_local()` mira el **nombre**, no resuelve: resolver es una operación de
red y este fichero no hace red. La consecuencia está dicha y comprobada en T130:
un alias del `hosts` que apunte a `127.0.0.1` **avisa de más**. Avisar de más es
el lado bueno en el que equivocarse.

### 8.4 Cómo se ha comprobado

**T130 — «El argumento `--gui`: las formas buenas y las raras»**, 43
comprobaciones, todas puras: siete formas buenas, los cinco límites del puerto,
los seis casos malos con el texto de su error, ocho de `es_bucle_local()`, dos
de cómo se vuelve a escribir un destino y seis de las constantes del protocolo
—versión, puerto y los tamaños de `Cabecera`, `Orden` y `Muestra`—.

Esas seis últimas compilan por `static_assert` de todos modos; están además en
T130 porque **un número que solo vive en un `static_assert` no aparece en ningún
informe de pruebas**, y el tamaño de `Orden` es justo el dato que, si cambia sin
querer, desconecta los dos programas sin un solo error de compilación.

**El resultado, medido:**

| | Antes | Ahora |
| :--- | ---: | ---: |
| `make test407` | 2074 | **2117** *(+43)* |
| Tiempo simulado del F407 | `2336217899213 ps` | **`2336217899213 ps`** |
| `make test446` | 203, `1033367277932 ps` | igual |
| `make test417` | 164, `718988288 ps` | igual |
| `make red` | 13 | igual |
| `make asan407` | limpio | **limpio**, 2117/2117 |
| Placas que validan | 6, con 0 avisos | igual |

**El invariante no se ha movido**, que era lo único que esta fase podía romper.
No se ha movido porque las 43 comprobaciones nuevas son funciones puras sobre
cadenas: no pasan por el bus, no despiertan un proceso y no piden tiempo. Es la
misma doctrina con la que se escribieron las comprobaciones de las máscaras del
RCC y las del catálogo de MCU.

### 8.5 Lo que NO se ha hecho, que también es la fase 0

Ni un socket. Ni un `Observable`. Ni una ventana con contenido. `--gui` hoy
imprime una línea y sigue:

```
gui: hablaria con mcu-sim-gui en localhost:3344 (protocolo v1)
     -- fase 0: todavia no se conecta
```

### 8.6 Lo que sigue sin verificar, y es el riesgo R-1

La GUI **configura, compila, enlaza y arranca** en Linux con g++ 13 y Qt 6.4.2
(con `QT_QPA_PLATFORM=offscreen`). En **Windows y macOS no se ha probado**, ni
la GUI ni SystemC. Nada del esqueleto es específico de plataforma, pero eso no
es una demostración, y sigue siendo el riesgo heredado más grande del proyecto.

Un detalle del camino que vale como aviso: el `CMakeLists.txt` pedía **Qt 6.5**
por costumbre y aquí solo hay 6.4.2. Se bajó a **6.3**, que es la versión desde
la que existe `qt_standard_project_setup()` y por tanto el mínimo real. Pedir
más versión de la que se usa no protege de nada: solo deja fuera máquinas que
habrían funcionado.

### 8.7 Windows: dos diagnósticos equivocados y una causa real

El riesgo **R-1** se puso a prueba antes de lo previsto, con MSYS2, y **sigue
abierto**: `mcu-sim-gui` todavía no se ha compilado en Windows. Pero el camino
hasta saber por qué merece quedar escrito entero, incluidos los dos diagnósticos
que no eran.

**El síntoma.** CMake no podía leer ni borrar el `a.exe` que acababa de escribir
para identificar el compilador, y Ninja decía que un fichero **fuente** recién
creado estaba *ausente*:

```
file STRINGS file ".../CompilerIdCXX/a.exe" cannot be read.
file failed to open for reading (Permission denied)
ninja: error: '.../testCXXCompiler.cxx', missing and no known rule to make it
-- Check for working CXX compiler: /mingw64/bin/c++.exe - broken
```

**Diagnóstico 1, equivocado: Dropbox.** El árbol de compilación estaba dentro de
la carpeta sincronizada, y eso explica los síntomas de manera perfectamente
razonable —el cliente abre cada fichero recién creado para subirlo—. Sacarlo de
ahí era buena idea de todos modos. **Y falló igual.**

**Diagnóstico 2, incompleto: el CMake de MSYS con el compilador de MinGW.** La
traza citaba `/usr/share/cmake/...` mientras el compilador era
`/mingw64/bin/c++.exe`: dos entornos mezclados, un error de verdad que había que
corregir. Corregido, **falló igual**, y esta vez peor: el compilador declarado
`broken`.

**La causa real: el antivirus.** La prueba que lo cerró no usa CMake:

```bash
printf 'int main(){return 0;}\n' > t.cpp
g++ t.cpp -o t.exe && ls -la t.exe && ./t.exe
```

El `.exe` **se creó** —32 KB, `g++` terminó sin una queja—, **no se dejó
ejecutar** («Permission denied») y unos segundos después **había desaparecido
del directorio**. Ahí se acaba la ambigüedad: un fichero bloqueado da permiso
denegado; un fichero que se esfuma está **en cuarentena**.

Y el dato que parecía descartarlo era justo el que lo confirmaba: Defender
informaba `RealTimeProtectionEnabled: False`. Eso no quiere decir que no haya
antivirus —quiere decir que **hay otro** que se ha registrado como el del
sistema y Defender se ha apartado. Era **ESET**.

**Por qué pasa, y por qué no tiene arreglo desde el proyecto.** Un compilador
produce ejecutables **nuevos, sin firmar y desconocidos**, que es literalmente
el perfil que un antivirus heurístico busca. No hay compilador, generador ni IDE
que lo evite. La salida son dos exclusiones de rendimiento —`C:\msys64\*` y el
directorio de compilación—, y están escritas con su ruta de menús en el README.

**Las tres lecciones, que es para lo que sirve escribir esto.**

1. **El primer diagnóstico razonable no es el correcto por ser razonable.**
   Dropbox explicaba los síntomas y era falso. Lo que lo destapó fue bajar un
   escalón: quitar CMake de en medio y probar el compilador a pelo. Cuando una
   herramienta compleja falla de forma rara, la pregunta útil es *qué es lo más
   simple que también debería funcionar*.
2. **«El antivirus está desactivado» hay que leerlo dos veces.** Defender
   apagado es un síntoma de que manda otro, no de que no mande nadie.
3. **El fallo intermitente era una pista, no ruido.** En el primer intento la
   misma ejecución llegó a decir `works` después de haber fallado la detección
   de ABI. Un componente que se contradice consigo mismo casi siempre tiene a
   alguien de fuera manipulándole los ficheros.

**Consecuencia para el producto, y no es una anécdota.** `mcu-sim-gui` se va a
distribuir a alumnos como un `.exe` sin firmar, y lo que acaba de pasar aquí les
va a pasar a ellos en sus máquinas. Es trabajo de la **fase 9**, anotado allí.


---

### 8.8 La trampa que espera al alumno: las DLL

Saliendo de la §8.7 apareció algo que **no es de este plan pero decide la fase
9**, y que se midió en la misma máquina.

`mcu-sim.exe` —construido en MSYS2, 22,5 MB— hace esto según dónde se ejecute:

| Dónde | Qué pasa |
| :--- | :--- |
| Terminal **MINGW64** | funciona |
| **PowerShell** | no dice nada y no hace nada |
| **`cmd.exe`** | *«No se encuentra el punto de entrada `clock_gettime64` en la biblioteca de vínculos dinámicos…»* |

El mensaje **no dice que falte la DLL**: dice que la que ha encontrado no
exporta ese símbolo. La encontró, y es la equivocada. El GCC de MSYS2 usa el
modelo de hilos POSIX, así que `std::chrono` y SystemC arrastran una
importación de `libwinpthread-1.dll`; en el shell MINGW64 el `PATH` lleva
delante la buena, y fuera gana cualquier otra copia que haya por la máquina —de
otro MinGW, de Qt, de un IDE—.

Arreglado en `mcu-sim` con **`-static`** en el enlazado de Windows, que es lo
que `-static-libgcc -static-libstdc++` no cubría. Está contado en
`mcu-sim/doc/compilacion.md` §5.6.

**Y aquí está lo que importa para este plan.** Ese fallo es *exactamente* el que
tendrá el alumno que reciba el ejecutable: un programa que arranca en la máquina
del profesor y no en la suya, con un mensaje sobre una DLL de la que nunca ha
oído hablar, y que además **depende del orden de su `PATH`**, así que funciona
en unas máquinas y en otras no, aparentemente al azar. Es la peor clase de
incidencia de soporte que existe, y es justo el argumento de `[AG]` §18.2 —el
tiempo del profesor es el recurso escaso— apareciendo por tercera vez.

**Para `mcu-sim-gui` el problema es mayor**, no menor: además de las de MinGW
hay que llevar las de **Qt** —`Qt6Core`, `Qt6Gui`, `Qt6Widgets`, `Qt6Network`,
los complementos de plataforma—, y eso no lo arregla un `-static` porque un Qt
dinámico es lo normal. La herramienta es `windeployqt`, y hay que usarla en la
fase 9 y **probar el resultado en una máquina donde no haya Qt instalado**, que
es la única prueba que vale.

Dicho de otro modo: la fase 9 tiene ahora tres deberes, no uno. Firmar el
ejecutable (§8.7), enlazar estáticamente lo que se pueda, y empaquetar con
`windeployqt` lo que no.

---

## 9. Fase 1, ejecutada

Todo en `mcu-sim`, en la rama `gui`. Este repositorio solo cambia en su
documentación: este plan, el README y dos reglas nuevas de `doc/protocolo.md`.

### 9.1 Qué se ha escrito

* **`parts/part_base.h`**: `Observable`, `Mando` y los seis métodos virtuales
  de `ExtPartBase`, con valores por omisión. Las diecinueve piezas que no
  declaran nada compilan sin tocarlas.
* **`parts/ext_parts.h`**: las tres piezas. `Led` declara `encendido` (el que
  sugiere pintar) y `corriente` en mA; `Button`, `pulsado` y el mando `pulsar`,
  que es exactamente `press()`/`release()`; `Crystal`, `presente`.
* **`parts/frontera_gui.h`**, nuevo:
  * `Catalogo`, sobre una copia del inventario. Numera los observables con un
    `id_obs` plano, valida una orden sin aplicarla y escribe el XML de
    `T_CATALOGO` con el formato de `doc/protocolo.md` §3;
  * `FronteraGui`, el módulo con los dos `SC_THREAD`: el **muestreador** y el
    **aplicador**. Sus salidas son dos colas que vaciarán la fase 4
    (instantáneas) y la 5 (ecos de órdenes). Hasta que alguien llame a
    `activa()`, los dos esperan sobre un evento que nadie notifica.
* **`top/sim_main.cpp`**: la frontera se construye siempre y no se activa. La
  activará el saludo de la fase 3.
* **`top/sc_main.cpp`**: la misma frontera, sin activar, en el banco del F407.
  Ningún grupo nuevo (§9.3).
* **`top/sc_main_gui.cpp`**, nuevo: el banco **`testgui`** (§9.3).
* `Makefile.mcu-sim`, `ci/pasa_suites.sh`, los tres bucles del workflow y
  `verif/invariantes.txt`, para que `testgui` sea una suite más.

### 9.2 `Crystal` no tiene frecuencia, y no se le ha inventado

El plan pedía que `Crystal` declarase `frecuencia`. La pieza no la tiene, y su
ficha de `--help` dice que es a propósito: «NO LLEVA LA FRECUENCIA, y no es un
olvido: la del HSE es un dato del árbol de reloj y se configura en el RCC». Darle
un atributo para poder publicarlo duplicaría un dato del RCC y contradiría esa
ficha; y publicar un número que la pieza no tiene sería el modelo diciendo lo
que no sabe, que es peor que la GUI calculando lo que sí sabe (R-7). Así que
declara lo que sí sabe: si está soldado (`presente`). Si un día
hace falta enseñar la frecuencia del HSE, es un observable del RCC, no del
cristal.

### 9.3 Todas las comprobaciones, en un banco aparte

El plan pedía un grupo nuevo en `test407` con lo que no gasta tiempo. **Se
escribió, pasó, y se sacó de allí**, porque choca con una decisión posterior a
este plan: la **D-12** del puente UART (`mcu-sim/doc/analisis_puente_serie.md`
§10), que dice que lo que no necesita el banco del F407 no va en él, ni
siquiera para mover su recuento, porque T130 lo movió en la fase 0. Así que en
`test407` hay una frontera construida y sin activar, y **ninguna comprobación**:
la prueba es que su línea de `invariantes.txt` no ha cambiado.

Y el muestreador y el aplicador **solo hacen cosas en el tiempo**: dejarlos sin
probar hasta la fase 4 era dejar sin probar justo lo que esta fase escribe. Las
dos cosas van a un quinto banco, **`testgui`**, con su propia línea en
`invariantes.txt`, sobre una placa mínima sin chip: un LED, un pulsador que lo
enciende, un pulsador normalmente cerrado, dos resistencias y un cristal. Sus
87 comprobaciones:

| Grupo | Qué |
| :--- | :--- |
| G0 | sin gastar tiempo: lo que declara cada pieza, que `acciona()` hace lo mismo que `press()`/`release()` (también en el NC, donde `pulsado` es el dedo y no el contacto), el catálogo y su XML, la validación (`RES_PIEZA`, `RES_MANDO`, `RES_RANGO`, un NaN) y una frontera sin activar que no acepta nada |
| G1 | el catálogo de esa placa; antes de activar no se acepta nada |
| G2 | el ejemplo de `doc/protocolo.md` §5 a escala de ms: cuatro órdenes con deltas enviadas **antes de arrancar**, aplicadas en 1,00 / 1,50 / 4,00 / 4,22 ms, y siete instantáneas que las ven pasar |
| G3 | órdenes **en marcha**: la primera, relativa al instante en que se encola; un delta de 0, en el mismo instante y en su orden |
| G4 | una orden y una muestra en el **mismo instante**: la muestra ve la orden. Y la corriente del LED contra el nodo resuelto a mano |
| G5 | `RES_TARDE`, `RES_PIEZA`, `RES_MANDO` y `RES_RANGO`: siete órdenes, siete ecos |
| G6 | la suscripción: se rechaza entera si un id no existe, periodo 0 la apaga, las muestras caen en la rejilla desde t = 0 |
| G7 | el atasco: con la cola llena se tiran las nuevas, y la primera que entra después dice cuántas |
| G8 | una orden **anterior** que llega mientras el aplicador espera a otra; y la frontera sin activar, que no ha hecho nada en todo el banco |

**Y se ha comprobado que pueden fallar.** Seis mutaciones de
`frontera_gui.h`, una a una, y las seis rompen lo que tienen que romper:

| Mutación | Lo que cae |
| :--- | :--- |
| quitar el delta de espera del muestreador | G4: la muestra se adelanta a la orden |
| tratar las órdenes en marcha como absolutas | G3, G4, G5 y G8 (10 comprobaciones) |
| no marcar nunca `RES_TARDE` | G5 |
| tirar las instantáneas viejas en vez de las nuevas | G7 (2) |
| contar el periodo desde la suscripción y no desde t = 0 | G6 y G7 (3) |
| no volver a mirar la cola cuando llega una orden anterior | G8 (2) |

La primera es la que justifica una decisión que el protocolo no tomaba: **las
órdenes de un instante van antes que la muestra de ese instante.** Sin el
delta de espera, con SystemC 2.3.4, la muestra se tomaba antes de aplicar la
orden; con otra versión podría ser al revés, porque la norma no fija en qué
orden despierta dos procesos en el mismo instante. Está escrito ahora en
`doc/protocolo.md` §4.1.

### 9.4 Cómo se ha comprobado

Con SystemC 2.3.4 y g++ 13 en Linux, que es la combinación del trabajo de Linux
del CI; las otras tres plataformas lo dirán en el primer push del pull request.

| | Antes | Ahora |
| :--- | ---: | ---: |
| `test407` | 2118 | **2118** |
| `resto` del F407 | `2240553274213 ps` | **`2240553274213 ps`** |
| total del F407 | `2337219149213 ps` | **`2337219149213 ps`** |
| `test446` | 204, `1033367277932 ps` | igual |
| `test417` | 165, `718988288 ps` | igual |
| `testserie` | 189, `400677589564 ps` | igual |
| `testgui` | — | **87**, `26500000000 ps` |

Las cinco con `ci/pasa_suites.sh` y `ci/comprueba_invariante.sh`, como el CI.
Además:

* **`sim` simula lo mismo con y sin la frontera.** Las diez placas de
  `placas/` validan con la misma salida que el `mcu-sim` de antes, y cinco
  simulaciones (el blinky en dos placas, la Nucleo-F446RE, el VCP en memoria y
  una con `--gui`) dan la misma salida **y el mismo número de deltas**. Solo
  cambia, con `--gui`, la línea que dice que todavía no se conecta.
* **El netlist del banco no cambia**: `test407 --netlist` sigue dando
  `placas/banco.xml` byte a byte. La frontera no es una pieza.
* **`testgui` con ASan y UBSan, limpio.** Sin LeakSanitizer, que no funciona
  bajo `ptrace` en el entorno donde se probó.

### 9.5 Lo que NO se ha hecho, que también es la fase 1

Ni un socket. Nadie activa la frontera fuera de `testgui`, y `--gui` sigue
imprimiendo una línea y nada más. Las otras diecinueve piezas no declaran
observables ni mandos: se irán añadiendo cuando la GUI las necesite, y sin tocar
nada más, porque el catálogo recorre el inventario.

---

## 10. Fase 2, ejecutada

### 10.1 Qué se ha escrito

**El marco, una sola vez.** `proto_io.h` tiene el `Emisor` y el `Lector` del
marco: el emisor añade cabecera y cuerpo a un búfer; al lector se le meten los
bytes que hayan llegado, en los trozos que sea, y devuelve mensajes completos.
No sabe de sockets ni de lo que significa un mensaje. Comprueba lo que manda
`doc/protocolo.md` §2: la magia, la versión, la longitud (con la cabecera sola:
4 GB se rechazan sin esperar a que lleguen) y el sentido. Ante cualquiera de
esas cuatro se queda roto y no intenta resincronizar. Un tipo desconocido no es
un error: lo entrega, y `es_conocido()` dice que no lo es. Ninguno de los dos
reserva memoria por mensaje: trabajan sobre un búfer que se reutiliza.

El plan pedía un codificador en `mcu-sim` y «el mismo par» en la GUI. Escribirlo
dos veces era tener dos lectores que se pueden separar. Así que es **un
fichero** que vive en los dos repositorios, como `protocolo.h`: la copia de
referencia es la de aquí y la de `mcu-sim/src/common/` es vendida.

**Una regla que el protocolo no decía.** `version` es «la que está en uso en
esta conexión», pero antes del saludo no hay ninguna en uso. Ahora sí: **la 1**,
la del saludo, en los dos sentidos, y la negociada después. Si no, un modelo
nuevo que ofreciese la 2 en un `T_HOLA` con un 2 en la cabecera sería rechazado
por una GUI vieja antes de poder ofrecérsela. Está en `doc/protocolo.md` §2.

**En `mcu-sim`:**

* `common/red.h`: **`conecta(host, puerto)`** y **`escucha(host, puerto)`**,
  **al lado** de `conecta_local` y `escucha_local`, que siguen ahí y siguen
  siendo las de GDB. Hablan IPv4 e IPv6 —`--gui [::1]` ya era una forma
  válida— y quitan los corchetes. `conecta` prueba cada dirección del host con
  un plazo de 5 s, porque un `connect` bloqueante contra una máquina que no
  contesta puede tardar minutos y el modelo no debe quedarse colgado antes de
  arrancar. Y dos ayudas: `puerto_local()`, para escuchar en el puerto 0 y
  saber cuál dio el sistema, y `espera_legible()`, para leer sin girar en
  vacío, que es lo que necesitará el saludo de la fase 3.
* `common/proto_io.h`, la copia vendida.
* `verif/prueba_gui_proto.cpp` y **`make gui-proto`** (§10.2).
* `verif/prueba_macros_win.cpp` incluye ahora `proto_io.h`. Y no era
  decorativo: el primer borrador del lector tenía un `enum { FALTA, MENSAJE,
  ERROR }`, y `ERROR` es una macro de `<windows.h>`. Los estados se llaman
  ahora `LEC_FALTA`, `LEC_MENSAJE` y `LEC_ERROR`.

**En `mcu-sim-gui`:**

* `src/conexion.h` y `src/conexion.cpp`: **`Conexion`**, sobre `QTcpServer` y
  `QTcpSocket`. Escucha, acepta **un** modelo (otro se cierra en el acto y se
  cuenta), entrega cada mensaje completo con la señal `mensaje()`, salta y
  cuenta los desconocidos, y si llega algo que no es el protocolo cierra y
  dice por qué con `desconectado()`. Cada conexión empieza de cero.
* `src/proto_io.h`, la copia de referencia.
* `CMakeLists.txt`: el transporte es una biblioteca sin Widgets, y hay pruebas
  con `ctest` que no necesitan pantalla.
* `tests/prueba_conexion.cpp` (§10.2).

### 10.2 Cómo se ha comprobado

**`make gui-proto`**, 40 comprobaciones sin SystemC, en tres partes:

| Parte | Qué |
| :--- | :--- |
| P1, el marco sin red | la cabecera byte a byte (`MSG1` legible en un volcado); los diecinueve tipos de ida y vuelta de un golpe, byte a byte y en trozos de 3, 7, 13, 101, 4099 y 65537 bytes; los casos feos: magia mala (un `GET /` de navegador), versión 0, versión 2 antes de negociar, versión distinta de la negociada, longitud de 4 GB, 8 MiB justos (legal), tipo del otro sentido, tipo desconocido que se salta, salto de secuencia |
| P2, por un socket de verdad, con dos hilos | un hilo hace de modelo con `conecta("localhost")` y el otro de pantalla con `escucha("127.0.0.1", 0)`: los diecinueve tipos en los dos sentidos; una placa de 300 kB que llega en 75 `recv`; un mensaje **partido en dos `recv` a la fuerza** (el emisor espera a que el lector haya leído la primera mitad); **dos mensajes en un solo `recv`**; un desconocido de 300 bytes en medio; el cierre visto como cero bytes; escribir después del cierre sin morir; un navegador en el puerto; `conecta` sin nadie escuchando; un host que no resuelve; IPv6 si la máquina lo tiene |
| P3, las copias | `protocolo.h` y `proto_io.h` idénticos byte a byte a los de `mcu-sim-gui` (R-6) |

**La prueba de la GUI**, 16 comprobaciones con `ctest`, la `Conexion` de verdad
contra un `QTcpSocket` crudo que escribe con el mismo `proto_io.h`: cinco
mensajes con una placa de 300 kB, uno partido a mitad de la cabecera, tres de
un golpe con un desconocido en medio, cinco de vuelta, un segundo modelo
rechazado, el cierre normal, la reconexión, el navegador, los 4 GB, un tipo de
pantalla llegando a la pantalla y `cierra()`.

**Y se ha comprobado que pueden fallar.** Seis mutaciones de `proto_io.h`, una
cada vez:

| Mutación | Lo que cae |
| :--- | :--- |
| sin el techo de longitud | los 4 GB |
| sin comprobar el sentido | las dos pantallas |
| sin comprobar la versión | las tres de versión |
| no avanzar tras un mensaje | 12, casi todo |
| la secuencia no sube | 9 |
| resincronizar después de un error | **nada**, y es correcto: un lector roto no acepta más bytes, así que no hay con qué resincronizarse. Esa propiedad la da la construcción, no la prueba |

**Dónde se ha ejecutado:**

* Linux, g++ 13: `make gui-proto` 40/40, `make red` 13/13, `macros-win`, y la
  prueba de la GUI 16/16 con Qt 6.4.2.
* **Windows, cruzado**: `gui-proto` y `red` compilan con MinGW-w64 sin un aviso,
  y **corren bajo Wine**: 40/40 y 13/13. No es un Windows de verdad —Wine
  traduce a sockets de Linux—, pero ejercita la rama de Winsock del código.
* **IPv6 no se ha podido probar**: la máquina donde se hizo no tiene IPv6 en el
  núcleo, y la prueba lo salta diciéndolo. Lo dirá el CI.
* Las cinco suites, igual que antes: 2118 / 204 / 165 / 189 / 87 y los cinco
  invariantes al picosegundo. `red.h` lo incluyen los servidores de GDB y el
  puente serie, y no se ha movido nada.

### 10.3 El CI

* `rapidas` clona también **este repositorio** y ejecuta
  `make gui-proto GUI_REPO=../mcu-sim-gui`. Compara con la rama principal de
  aquí, así que **un cambio del protocolo se sube primero aquí** y después a
  `mcu-sim`; al revés, el CI de `mcu-sim` falla, que es lo que tiene que pasar.
  Este repositorio es público y no hace falta token; si deja de serlo, hará
  falta uno.
* `macos` y `windows` pasan `make red gui-proto`, sin comparar copias, que ya se
  comparan en `rapidas`.
* Y este repositorio tiene **CI propio** desde el mismo día:
  `.github/workflows/ci.yml` compila y pasa `ctest` en Linux (Qt 6.4.2 del
  sistema, Ubuntu 24.04: la 22.04 trae la 6.2, por debajo de la 6.3 que pide
  `CMakeLists.txt`), en Windows (MSYS2 y el Qt de MinGW-w64) y en macOS (el Qt
  de Homebrew); en Linux, además, arranca la ventana con `offscreen` y
  comprueba que no se cae. No compara las copias compartidas: eso ya lo hace el
  CI de `mcu-sim`. Es lo que cerrará la mitad de **R-1** que sigue abierta,
  que esto compile en Windows, en cuanto pase allí la primera vez.

### 10.4 Lo que NO se ha hecho, que también es la fase 2

Ni un mensaje con significado: nadie manda un `T_HOLA` de verdad todavía.
`sim --gui` sigue imprimiendo una línea y nada más, y la ventana no usa la
`Conexion`. Las dos cosas son la fase 3.

---

## 11. Fase 3, ejecutada

### 11.1 Qué se ha escrito

**En `mcu-sim`:**

* **`common/gui_cliente.h`**, nuevo: `ClienteGui`, el saludo del lado del
  modelo, **sin SystemC**. Conecta, manda `T_HOLA`, espera `T_VERSION` (10 s
  como mucho), manda `T_PLACA`, `T_CATALOGO` y `T_LISTO`, y espera `T_ARRANCA`
  sin plazo: contesta `T_PING`, acepta `T_PARA` e ignora `T_SUSCRIBE` y
  `T_ORDENES`, que son de las fases 4 y 5. Y `fin()`, que manda `T_FIN` y
  cierra.
* **`top/sim_main.cpp`**: `saluda_gui()`, entre construir la placa y
  `sc_start()`. `T_PLACA` es `Netlist::volcar_xml`, la placa declarada con los
  `--serie` ya aplicados: el mismo volcado que `test407 --netlist`. `T_CATALOGO`
  es el `Catalogo` de la fase 1, y con ese mismo catálogo se **activa la
  frontera**, para que los índices de las fases 4 y 5 sean los que la ventana
  ya tiene. Activarla no cuesta nada sin suscripciones ni órdenes. Al acabar la
  ventana, `T_FIN` con el instante real; si `muere()`, `T_FIN` con `M_ERROR`.
  `T_HOLA` lleva `protocolo_max`, `mcu_sim`, `pid`, `placa`, `mcu`, `firmware`,
  `argumentos` y `modo`. El contenido de `T_ARRANCA` (ritmo, factor, ventana)
  se lee y no se usa: es la fase 6.
* `parts/part_base.h` y `parts/netlist.h`: `pon_id()` (§11.3).

**En `mcu-sim-gui`:**

* **`src/placa.h/.cpp`**: lee `T_CATALOGO` y `T_PLACA` y los junta por el `id`
  de cada pieza en un `PlacaGui`: piezas con sus observables, sus mandos, sus
  patillas y si están soldadas. Solo QtCore. Ni un tipo de C++ del modelo.
* **`src/sesion.h/.cpp`**: `Sesion`, el saludo del lado de la ventana, encima
  de `Conexion`. Elige la versión más alta que conocen los dos (o `protocolo=0`
  y cierra), junta la placa, avisa con `listo()` y manda `T_ARRANCA` y `T_PARA`.
* **`src/panel.h/.cpp`**: `construye_panel()`, la pantalla a partir de
  `PlacaGui`: un recuadro por pieza con sus patillas, un indicador por
  observable interesante (y cuántos no se pintan), y un control por mando según
  su tipo —botón, casilla o deslizador—. Los indicadores dicen «—» y los
  controles están desactivados hasta las fases 4 y 5. Cada widget lleva su
  identificador del protocolo como `objectName`.
* **`src/ventana_principal.*` y `main.cpp`**: la ventana escucha al abrirse
  (`--puerto N`, por omisión 3344), construye el panel al llegar la placa, y
  tiene «Arrancar» y «Parar».
* `src/conexion.cpp`: **`cierra()` ahora es ordenado**. Usaba `abort()`, que
  tira lo que quede por escribir, y lo último antes de un cierre suele ser lo
  que lo explica: un `T_VERSION protocolo=0`. Lo cazó la prueba de la sesión.
* `CMakeLists.txt`: tres bibliotecas por capas —`transporte`, `sesion`,
  `pantalla`— y cuatro pruebas.

### 11.2 `--valida --gui`, decidido

Placa, catálogo y `T_FIN`, **sin `T_LISTO` y sin esperar**, y `modo=valida` en
`T_HOLA` para que la ventana lo sepa desde el principio. Sirve para enseñar una
placa sin simularla. Una placa con errores no llega a conectarse: `mcu-sim`
muere antes, con código 2 y la explicación en la salida de error, que la
ventana verá cuando sea ella quien lo lance (fase 7). Se descartaron dos
alternativas: rechazar la combinación —quita un uso legítimo— y mandar los
avisos de validación por `T_AVISO` —es la fase 4, y la salida de error ya los
lleva—.

### 11.3 Un fallo de la fase 1, destapado aquí

El catálogo usaba `ExtPartBase::pieza()` como `id`. Las piezas que son
`sc_module` (un `Led`) lo reciben en el constructor; **las que no** (`Crystal`,
`Button`, `Rpull`) se quedaban con un nombre numerado. Resultado, en la placa de
la Discovery: el catálogo hablaba de `Crystal_1` y `Button_3`, y la placa de
`X2` y `B1`. La ventana no podía casarlos.

Nadie lo veía porque hasta ahora nadie juntaba los dos XML: la fase 1 solo
comprobaba que el catálogo se escribía bien. Se arregla en el origen:
`Netlist::construye` le pone a cada pieza el `id` de su `<componente>`
(`pon_id`), así que `X2` se llama `X2` en todas partes. Lo único que mostraba
el nombre numerado era el volcado de diagnóstico `test407 --inventario`.
`make gui-saludo` lo comprueba: **cada componente de la placa está en el
catálogo con el mismo id y el mismo tipo**. Quitando el arreglo, cae.

### 11.4 Cómo se ha comprobado

**La prueba que pide el plan**, `make gui-saludo` (`verif/gui/saludo.py`, 22
comprobaciones): el `mcu-sim` de verdad con `placas/discovery_min.xml` y el
blinky, contra una ventana hecha en Python con **su propia implementación del
marco** —no comparte `proto_io.h`, a propósito: un error en ese fichero no lo
cazaría ninguno de los dos extremos de verdad—:

| Grupo | Qué |
| :--- | :--- |
| S1 | `T_HOLA` con lo que tiene que decir; `T_PLACA`, `T_CATALOGO`, `T_LISTO`; los 11 componentes de la placa en el catálogo con su id y su tipo |
| S2 | **sin `T_ARRANCA`, espera**: dos segundos después sigue vivo y ha gastado **0,000 s de CPU**; contesta `T_PING`; y a `T_PARA` contesta `T_FIN` con el tiempo simulado **en cero**. «Para siempre sin avanzar un picosegundo» |
| S3 | con `T_ARRANCA` simula y manda `T_FIN` en **50 ms y 110 µs** —la ventana más el arranque eléctrico—, y la simulación es **la misma que sin `--gui`**: los mismos cuatro LEDs y los mismos **3186 deltas**, con un `T_SUSCRIBE` y un `T_ORDENES` de por medio |
| S4 | `--valida --gui` |
| S5 | nadie escuchando, ninguna versión común, la ventana cerrando antes de arrancar: código 2 y dicho |

**`make gui-proto`**, que pasa de 40 a 58: la parte nueva, P4, es `ClienteGui`
contra una GUI falsa en otro hilo —el camino bueno, `T_PARA`, `--valida`,
`T_PING`, y ocho maneras de fallar, cada una con su mensaje—. Compilado también
con MinGW y ejecutado bajo Wine: 58/58.

**`ctest` en esta GUI**, cuatro pruebas:

| Prueba | Comprobaciones | Qué |
| :--- | ---: | :--- |
| `conexion` | 16 | la de la fase 2, que sigue igual |
| `sesion` | 24 | los dos XML juntos (y lo que no casa), el saludo contra un modelo falso, la versión, `T_ARRANCA`, `T_FIN`, y lo que no puede ser |
| `ventana` | 15 | **con widgets de verdad y sin pantalla** (`offscreen`): el panel —cuatro recuadros, tres indicadores, un control, la pieza desoldada, las patillas, y un servo que la ventana no ha visto nunca saliendo igual— y la ventana entera contra un modelo falso, pulsando «Arrancar» |
| `cruzada` | 12 | **la `Sesion` de verdad contra el `mcu-sim` de verdad**, con la placa de la Discovery: se construye entera, arranca y termina; y para sin simular. Necesita `MCU_SIM` y `MCU_SIM_SRC`; sin ellas se salta, como en el CI |

Una mutación de comprobación: con `cierra()` volviendo a `abort()`, la prueba
de la sesión falla, porque el `T_VERSION protocolo=0` no llega.

Y **las cinco suites de `mcu-sim`, igual que antes**: 2118 / 204 / 165 / 189 /
87 y los cinco invariantes al picosegundo, con `pon_id` cambiando el nombre de
las piezas del banco. `test407 --netlist` sigue dando `placas/banco.xml` byte a
byte, y la interoperabilidad del puente UART, 47/47.

### 11.5 Lo que NO se ha hecho, que también es la fase 3

Los indicadores no tienen valores ni los controles hacen nada: son las fases 4
y 5. El contenido de `T_ARRANCA` no se usa: fase 6. La ventana no lanza el
modelo: hoy se lanza a mano con `--gui`, y lanzarlo es la fase 7.

No se ha probado en Windows ni en macOS más que lo que el CI diga: `gui-saludo`
corre en los tres trabajos de `mcu-sim`, y `ctest` en los tres de este.

---

## 12. Fase 4, ejecutada

### 12.1 Qué se ha escrito

**En `mcu-sim`:**

* **`parts/enlace_gui.h`**, nuevo: `EnlaceGui`, un `SC_THREAD` que despierta
  cada 100 µs simulados —el patrón del GDB— y en cada vuelta lee lo que manda
  la ventana (`T_SUSCRIBE` a la frontera, `T_PING` con su `T_PONG`; lo de las
  fases 5 y 6, leído e ignorado), escribe lo que se quedara atascado, pasa a la
  salida los avisos y las instantáneas, y pone un `T_ESTADO` detrás de cada
  tanda o, cada 250 ms de pared sin ninguno, uno suelto. **No sabe de
  sockets**: habla con un `CanalGui`. En `sim` el canal es el socket; en
  `testgui`, un búfer en memoria que se atasca a voluntad. Así la
  contrapresión se prueba sin un socket y de forma determinista, que es lo
  único que la hace probable: el sistema operativo amortigua megas.
* **La contrapresión**, como la escribía el plan: la salida no pasa de 256 kB;
  las instantáneas que no caben se quedan en la cola del muestreador, que tira
  las nuevas y las cuenta; los avisos esperan en la suya y, si llegan a 1000,
  `T_FIN` con `M_ERROR` va delante de todo, la conexión se cierra **y la
  simulación sigue**.
* **El desvío de `SC_REPORT`**: el enlace pone su manejador al activarse,
  encadenado al de antes —la consola sigue igual—, y lo quita al perder la
  conexión. Va a la ventana lo que se ve en la consola; lo silenciado no; el
  «Simulation stopped by user» del núcleo, tampoco.
* **`common/gui_mensajes.h`**, nuevo: el `CanalGui`, y los cuerpos de
  `T_AVISO` y `T_SUSCRIBE`. No va en `proto_io.h`, que es el mismo fichero en
  los dos repositorios.
* **`common/gui_cliente.h`**: el saludo manda los avisos de placa, guarda el
  último `T_SUSCRIBE` de antes de arrancar y, con `T_ARRANCA`, le **entrega**
  la conexión al enlace —un `CanalSocket`— con el emisor y el lector tal como
  quedaron.
* **`top/sim_main.cpp`**: el enlace se construye siempre y se activa al
  arrancar; la suscripción previa se aplica antes de `sc_start()`; al acabar,
  `termina()` vacía lo pendiente, manda un `T_ESTADO` final y `T_FIN`.
* **`top/sc_main.cpp`**: `test407` lleva un `EnlaceGui` construido y sin
  activar.

**En `mcu-sim-gui`:** `Sesion` lee `T_INSTANTANEA`, `T_AVISO` y `T_ESTADO` y
manda `T_SUSCRIBE`; el panel pasa a ser una clase, `Panel`, que pone cada
muestra en su indicador; y la ventana se suscribe al recibir `T_LISTO` —antes
de arrancar— a lo que pinta, a 60 Hz simulados, enseña los dos relojes y las
instantáneas perdidas, y tiene una lista de avisos. «Parar» se desactiva en
marcha: parar en marcha es la fase 6.

**Cómo se escribe un valor, sin conocer un tipo:** lo decide la declaración
del observable. Uno de 0 a 1 sin unidad es un sí o un no, y se pinta ● / ○;
cualquier otro, el número con su unidad.

### 12.2 Los avisos de placa, en el saludo

El plan decía que el desvío de `SC_REPORT` pondría «delante del alumno los
avisos de placa que hoy solo ve quien ejecuta `--valida`». Pero esos avisos no
son de `SC_REPORT`: los imprime `sim` al montar la placa, antes de que exista
la simulación. Así que van por otro camino, y en otro momento: como `T_AVISO`
de origen `placa` **entre `T_CATALOGO` y `T_LISTO`**, también con `--valida`.
Es cuando sirven: antes de pulsar «arranca», que es cuando un aviso todavía
ahorra tiempo.

### 12.3 Un fallo de `--tiempo-real` que ya estaba

Con una ventana finita, `sim --tiempo-real` hacía una sola espera de toda la
ventana: simulaba los segundos de golpe, en centésimas, y luego dormía lo que
faltaba. El total cuadraba con el reloj de pared, y en la consola no se notaba
—solo se ve cómo acaba cada LED—. Con una ventana mirando es lo primero que se
ve: todo llega en una ráfaga, y luego nada.

Ahora va en rodajas de 1 ms, como ya iba el bucle de GDB y de los puentes. Y al
medirlo apareció un segundo defecto: el freno se calculaba rodaja a rodaja, y
lo que cada `sleep_for` se pasaba se iba sumando, **un 10 % de retraso** a los
pocos segundos. Ahora se mide contra un ancla, y si el modelo va por detrás más
de 50 ms el ancla se mueve: no se acumula deuda. Medido con el blinky: los
flancos de 100 ms simulados llegan cada 100 ms de pared, con un desfase fijo
de unos 50 ms del arranque.

### 12.4 Cómo se ha comprobado

**La prueba que pide el plan**, `make gui-marcha` (`verif/gui/marcha.py`, 24
comprobaciones), con la misma ventana en Python de la fase 3:

| Grupo | Qué |
| :--- | :--- |
| M1 | `discovery_min.xml` con el blinky, suscrito desde antes de arrancar a `encendido` y `corriente` de LD4 cada 1 ms: **mil instantáneas sin un hueco ni una pérdida, y el LED parpadea en ellas: seis flancos en 3, 103, 203, 303, 403 y 503 ms, separados 100 ms**, que es lo que programa el firmware. La corriente acompaña (1,77 mA encendido). Los `T_ESTADO`, sin retroceder, y el último, `TERMINADA` y en el instante del `T_FIN` |
| M2 | otra ejecución igual: **las mil instantáneas idénticas**, instante a instante y valor a valor |
| M3 | una placa con un conflicto eléctrico: el aviso llega entre `T_CATALOGO` y `T_LISTO`, con y sin `--valida`, y la consola lo sigue diciendo |
| M4 | en marcha, con `--tiempo-real`: el latido sin suscripción, una suscripción nueva que da muestras desde el siguiente múltiplo del periodo, `T_PING`, una suscripción a un id que no existe (aviso, y la anterior sigue) y una vacía que las apaga |
| M5 | la ventana se va en marcha: `sim` lo dice, simula su ventana entera y sale con 0 |

**`testgui`**, G9 a G12 contra el canal en memoria (87 → 114 comprobaciones, y
`70550000000 ps`): instantáneas con su `T_ESTADO`, `T_PING`, el latido con un
reloj de pared falso, los avisos que se ven y los que no, las suscripciones
malas; el atasco —la salida a su tope, la cola llena, 14 instantáneas tiradas
y **dichas exactamente** por la primera que llega, y las que llegan más las que
faltan son todas las tomadas—, tres avisos que esperan y llegan en orden, el
sexto que cierra con `T_FIN` delante; la ventana que se va; y `termina()`.
**Seis mutaciones del enlace**, una a una, y las seis caen.

**`test407` no cambia**, ni en picosegundos ni en recuento, con el enlace
construido dentro: 2118 y `resto` en `2240553274213 ps`. Las otras tres suites
tampoco, `banco.xml` sale igual, la interoperabilidad del puente UART da 47/47,
`gui-saludo` 22/22 y `gui-proto` 61/61.

**En esta ventana**, con `ctest`: `sesion` 33, `ventana` 20 —con el formato de
los valores, la suscripción al recibir `T_LISTO`, las muestras en los
indicadores, los relojes, las pérdidas y los avisos— y `cruzada` 14 contra el
`mcu-sim` de verdad: **700 muestras, y el LED parpadea en ellas con flancos
cada 100 ms**.

### 12.5 Lo que cambió de la fase 3

Con `--gui`, `sim` ya no da los mismos deltas que sin él: el enlace es un
proceso que despierta. `gui-saludo` compara ahora los LEDs —tensión y
corriente— y el instante final, que siguen siendo iguales; que la secuencia
observada se repite lo comprueba M2.

### 12.6 Lo que NO se ha hecho, que también es la fase 4

Los mandos no hacían nada —los ecos de órdenes se recibían y se ignoraban—:
era la fase 5, §13. El ritmo de `T_ARRANCA`, la pausa y parar en marcha: fase 6. Y las
instantáneas de la ventana van a 60 Hz **simulados**, que sin ritmo pueden ser
cientos por segundo de pared: qué se pinta y cada cuánto, cuando el ritmo
exista.

---

## 13. Fase 5, ejecutada

### 13.1 Qué se ha escrito

**En `mcu-sim`:**

* **`parts/enlace_gui.h`**: el enlace lee `T_ORDENES` y lo encola en la
  frontera con el instante en que lo lee —el «sacarla de la cola» de
  `protocolo.md` §5—; `ordenes(cuerpo, en_marcha)` es la misma puerta para los
  que llegaron antes de arrancar. Y vacía hacia la ventana los **ecos** que deja
  el aplicador de la fase 1, un `T_ORDEN_HECHA` por orden, mezclados con los
  avisos por su instante simulado —a igual instante, primero el eco—. **Los
  ecos no se tiran**: son lo único que dice cuándo se aplicó de verdad una
  orden, y lo que la fase 8 grabará. Esperan en la cola de la frontera, con el
  mismo máximo que los avisos y la misma salida si se llena.
* **La validación sin silencios**: la de pieza, mando y rango ya la hacía el
  catálogo de la fase 1 al aplicar; lo nuevo es que el recorte de rango lleva
  **un `T_AVISO` justo detrás de su eco**, que nombra la pieza y el mando, dice
  de qué lado se salió, qué se aplicó y el rango. Y que un `T_ORDENES` mal
  formado —vacío, o que no es un múltiplo de 16 bytes— no se aplica ni a
  medias, y lo dice.
* **`common/gui_cliente.h`**: guarda **todos** los `T_ORDENES` de antes de
  `T_ARRANCA`, en orden —no el último, como la suscripción—.
* **`top/sim_main.cpp`**: los encola antes de `sc_start()`, detrás de la
  suscripción previa. Son instantes absolutos: la secuencia es reproducible.
* **`common/gui_mensajes.h`**: el cuerpo de `T_ORDENES`, de ida y de vuelta.

**En `mcu-sim-gui`:** `Sesion::ordena()` manda `T_ORDENES` —antes de arrancar
y en marcha— y `T_ORDEN_HECHA` es la señal `orden_hecha()`. Los controles del
`Panel` dejan de estar muertos: nacen apagados, `activa_mandos()` los enciende
con `T_LISTO` y los apaga con `T_FIN`, y cada uno emite `orden()` según el
**tipo del mando y su rango declarado**, sin saber qué pieza es: el botón, el
máximo al hundirlo y el mínimo al soltarlo; la casilla, su máximo o su mínimo;
el deslizador, su posición. La ventana manda cada toque al momento, y pone en
la lista de avisos los ecos que no son `RES_OK`, menos el de rango, que ya
avisa el modelo.

### 13.2 Cómo se ha comprobado

**La prueba que pide el plan**, `make gui-ordenes` (`verif/gui/ordenes.py`, 23
comprobaciones), con la ventana en Python de las fases 3 y 4 y sin GUI:

| Grupo | Qué |
| :--- | :--- |
| O1 | **El ejemplo del enunciado** sobre B1 de `discovery_min.xml`, en un solo `T_ORDENES` antes de `T_ARRANCA`: **los cuatro ecos en exactamente 1,000000000 / 1,500000000 / 4,000000000 / 4,220000000 s**, con `RES_OK`; llegan **a medida que se aplican**, cada uno justo detrás de la instantánea de 10 ms antes; y `pulsado`, muestreado cada 10 ms, vale 1 justo en [1,00, 1,50) y [4,00, 4,22) —la muestra de 1,00 ya ve la pulsación—. Sin un aviso |
| O2 | otra ejecución igual: los mismos ecos y las mismas 500 instantáneas, intercaladas en el mismo orden |
| O3 | en cinco mensajes antes de arrancar —cada uno con su primer instante absoluto— y uno malformado: pieza inexistente → `RES_PIEZA`; un LED, que no tiene mandos, y un mando 7 → `RES_MANDO`; 5 y −2 → recortados a 1 y 0, `RES_RANGO` y **cada uno con su `T_AVISO` justo detrás**; **delta 0 → pulsa y suelta en el mismo instante, en el orden del mensaje**, y la muestra de ese instante ve lo último; 20 bytes → un `T_AVISO`, y ninguna orden |
| O4 | en marcha, con `--tiempo-real`: la primera orden cae 100 ms después de un instante que la ventana no conoce —nunca antes de lo último que vio más 100 ms—, y las demás guardan sus deltas **exactos** |

**`testgui`**, G13 contra el canal en memoria (114 → 126 comprobaciones, y
`75300000000 ps`): la orden relativa a la vuelta que la lee, al microsegundo;
el primer eco que sale antes de que se apliquen las demás; delta 0; los cuatro
resultados con el aviso detrás de los de rango; dos `T_ORDENES` malformados;
doce ecos que esperan con el canal atascado y llegan en orden, **y detrás** el
aviso que se produjo después; y veinte que cierran la conexión, con la orden
aceptada aplicándose igual. **Cinco mutaciones del enlace**, una a una, y las
cinco caen; y quitar el encolado de las órdenes previas en `sim` tumba
`gui-ordenes`.
`gui-proto` P4 (61 → 64): los `T_ORDENES` de antes de arrancar se guardan
todos y enteros, y el cuerpo de ida y vuelta.

**`test407` no cambia**, ni en picosegundos ni en recuento: 2118 y `resto` en
`2240553274213 ps`. Las otras tres suites tampoco, `gui-saludo` 22/22 y
`gui-marcha` 24/24.

**En esta ventana**, con `ctest`: `sesion` 41 (G5: `ordena()` antes de arrancar,
en marcha y terminada; los ecos; uno que no mide lo que debe), `ventana` 31
(lo que ordena cada tipo de control; los mandos se encienden con `T_LISTO` y se
apagan con `T_FIN`; un toque antes de arrancar y otro en marcha; qué ecos van a
la lista de avisos) y `cruzada` 16 contra el `mcu-sim` de verdad: **B1 pulsado
de 200 a 250 ms con dos ecos en esos instantes exactos, y uno con `RES_PIEZA`,
mientras el blinky parpadea igual**.

### 13.3 Un fallo de la prueba cruzada, destapado aquí

`QProcess::waitForFinished()` devuelve `false` si el proceso **ya** ha
terminado, y `mcu-sim` termina en cuanto manda `T_FIN`: si lo hacía mientras la
prueba esperaba con el bucle de eventos en marcha, «termina con código 0»
fallaba sin que nadie hubiera hecho nada mal. Con las órdenes esa ventana de
tiempo creció, y falló dos de cada tres veces; sin ellas no falló en tres
ejecuciones, pero la carrera era la misma. Ahora se mira
primero si el proceso sigue vivo.

### 13.4 Lo que NO se ha hecho, que también es la fase 5

La grabación de sesiones: es la fase 8, y los ecos con su instante real son lo
que grabará. El control de la simulación —ritmo, pausa, parar en marcha—: fase
6. Y la ventana no refleja el eco en el control: una casilla marcada cuya orden
se recortó sigue marcada; lo que el modelo aplicó lo enseñan los indicadores.

---

## 14. Fase 6, ejecutada

### 14.1 La trampa, y cómo se ha resuelto

El plan pedía resolver que el socket lo atiende un proceso de SystemC, y que
por eso «solo se atiende si el tiempo simulado avanza». Proponía trocear
rodajas cortas de tiempo simulado con el resto del modelo quieto. **Se ha hecho
algo más literal, y más barato**: todos los procesos de SystemC comparten un
hilo del sistema operativo, así que mientras el enlace no llame a `wait()` no
corre ningún otro y el tiempo simulado no se mueve. **La pausa es un bucle de
reloj de pared dentro del proceso del enlace**: duerme en el socket
(`CanalGui::espera_lectura`, un `select`) hasta que llega algo, contesta
`T_PING`, acepta suscripciones y órdenes, manda su latido con `F_PAUSADA`, y
vuelve al llegar `T_SIGUE`. Es lo que ya hacía `--espera-terminal` con los
puentes serie antes de arrancar, ahora en marcha. El modelo queda exactamente
donde estaba —ni un picosegundo, ni un delta— y no gasta CPU.

### 14.2 Qué se ha escrito

**En `mcu-sim`:**

* **`parts/enlace_gui.h`**: `T_PAUSA`, `T_SIGUE`, `T_PASO` y `T_PARA`. La pausa,
  como en §14.1. **`T_PASO` para en t + ns exacto**: el proceso acorta su
  última espera para despertar justo ahí, aunque no caiga en la rejilla de
  100 µs. Pasos seguidos se suman. `T_PARA`, desde la marcha o desde la pausa,
  es `sc_stop()`, y `parada()` se lo dice a quien llamó a `sc_start()`. Si la
  ventana se va en pausa, la pausa se quita; y con `para_al_perderse()` —la
  ventana de tiempo sin fin— la simulación se para, porque ya no queda quien
  la pare.
* **`common/gui_mensajes.h` y `gui_cliente.h`**: `CanalGui::espera_lectura`, y
  su `select` en el `CanalSocket`.
* **`top/sim_main.cpp`**: `aplica_arranca()` aplica el contenido de
  `T_ARRANCA`, que desde la fase 3 se leía y no se usaba. El ritmo: `RIT_REAL`
  con su factor es el freno de `--tiempo-real` —que con `--gui` deja de
  mandar, y se dice—; `RIT_DEMANDA` arranca en pausa. La ventana de tiempo:
  `ventana_ns` > 0 manda; **0 es la de la línea de órdenes o, si no se dio,
  sin fin** —ahora se puede parar desde la ventana—. El resumen del final
  (lo simulado y cada LED) pasa a `informe()`, que se dice también al parar
  desde la ventana; y al salir de una pausa el freno se reancla.
* **`common/protocolo.h`**, en los dos repositorios: el comentario de
  `ventana_ns`, que decía «0 = indefinida» y ahora dice lo de arriba.

**En `mcu-sim-gui`:** `Sesion::arranca(ritmo, factor, ventana)`, `pausa()`,
`sigue()`, `paso(ns)` y `para()` en marcha, cada uno solo cuando vale, y
`pausada()`, que dice lo que dijo el último `T_ESTADO`. En la ventana, un
selector de ritmo antes de arrancar —tiempo real por omisión, a la mitad,
libre, a demanda— y en marcha **Pausa / Sigue**, **Paso** con sus
milisegundos y **Parar**. El botón de pausa dice lo que dijo el modelo, no lo
que se pidió, y Paso se apaga mientras avanza y vuelve cuando el modelo dice
que está otra vez en pausa.

### 14.3 Cómo se ha comprobado

**La prueba que pide el plan**, `make gui-control` (`verif/gui/control.py`, 29
comprobaciones), con el cliente en Python y sin Qt:

| Grupo | Qué |
| :--- | :--- |
| C1 | a tiempo real y sin ventana de tiempo: **pausa**, y durante 1,2 s llegan cinco `T_ESTADO`, todos `PAUSADA` y **en el mismo instante y con los mismos deltas**; tres `T_PING`, tres `T_PONG`; una suscripción nueva no da muestras; y **0,000 s de CPU**. **Sigue**: `T_ESTADO CORRIENDO` y 500 ms simulados en 0,50 s de pared, sin correr para recuperar la pausa. **Para**: `T_FIN` con `M_PARA`, el `T_ESTADO TERMINADA` delante, y `mcu-sim` sale con 0 dando su resumen |
| C2 | a demanda: en pausa en t = 0 y quieto medio segundo; pasos de 100, 250, 0, 50 + 50 y 150 ms que paran en **exactamente** 100, 350, 350, 450 y 600 ms; `T_SIGUE`, un aviso; `T_PARA` en pausa; y **las 59 muestras del LED hasta 600 ms, idénticas a las de una ejecución libre de un tirón** |
| C3 | la ventana de `T_ARRANCA` manda sobre la de la línea de órdenes (30 ms y no 5000); sin ninguna no hay fin; y si la ventana se va, `mcu-sim` se para solo |
| C4 | `--tiempo-real` en la línea de órdenes con ritmo libre en `T_ARRANCA`: va libre y lo dice; `T_PASO` a tiempo real y uno de 3 bytes, avisos |

**`testgui`**, G14 contra el canal en memoria (126 → 136, y `76450000000 ps`).
Como en pausa no corre ningún otro proceso —tampoco el del banco—, lo que la
ventana hace durante la pausa lo hace **un guion** que el canal ejecuta en
cada espera. Comprueba que en las esperas el tiempo y los deltas no se mueven
y que el banco, que esperaba 100 µs, no despierta hasta el `T_SIGUE`; el
`T_PONG` y el latido de pausa; los pasos exactos (250 µs, fuera de la
rejilla), sumados y de cero; `T_SIGUE` a demanda; y la ventana que se va en
pausa. T_PARA no se puede probar aquí: pararía el banco. **Seis mutaciones del
control**, una a una, y las seis caen.

**`test407` no cambia**, ni en picosegundos ni en recuento. Las otras tres
suites tampoco; `gui-proto` 64 (también bajo Wine), `gui-saludo` 22,
`gui-marcha` 24, `gui-ordenes` 23 e `interop` 47.

**En esta ventana**, con `ctest`: `sesion` 51 (G6, el control y cuándo vale cada
cosa), `ventana` 44 (G5, el selector de ritmo, Pausa / Sigue según el modelo,
Paso, Parar) y `cruzada` 21 contra el `mcu-sim` de verdad: a demanda y sin
ventana de tiempo, un paso de 100 ms que para en exactamente 100 ms y `T_PARA`
con su `M_PARA`.

### 14.4 Lo que cambió de las fases anteriores

* `marcha.py` M4 y M5 y `ordenes.py` O4 pedían `--tiempo-real` en la línea de
  órdenes y ritmo libre en `T_ARRANCA`: ahora el ritmo lo dice la ventana, y
  piden `RIT_REAL`.
* `testgui` G9 mandaba un `T_PAUSA` para ver que se ignoraba; ahora pararía el
  enlace, y lo sustituye un tipo desconocido.
* La ventana arranca a tiempo real, no libre.

### 14.5 Lo que NO se ha hecho, que también es la fase 6

Lanzar el modelo desde la ventana: fase 7. Y el ritmo no se cambia en marcha
—el protocolo no tiene mensaje para eso—: se elige al arrancar.

---

## 15. Fase 7, ejecutada

### 15.1 Qué se ha escrito

**En `mcu-sim`:**

* **`--argumentos`** (`top/sim_main.cpp`): vuelca en XML los dos posicionales y
  las quince opciones —forma, tipo, omisión, unidad, grupo, si es repetible,
  si con `--gui` tiene sentido, y una línea de ayuda—, con los 29 MCUs del
  catálogo como valores de `--mcu`. Es lo que el plan pedía para no tener la
  lista de opciones en dos sitios.
* Pero **dentro de `mcu-sim` sigue estando en dos**: la tabla que se vuelca y
  el bucle que lee `argv`. Reescribir el bucle a partir de la tabla era más
  riesgo que beneficio; lo que hay es un vigilante, **`make gui-argumentos`**
  (`verif/gui/argumentos.py`): toda opción que cite `--help` está en el volcado
  y viceversa, y cada una, con su omisión o su ejemplo, la acepta `mcu-sim` de
  verdad.
* Y al escribirlo apareció **I-26**, que llevaba en `doc/todo.md` desde antes
  de este proyecto: una opción desconocida se tomaba en silencio por el nombre
  del firmware. Con un diálogo donde se escriben argumentos a mano ya no era
  barato ignorarlo: ahora es «opcion desconocida» y código 1.
* `--ms` **no declara omisión a propósito**: sin él son 100 ms, pero con
  `--gui` es sin fin, y un diálogo que enseñase «100» mentiría justo en su caso.

**En `mcu-sim-gui`**, cuatro piezas nuevas:

* **`argumentos.h`** (capa `sesion`, sin pantalla): leer el volcado —saltando
  la cabecera de copyright de SystemC, que sale antes de que empiece el
  programa— y escribir la línea de órdenes a partir de los valores. **Una
  opción vacía no se escribe**, que no es lo mismo que escribir su omisión.
* **`configuracion.h`**: el JSON, con `QJsonDocument`. Los argumentos se
  guardan **por nombre**, sin lista: lo que alguien puso. Rutas relativas al
  fichero; al guardar, los comentarios de la plantilla sobreviven.
* **`lanzador.h`**: `mcu-sim` como `QProcess`, su salida línea a línea, cuándo
  termina y si se estrelló. Comprueba que el ejecutable exista —con o sin
  `.exe`— antes de intentarlo. Le quita al hijo la cabecera de SystemC
  (`SYSTEMC_DISABLE_COPYRIGHT_MESSAGE`), que sale por la salida de error y en
  la consola parecía un fallo en cada arranque.
* **`dialogo_lanzamiento.h`** (capa `pantalla`): se construye con la lista —una
  casilla por bandera, un desplegable por elección, un campo con la omisión de
  muestra para lo demás, las del mismo grupo se excluyen—, más «a mano» y la
  línea que va a salir. Si el ejecutable no contesta a `--argumentos`, lo dice
  y deja placa, firmware y «a mano».

Y en la ventana: escucha **primero**, en el puerto de la configuración o, si
está cogido, en otro, sin decir nada; *Simulación ▸ Lanzar mcu-sim* (Ctrl+L),
*Lanzar otra vez* (Ctrl+R) y *Detener*; una pestaña con la salida de
`mcu-sim`, la de error en rojo; y **cerrar con el modelo corriendo** le pide
parar —su resumen, su `T_FIN`— y, si en dos segundos no ha terminado, lo mata.
`main.cpp` gana `--config` y `--lanza`.

### 15.2 Cómo se ha comprobado

El plan dice que aquí **hace falta una persona**. Sigue haciendo falta: falta
abrir la ventana en una pantalla de verdad y verlo. Lo que se ha hecho es
automatizar todo lo que esa persona miraría, y la lista de fallos que el plan
manda probar a mano, con la `VentanaPrincipal` entera en la plataforma
`offscreen` contra el `mcu-sim` de verdad (`prueba_lanzamiento`, 18):

| Grupo | Qué |
| :--- | :--- |
| L1 | `mcu-sim --argumentos` se lee; el diálogo, abierto sin lista, se la pide y sale de ella |
| L2 | `lanza()` con la configuración: el hijo se conecta solo, la consola enseña la orden con su `--gui` y lo que dice `mcu-sim`; **a tiempo real, el LED cambia cada 100 ms simulados y cada 100 ms de pared** (medido: 100 100 100 100), con los dos relojes en pantalla. Con `CAPTURAS=dir`, deja dos capturas de la ventana —LED encendido y apagado— y una del diálogo |
| L3 | **cerrar la ventana con el modelo corriendo**: `T_PARA`, y `mcu-sim` termina con 0 dando su resumen, que llega a la consola |
| L4 | **matar `mcu-sim` desde fuera** (`kill -9`; en Windows, `taskkill /F`, que aquí no se ha podido ejecutar): la ventana lo ve, lo dice, apaga el control, y se puede volver a lanzar |
| L5 | **un puerto ocupado a propósito**: escucha en otro, y el hijo se conecta a ese |
| L6 | una placa que no existe: el hijo termina con 2 sin conectarse, y la ventana lo dice con su salida de error |

Y `prueba_argumentos` (34), sin `mcu-sim`: leer el volcado y lo que no se
puede leer, la línea de órdenes, la configuración —la plantilla versionada se
lee, se guarda y se relee igual—, el diálogo construido con una lista, el
puerto cogido y **una ruta de ejecutable mala**.

En `mcu-sim`: `gui-argumentos` 11; las cinco suites y su invariante sin
cambios —`test407` 2118 y `2240553274213 ps`—; `gui-saludo` 22, `gui-marcha`
24, `gui-ordenes` 23, `gui-control` 29, `gui-proto` 64 e `interop` 47.

### 15.3 Lo que queda para una persona

Abrir la ventana en una pantalla de verdad, `Ctrl+L`, `Lanzar`, `Arrancar`, y
**ver** el LED verde parpadear a su ritmo con los dos relojes avanzando. Y en
Windows, además: que el diálogo encuentra `mcu-sim.exe` sin escribir el
`.exe`, y que cerrar la ventana con el modelo corriendo no deja un `mcu-sim`
en el administrador de tareas.

### 15.4 Lo que NO se ha hecho, que también es la fase 7

Nada bonito: esta fase es que funcione. La grabación de sesiones es la fase 8.
