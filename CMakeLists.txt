# =============================================================================
# mcu-sim-gui — la contraparte de visualización de mcu-sim
#
# Qt 6 y CMake. Nada más: ni SystemC, ni una cabecera de mcu-sim. Lo único que
# los dos programas comparten es `src/protocolo.h` y `src/proto_io.h`, y lo
# comparten por copia —`make gui-proto` en mcu-sim exige que las dos copias
# sean idénticas byte a byte— y no por inclusión cruzada de árboles.
#
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#   ctest --test-dir build --output-on-failure      # las pruebas, sin ventana
#
# Si CMake no encuentra Qt, se le dice dónde está:
#   cmake -B build -DCMAKE_PREFIX_PATH=/ruta/a/Qt/6.7.0/gcc_64
# =============================================================================
cmake_minimum_required(VERSION 3.19)

project(mcu-sim-gui
        VERSION 0.1.0
        DESCRIPTION "Contraparte de visualizacion grafica de mcu-sim"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dos avisos que ahorran una tarde ---------------------------------------
#
# 1. COMPILAR DENTRO DE UNA CARPETA SINCRONIZADA (Dropbox, OneDrive, Drive) es
#    mala idea: se sincronizan miles de ficheros objeto que no le importan a
#    nadie, y el cliente de sincronizacion abre cada fichero recien creado, lo
#    que puede dar bloqueos esporadicos. No es fatal -hay quien compila ahi
#    todos los dias- pero no cuesta nada evitarlo:
#
#      cmake -G Ninja -B C:/build/mcu-sim-gui -S .
#
# 2. Y SI EL SINTOMA ES "Permission denied" SOBRE UN .exe RECIEN CREADO, o un
#    ejecutable que aparece y luego DESAPARECE, no es la carpeta: es el
#    ANTIVIRUS poniendo en cuarentena binarios recien compilados, que es
#    exactamente la pinta que tiene lo que un antivirus heuristico busca. Un
#    compilador produce ficheros nuevos, sin firmar y desconocidos. El README
#    lo cuenta, con las exclusiones concretas.
#
#    Eso paso de verdad en este proyecto, con ESET, y costo dos diagnosticos
#    equivocados -este aviso era el primero de ellos-. Por eso sigue aqui, pero
#    diciendo lo que es: una molestia, no la causa de esa clase de error.
foreach(_carpeta_nube "Dropbox" "OneDrive" "Google Drive" "GoogleDrive" "iCloud")
    string(FIND "${CMAKE_BINARY_DIR}" "${_carpeta_nube}" _pos_nube)
    if(NOT _pos_nube EQUAL -1)
        message(WARNING
            "El arbol de compilacion esta dentro de una carpeta de ${_carpeta_nube}:\n"
            "  ${CMAKE_BINARY_DIR}\n"
            "Se van a sincronizar miles de ficheros objeto, y el cliente puede "
            "bloquear ficheros recien creados. Mejor fuera:\n"
            "  cmake -G Ninja -B C:/build/mcu-sim-gui -S .\n"
            "Y si ves 'Permission denied' sobre un .exe recien creado, o un "
            "ejecutable que desaparece, eso NO es esto: es el antivirus. "
            "Vease el apartado de Windows del README.")
    endif()
endforeach()

find_package(Qt6 6.3 REQUIRED COMPONENTS Core Widgets Network)

# La version que dice `mcu-sim-gui --version`. La de `project()` con
# «-desarrollo» detras, salvo que alguien diga otra: la integracion continua,
# al construir una etiqueta `v1.2.3`, pasa -DMCU_SIM_GUI_VERSION=1.2.3. Asi un
# ejecutable de una Release dice de cual sale, y uno compilado a mano no se
# hace pasar por ella.
set(MCU_SIM_GUI_VERSION "${PROJECT_VERSION}-desarrollo" CACHE STRING
    "Version que anuncia el ejecutable")

qt_standard_project_setup()

# Tres capas, de abajo arriba, y solo la ultima necesita pantalla:
#
#   transporte   mover bytes: el marco y el socket (fase 2)
#   sesion       el significado: el saludo y los dos XML de la placa (fase 3);
#                y desde la fase 7, lanzar mcu-sim, su lista de opciones y la
#                configuracion
#   pantalla     construir los widgets a partir de esos XML (fase 3), y el
#                dialogo de lanzamiento (fase 7)
#
# Las pruebas enlazan la capa que prueban, y las dos primeras corren sin
# pantalla ninguna.
qt_add_library(transporte STATIC
    src/conexion.h
    src/conexion.cpp
    src/proto_io.h
    src/protocolo.h
)
target_include_directories(transporte PUBLIC src)
target_link_libraries(transporte PUBLIC Qt6::Core Qt6::Network)

qt_add_library(sesion STATIC
    src/placa.h
    src/placa.cpp
    src/sesion.h
    src/sesion.cpp
    src/argumentos.h
    src/argumentos.cpp
    src/configuracion.h
    src/configuracion.cpp
    src/lanzador.h
    src/lanzador.cpp
)
target_link_libraries(sesion PUBLIC transporte)

qt_add_library(pantalla STATIC
    src/panel.h
    src/panel.cpp
    src/ventana_principal.h
    src/ventana_principal.cpp
    src/dialogo_lanzamiento.h
    src/dialogo_lanzamiento.cpp
)
target_link_libraries(pantalla PUBLIC sesion Qt6::Widgets)

qt_add_executable(mcu-sim-gui
    src/main.cpp
)

target_link_libraries(mcu-sim-gui PRIVATE pantalla)
target_compile_definitions(mcu-sim-gui PRIVATE
    MCU_SIM_GUI_VERSION="${MCU_SIM_GUI_VERSION}")

# En Windows, sin consola detrás de la ventana.
set_target_properties(mcu-sim-gui PROPERTIES WIN32_EXECUTABLE ON)

install(TARGETS mcu-sim-gui
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION bin)

# --- Las pruebas -------------------------------------------------------------
# Sin ventana: QtCore y QtNetwork. `ctest --test-dir build` las ejecuta.
# `cruzada` necesita un mcu-sim de verdad: MCU_SIM=ejecutable y
# MCU_SIM_SRC=su directorio src/. Sin ellas se salta (codigo 77).
include(CTest)
if(BUILD_TESTING)
    function(prueba nombre fuente capa)
        qt_add_executable(${nombre} ${fuente} tests/comun.h)
        target_link_libraries(${nombre} PRIVATE ${capa})
        target_include_directories(${nombre} PRIVATE tests)
        set_target_properties(${nombre} PROPERTIES WIN32_EXECUTABLE OFF MACOSX_BUNDLE OFF)
    endfunction()

    prueba(prueba_conexion tests/prueba_conexion.cpp transporte)
    prueba(prueba_sesion   tests/prueba_sesion.cpp   sesion)
    prueba(prueba_ventana  tests/prueba_ventana.cpp  pantalla)
    prueba(prueba_cruzada  tests/prueba_cruzada.cpp  sesion)
    prueba(prueba_argumentos  tests/prueba_argumentos.cpp  pantalla)
    prueba(prueba_lanzamiento tests/prueba_lanzamiento.cpp pantalla)
    # La plantilla de configuracion se lee de las fuentes
    target_compile_definitions(prueba_argumentos PRIVATE FUENTES="${CMAKE_SOURCE_DIR}")

    add_test(NAME conexion COMMAND prueba_conexion)
    add_test(NAME sesion   COMMAND prueba_sesion)
    add_test(NAME ventana  COMMAND prueba_ventana)
    set_tests_properties(ventana PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME cruzada  COMMAND prueba_cruzada)
    set_tests_properties(cruzada PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME argumentos COMMAND prueba_argumentos)
    set_tests_properties(argumentos PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    # Como `cruzada`: con MCU_SIM y MCU_SIM_SRC, contra el mcu-sim de verdad
    add_test(NAME lanzamiento COMMAND prueba_lanzamiento)
    set_tests_properties(lanzamiento PROPERTIES SKIP_RETURN_CODE 77
                         ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
