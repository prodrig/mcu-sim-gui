# =============================================================================
# mcu-sim-gui — la contraparte de visualización de mcu-sim
#
# Qt 6 y CMake. Nada más: ni SystemC, ni una cabecera de mcu-sim. Lo único que
# los dos programas comparten es `src/protocolo.h`, y lo comparten por copia
# —hay una comprobación en la suite de mcu-sim que exige que las dos sean
# idénticas byte a byte— y no por inclusión cruzada de árboles.
#
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
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

# --- Un aviso que ahorra una tarde ------------------------------------------
# Compilar DENTRO de una carpeta sincronizada (Dropbox, OneDrive, Drive) falla
# de una forma que no se parece en nada a su causa: el cliente de sincronizacion
# -y el antivirus detras- abren cada `.exe` recien creado para subirlo, y
# mientras lo tienen abierto CMake no puede leerlo ni borrarlo. El sintoma es
# "Permission denied" sobre `CompilerIdCXX/a.exe` o sobre un `cmTC_*.exe`, y
# como la deteccion del compilador REINTENTA, a veces acaba diciendo "works"
# despues de haber fallado. Un fallo intermitente que ademas se contradice.
#
# La solucion no es apagar nada: es sacar el arbol de compilacion de ahi. Y de
# paso se deja de sincronizar un directorio con miles de ficheros objeto.
#
#   cmake -G Ninja -B C:/build/mcu-sim-gui -S .
#
# Esto no lo impide -hay quien tiene el antivirus configurado y le funciona-
# pero lo dice antes de que el error aparezca y confunda.
foreach(_carpeta_nube "Dropbox" "OneDrive" "Google Drive" "GoogleDrive" "iCloud")
    string(FIND "${CMAKE_BINARY_DIR}" "${_carpeta_nube}" _pos_nube)
    if(NOT _pos_nube EQUAL -1)
        message(WARNING
            "El arbol de compilacion esta dentro de una carpeta de ${_carpeta_nube}:\n"
            "  ${CMAKE_BINARY_DIR}\n"
            "El cliente de sincronizacion abre cada .exe recien creado y CMake no "
            "puede leerlo ni borrarlo: sale 'Permission denied' sobre a.exe o "
            "cmTC_*.exe. Compila fuera de la carpeta sincronizada:\n"
            "  cmake -G Ninja -B C:/build/mcu-sim-gui -S .")
    endif()
endforeach()

find_package(Qt6 6.3 REQUIRED COMPONENTS Widgets Network)

qt_standard_project_setup()

qt_add_executable(mcu-sim-gui
    src/main.cpp
    src/ventana_principal.h
    src/ventana_principal.cpp
    src/protocolo.h
)

target_link_libraries(mcu-sim-gui PRIVATE Qt6::Widgets Qt6::Network)

# En Windows, sin consola detrás de la ventana.
set_target_properties(mcu-sim-gui PROPERTIES WIN32_EXECUTABLE ON)

install(TARGETS mcu-sim-gui
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION bin)
