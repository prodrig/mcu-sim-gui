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
