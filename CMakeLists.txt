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

qt_standard_project_setup()

# El transporte (fase 2): sin Widgets, para que las pruebas no necesiten
# pantalla. La ventana lo enlaza; las pruebas, tambien.
qt_add_library(transporte STATIC
    src/conexion.h
    src/conexion.cpp
    src/proto_io.h
    src/protocolo.h
)
target_include_directories(transporte PUBLIC src)
target_link_libraries(transporte PUBLIC Qt6::Core Qt6::Network)

qt_add_executable(mcu-sim-gui
    src/main.cpp
    src/ventana_principal.h
    src/ventana_principal.cpp
)

target_link_libraries(mcu-sim-gui PRIVATE transporte Qt6::Widgets)

# En Windows, sin consola detrás de la ventana.
set_target_properties(mcu-sim-gui PROPERTIES WIN32_EXECUTABLE ON)

install(TARGETS mcu-sim-gui
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION bin)

# --- Las pruebas -------------------------------------------------------------
# Sin ventana: QtCore y QtNetwork. `ctest --test-dir build` las ejecuta.
include(CTest)
if(BUILD_TESTING)
    qt_add_executable(prueba_conexion tests/prueba_conexion.cpp)
    target_link_libraries(prueba_conexion PRIVATE transporte)
    set_target_properties(prueba_conexion PROPERTIES WIN32_EXECUTABLE OFF MACOSX_BUNDLE OFF)
    add_test(NAME conexion COMMAND prueba_conexion)
endif()
