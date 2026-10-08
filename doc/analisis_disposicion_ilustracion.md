# La disposición de la ilustración: mover, girar y escalar placas, el lienzo, y guardarlo

*Análisis del 2026-10-08, antes de escribir nada: lo que se pide, cuánto
cuesta, dónde guardarlo y un plan por fases. Al final (§8), lo que se decidió
y lo hecho de cada fase.*

## 1. La pregunta

En `mcu-sim-gui`, que quien mira la ilustración de un sistema pueda:

1. **mover y girar** las placas dentro de la ilustración;
2. cambiar el **tamaño del lienzo**;
3. **escalar** las placas;
4. **guardar** lo que haya hecho, para que la próxima vez que se abra ese
   sistema salga igual. Se pregunta si el sitio es el propio XML de
   `mcu-sim`, a través del protocolo.

## 2. Lo que hay hoy

Todo pasa en `VistaPlaca` (`src/ilustracion.cpp`, unas 1.300 líneas):

* **Cada placa es una capa** (`Capa`), con todo lo suyo colgando de un
  `QGraphicsRectItem`, `raiz`: el fondo, los elementos vivos, los halos, los
  contornos, las etiquetas y la imagen de las pantallas.
* **La colocación es automática** (`coloca()`): las placas en el orden del
  sistema, una al lado de otra a 30 mm, centradas en vertical, a la misma
  escala —la de sus milímetros (§10 del análisis de ilustraciones)—. Solo
  hace `setPos` y `setScale` sobre cada `raiz`.
* **Las líneas** (`traza_lineas()`) se calculan desde las cajas en la escena
  de los conectores y los pines, y se rehacen enteras cada vez que algo se
  recoloca. Desde el plan §33 nacen escondidas.
* **El lienzo no existe como tal**: la escena es la caja de todas las placas
  más un margen, y la vista la encaja siempre entera (`encaja()`, un
  `fitInView` en cada cambio de tamaño). No hay zoom ni barras de
  desplazamiento.
* **El ratón ya está ocupado**: el clic, el arrastre y la rueda sobre una
  pieza son sus mandos —pulsar un botón, girar un encoder—, y la rueda
  atraviesa una pieza sin mando continuo (plan §29).
* **El giro ya existe, en `mcu-sim`**: `giro="90"` en una `<placa id>` del
  sistema gira el SVG antes de mandarlo, y la pantalla de una TFT gira con
  él. `T_PLACA` ya lo cuenta.
* **La ventana ya recuerda cosas** entre una vez y otra, en un JSON
  (`configuracion.h`: el ejecutable, los argumentos, el puerto, el ritmo...).
  Los dibujos abiertos a mano los recuerda solo durante la sesión
  (`dibujos_`, por nombre de placa).
* **La ventana no sabe dónde está el fichero del sistema** si no lanzó ella
  `mcu-sim`. Aunque lo lanzara, ese fichero puede estar en otra máquina: el
  modelo se puede conectar por red.

Lo bueno de todo esto: casi todo lo que se pide son **transformaciones de
`raiz`**, que es justo como está hecho. Halos, contornos, imágenes y elementos
vivos son hijos y van solos. Lo que no va solo se cuenta abajo.

## 3. Lo que cuesta cada cosa

| | Qué hace falta | Dónde | Tamaño |
| :--- | :--- | :--- | :--- |
| **Mover** | Arrastrar la `raiz` de una placa. Volver a trazar las líneas al soltar, y mientras se arrastra. La bandeja de la placa va con ella | GUI | medio |
| **Girar** | `setRotation` sobre la `raiz`, de 90 en 90 grados. Las etiquetas tienen que seguir derechas. Las líneas tienen que salir por el lado bueno de un conector que ahora es vertical | GUI | medio |
| **Escalar** | Un factor por placa sobre la escala de sus milímetros, en pasos (50 %, 75 %, 100 %, 150 %, 200 %...). Las etiquetas y el grosor de las líneas no deben crecer con la placa | GUI | pequeño |
| **Lienzo** | Que la escena tenga un tamaño en milímetros, en vez de ser la caja de las placas. Zoom y barras de desplazamiento, con «Ajustar a la ventana» para volver a lo de hoy | GUI | pequeño-medio |
| **Guardar en la ventana** | La disposición de un sistema en el JSON de la configuración. Se guarda sola y se aplica al abrir el mismo sistema | GUI | pequeño |
| **Leerla del XML** | Atributos nuevos en `<placa id>` y en `<sistema>`. `mcu-sim` los valida y los manda en `T_PLACA`, y la ventana los usa como disposición inicial | los dos | pequeño-medio |
| **Escribirla en el XML por el protocolo** | Un mensaje nuevo de la ventana al modelo, y que `mcu-sim` reescriba su propio fichero. Versión 3 del protocolo, y las pruebas de los dos lados | los dos | medio, y es lo más frágil |

Y lo que lo encarece todo un poco: **un modo de edición**. Si el arrastre es
para mover placas, no puede ser a la vez para pulsar botones. Hace falta un
interruptor —el botón «Edición» (§8)— que mientras está puesto apague los
mandos (`activa_mandos(false)` ya existe) y cambie lo que hacen el ratón y la
rueda.

**En conjunto, el esfuerzo es grande.** Del orden de 1.500 a 2.200 líneas
entre código, pruebas y documentación, en los dos repositorios. Es parecido a
la pantalla TFT o al servo, pero repartido en cuatro entregas, más una
opcional. Por eso va por fases (§6).

## 4. ¿Dónde se guarda? El XML, la configuración o las dos cosas

Hay tres sitios posibles, y cada uno sirve para una cosa distinta.

### 4.1 En el XML del sistema, escribiéndolo `mcu-sim` por el protocolo

Es lo que se apuntaba en la pregunta. Tiene a favor que la disposición viaja
con el sistema: se versiona en git, y quien abra
`placas/nucleo_f446re_servo.xml` lo ve como lo dejó su autor. En contra:

* **El modelo escribiría ficheros del usuario.** Hoy solo los lee. Habría que
  reescribir el XML a mano, como texto, para no perder los comentarios: es
  lo que hace `svg_variantes.h` con los SVG, así que se sabe hacer. Aun así
  es delicado.
* **Puede no ser el fichero que se cree.** `make datos` copia `placas/` a
  `build/`. Si se lanza desde allí, se edita la copia, y como queda más nueva
  que el original, `make` ya no la pisa: los dos divergen sin avisar.
  Dropbox, un fichero de solo lectura o un sistema de los ejemplos del
  repositorio son otros casos que hay que tratar.
* **Solo funciona con el modelo conectado**, y mientras dura la sesión.
* **Protocolo nuevo**: un mensaje de la ventana al modelo (`T_DISPOSICION`,
  0x8009) y su eco, la versión 3, `protocolo.h` y `proto_io.h` en los dos
  repositorios, y las suites `gui-proto`, `gui-saludo` e `interop`.

### 4.2 Solo en la configuración de la ventana

Es lo más barato y lo más robusto. No cambia el protocolo ni toca ficheros
del usuario, y vale aunque `mcu-sim` esté en otra máquina. Pero es personal:
no viaja con el sistema, y otra persona, u otro ordenador, lo ve como siempre.

### 4.3 La propuesta: el XML para leer, la configuración para guardar

Se combinan las dos cosas, cada una para lo suyo:

1. **El XML del sistema puede llevar la disposición**, escrita por su autor.
   Son atributos nuevos, todos opcionales:

   ```xml
   <sistema nombre="nucleo-f446re-servo" lienzo="260x180">
     <placa id="N" fichero="nucleo_f446re.xml" x="0" y="20"/>
     <placa id="S" fichero="servo_sg90.xml"    x="140" y="10" escala="1.5"/>
     <placa id="T" fichero="tft_128x160.xml"   x="200" y="90" giro="90"/>
   </sistema>
   ```

   `x` e `y` son los milímetros de la esquina de arriba a la izquierda de la
   placa ya girada. `escala` va sobre su tamaño real. `lienzo` es ancho x
   alto en milímetros, y sin él el lienzo es automático, como hoy. **El giro
   es el `giro=` que ya existe**: lo aplica `mcu-sim` al SVG y la ventana no
   tiene que saber nada. Es poco trabajo en `mcu-sim`: leer, validar y
   volcarlo en `T_PLACA`, como se hizo con `giro`. Sin protocolo nuevo, porque
   es añadido a la versión 2.
2. **La ventana guarda lo que se toque en su configuración**, sola, por
   sistema. La clave es el nombre del sistema y sus placas, con sus ficheros
   y su `giro`; si el sistema cambia, lo guardado no se aplica a ciegas.
3. **Al abrir, la que manda es la de la ventana**; si no hay, la del XML; y si
   tampoco, la automática. Un **«Restablecer»** vuelve a la del XML.
4. **Para llevarla al XML, «Copiar como XML»**: la ventana pone en el
   portapapeles las líneas `<placa id ... x= y= escala= giro=>` y el `lienzo=`,
   listas para pegar. Si la ventana lanzó ella `mcu-sim` y sabe la ruta, puede
   ofrecer además **escribirlas en el fichero**, con el mismo cuidado de
   tocar solo esos atributos y avisando si es una copia de `build/`.
5. **Escribir el XML por el protocolo** se deja para el final, como fase
   opcional (§6, fase 5). Solo merece la pena si de verdad hace falta guardar
   desde una ventana que no tiene el fichero a mano.

Con esto se consigue lo que se pide, que la próxima visualización salga como
se dejó, sin que el modelo escriba ficheros. Y un autor puede dejar sus
ejemplos bien colocados en el repositorio.

## 5. Decisiones de diseño que conviene fijar antes

* **Girar de 90 en 90 grados.** Es lo que admite `giro=`. Con esos giros la
  caja de cada elemento sigue siendo exacta (plan §31), y es como se monta
  una placa en una mesa. Un giro libre complica las líneas y el aviso de
  «girado», y no se ha pedido.
* **Cómo se gira en la ventana.** El giro se ve en el momento, en la propia
  vista, sin esperar a `mcu-sim`. Al guardarlo en el XML se suma al `giro=`
  que ya tuviera. Así la próxima vez llega el SVG ya girado y la ventana no
  gira nada.
* **El modo de edición**: el botón «Edición» de la ventana de la
  ilustración (§8). Dentro de ese modo:
  * se arrastra una placa por cualquier punto, y se ve su contorno;
  * R y Mayús+R giran la placa elegida;
  * Ctrl+rueda la escala;
  * el menú del botón derecho tiene «Girar», «Escala» y «Restablecer esta
    placa».

  Fuera de ese modo todo es como hoy. Dentro, los mandos están apagados.
* **Zoom fuera del modo.** Ctrl+rueda sobre el fondo hace zoom; la rueda sola
  sigue siendo de los mandos. Hay un botón «Ajustar», que es lo de hoy.
* **Ajuste a una rejilla de 1 mm** al soltar una placa, para que lo guardado
  sea legible en el XML.
* **Una placa suelta**, no un sistema, también se puede girar y escalar, y el
  lienzo vale igual. Mover una sola no tiene mucho sentido, pero no estorba.

## 6. El plan, por fases

Cada fase es una entrega con sus pruebas y su documentación, como hasta
ahora, y se puede usar sin las siguientes.

### Fase 1 — Mover y girar (`mcu-sim-gui`)

* El modo de edición, con el botón «Edición», y arrastrar una placa con su
  bandeja.
* Girar de 90 en 90 grados, con el teclado o el menú.
* Las líneas se vuelven a trazar al mover y al girar. Salen por el lado del
  conector que mira a la otra placa, también si el conector ha quedado en
  vertical (hoy solo miran izquierda o derecha).
* Las etiquetas siguen derechas: se contragiran.
* Al salir del modo, los mandos vuelven.
* Pruebas, en `prueba_ilustracion`, un grupo nuevo: arrastrar con el ratón
  simulado y ver la posición y el extremo de la línea; girar y ver los
  píxeles, la imagen de una pantalla que gira con la placa y una etiqueta que
  no gira; que en el modo un clic no manda una orden, y fuera sí.
* Unas 500 a 600 líneas.

### Fase 2 — Escalar y el lienzo (`mcu-sim-gui`)

* Una escala por placa, en pasos, con Ctrl+rueda o el menú.
* Un lienzo con tamaño, que se pide en un diálogo o con «Lienzo a la medida
  de las placas».
* Zoom, barras de desplazamiento y «Ajustar».
* `imagen()`, la de las pruebas, pinta el lienzo.
* Pruebas: la escala en la caja de la placa y en los píxeles, un lienzo más
  grande que las placas con su zoom, y que «Ajustar» deja lo de hoy.
* Unas 350 a 450 líneas.

### Fase 3 — Guardarla en la ventana (`mcu-sim-gui`)

* La disposición de cada sistema en el JSON de la configuración, guardada
  sola al salir del modo y aplicada al abrir.
* «Restablecer».
* Pruebas: ida y vuelta por un fichero de configuración temporal; un sistema
  que ha cambiado no recibe lo guardado; y una placa nueva en él se coloca
  sola.
* Unas 250 a 300 líneas.

### Fase 4 — La disposición en el XML del sistema (los dos)

* En `mcu-sim`:
  * `x`, `y` y `escala` en `<placa id>`, y `lienzo` en `<sistema>`, leídos y
    validados en `netlist_xml.h`, que hoy rechaza cualquier atributo que no
    conozca;
  * volcados en `T_PLACA`;
  * `doc/parts.md`, y `make gui-sistema` con un grupo nuevo;
  * de paso, una disposición cuidada para `placas/nucleo_f446re_servo.xml`.
* En `mcu-sim-gui`:
  * leerla de `T_PLACA` como disposición inicial;
  * «Copiar como XML»;
  * y, si la ventana lanzó `mcu-sim`, «Guardar en el XML» tocando solo esos
    atributos, con su aviso para las copias de `build/`.
* Unas 400 a 500 líneas entre los dos.

### Fase 5, opcional — Guardar en el XML por el protocolo (los dos)

* `T_DISPOSICION` de la ventana al modelo, y `mcu-sim` reescribe su fichero.
* La versión 3 del protocolo, y las suites de los dos lados.
* Solo si la fase 4 se queda corta: por ejemplo, con el modelo en otra
  máquina y el fichero allí.

## 7. Riesgos

* **El ratón.** Mezclar colocar placas con tocar mandos es la fuente de
  errores más probable; el modo separado lo evita. Hay que probar que la
  rueda sobre un encoder sigue girándolo fuera del modo.
* **Las pruebas de hoy** suponen la colocación automática y el
  `fitInView`. Lo automático tiene que seguir siendo exactamente lo de hoy
  cuando no hay nada guardado.
* **Lo guardado que ya no encaja**: un sistema editado, una placa quitada.
  Por eso la clave lleva las placas y sus ficheros, y lo que falte se coloca
  solo.
* **Las etiquetas y los grosores** con escalas distintas por placa: hoy se
  calculan con el tamaño de la placa entera.
* **Ningún invariante de `mcu-sim` se mueve en ninguna fase**: son atributos
  de dibujo que no construyen nada.

## 8. Lo que se decidió, y lo hecho

**Decidido el 2026-10-08:**

1. **Dónde guardar**: como propone §4.3, la configuración de la ventana y,
   más adelante, leerlo del XML. Sin escribir por el protocolo, de momento.
2. **Girar de 90 en 90 grados.**
3. **El orden de §6.**
4. **Un botón «Edición»** en la ventana de la ilustración, que se queda
   pulsado hasta que se vuelve a pulsar. Pulsado, es el modo de edición: el
   ratón coloca las placas —su posición, su escala y su giro— y los mandos
   están apagados. Al volver a pulsarlo, el botón queda como al principio,
   sin pulsar, se sale del modo y los mandos vuelven.

### Fase 1, hecha (plan §34 de `mcu-sim-gui`)

* **El botón «Edición»** (`edicion`), arriba en la ventana de la
  ilustración, con una línea de ayuda a su lado mientras está pulsado. Los
  mandos se apagan aunque el modelo corra (`mandos_activos()` es falso), y al
  salir vuelven como dijera el modelo.
* **Arrastrar una placa** la mueve, con su bandeja. Al soltarla se queda en
  milímetros enteros. La primera vez que se toca una, todas las demás se
  quedan donde están (`Ajuste::fija`). Las que no se han tocado nunca se
  colocan solas, a la derecha de las fijas.
* **Girar**: doble clic, 90 grados a la derecha; Mayús+rueda, a un lado o al
  otro; y el botón derecho, con «Girar 90 grados a la derecha», «a la
  izquierda», «Dejar que esta placa se coloque sola» y «Colocar todas como al
  principio». El giro es sobre el centro de la placa.
* **Lo que va con la placa**: sus piezas, sus halos, la imagen de su pantalla
  y su bandeja. **Las etiquetas** siguen derechas y centradas bajo su pieza,
  también con la placa girada. **Las líneas** salen por el lado que mira a la
  otra placa: izquierda o derecha, o arriba o abajo si la otra está más
  encima que de lado.
* **Mientras se arrastra**, la vista no se reencaja: la placa no da saltos
  bajo el ratón. Se reencaja al soltar.
* **Sin tocar nada, todo es como antes**: la colocación automática y la
  escena de una placa suelta son exactamente las de siempre.
* **Cómo se comprueba**: `prueba_ilustracion` I15 (195 → 210).

### Fase 2, hecha (plan §35 de `mcu-sim-gui`)

* **La escala de cada placa** (`Ajuste::escala`), sobre su tamaño real y
  sobre su centro.
  * **Cómo se cambia, en la edición:** la rueda sola sube o baja un paso;
    Mayús+rueda sigue siendo girar. El botón derecho tiene «Mas grande»,
    «Mas pequena» y «Tamano real», que dice la de ahora.
  * **Los pasos:** 25, 33, 50, 67, 75, 100, 125, 150, 200, 300 y 400 %.
  * **Lo que no crece:** las etiquetas de la placa, ni el grosor de las
    líneas.
* **El lienzo.** «Lienzo...», en la edición, abre un diálogo:
  * por omisión es «A la medida de las placas», como siempre;
  * si no, se le da un ancho y un alto en milímetros, con un botón que los
    pone a la medida de las placas más 10 mm por lado;
  * un lienzo nuevo se centra en las placas, y uno que ya era fijo conserva
    su esquina.

  En la vista, el lienzo fijo es una hoja blanca sobre fondo gris, y la
  escena es el lienzo más lo que se salga de él.
* **El zoom.** Ctrl+rueda acerca o aleja un 25 % cada muesca, en la edición y
  fuera de ella, también sobre un mando, que no se mueve. Con el zoom salen
  las barras de desplazamiento y la vista deja de seguir el tamaño de la
  ventana. «Ajustar» vuelve a enseñar todo el lienzo.
* **Cómo se comprueba**: `prueba_ilustracion` I16 (210 → 229).
