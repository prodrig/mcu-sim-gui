**mcu-sim-gui** — la contraparte gráfica de [mcu-sim](https://github.com/prodrig/mcu-sim): lo lanza, lo ve y lo toca.

Estos paquetes los ha construido la integración continua a partir del commit
etiquetado, y **cada uno se publica solo si en su plataforma pasaron las
pruebas y el paquete arrancó** —en Windows, con un `PATH` sin MSYS2, que es como
lo va a ejecutar quien lo descargue—. **Llevan Qt dentro**: no hay que
instalarlo.

| Tu máquina | Fichero | Qué es |
| :--- | :--- | :--- |
| Windows 10/11 x64 | `mcu-sim-gui-windows-x86_64.zip` | una carpeta: `mcu-sim-gui.exe` y sus DLL |
| Linux x86-64 (glibc 2.38 o posterior: Ubuntu 24.04, Debian 13, Fedora 39...) | `mcu-sim-gui-linux-x86_64.tar.gz` | un AppImage |
| Mac Apple Silicon | `mcu-sim-gui-macos-arm64.zip` | `mcu-sim-gui.app` |
| Mac Intel | `mcu-sim-gui-macos-x86_64.zip` | `mcu-sim-gui.app` |

**mcu-sim va aparte**, de sus propias Release: esta ventana lo lanza, pero no
lo lleva. Con los dos descargados, en la ventana *Simulación ▸ Lanzar mcu-sim*
(Ctrl+L) y en *Ejecutable* la ruta del `mcu-sim` (o `mcu-sim.exe`). La ventana
lo recuerda en su `config.json`; `config.ejemplo.json` es la plantilla.

**Cómo se arranca, y los avisos que van a salir:**

- **Windows**: descomprimir el `.zip` **entero** y ejecutar `mcu-sim-gui.exe`
  de dentro, sin sacarlo de su carpeta —las DLL de al lado son las suyas, y es
  justo eso lo que evita el «no se encuentra el punto de entrada» de otras
  copias de Qt en el `PATH`—. **SmartScreen avisa** porque no está firmado:
  *Más información* → *Ejecutar de todas formas*.
- **Linux**: `tar xzf mcu-sim-gui-linux-x86_64.tar.gz` y
  `./mcu-sim-gui-x86_64.AppImage`. Si dice que falta FUSE, o
  `sudo apt install fuse3`, o `./mcu-sim-gui-x86_64.AppImage
  --appimage-extract-and-run`.
- **macOS**: descomprimir, y antes de abrir la `.app`, quitarle la cuarentena
  —no está firmada por Apple—:
  `xattr -dr com.apple.quarantine mcu-sim-gui.app`. Sin Terminal: abrirla una
  vez, y luego *Ajustes → Privacidad y seguridad → Abrir de todos modos*. Nada
  de esto necesita `sudo`.

`SHA256SUMS.txt` va al lado para comprobar la descarga. Las licencias de lo
que llevan dentro —Qt, LGPLv3—, en `TERCEROS.md` y `licencias/`.
