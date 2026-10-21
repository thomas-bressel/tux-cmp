# TuxCMP: PackZX0's ZX0 compressor for the Amstrad CPC, as a TuxAPE plugin
# (see PLUGINS.md at TuxAPE's root).
cmake_minimum_required(VERSION 3.21)

project(TuxCMP
    VERSION 0.1.0
    DESCRIPTION "ZX0 compressor for the Amstrad CPC: a plugin for TuxAPE, after PackZX0"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# The engine goes into the plugin, a shared library.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Qt, for the whole project: TuxAPE's, or an earlier Qt 6 (checked by
# tuxape_add_plugin).
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

# TuxAPE's plugin SDK: plugin.h and TuxAPEPlugin.cmake, all the plugin
# needs of TuxAPE.
set(TUXAPE_SDK "${CMAKE_SOURCE_DIR}/../TuxAPE/sdk" CACHE PATH "TuxAPE's sdk folder")
if(NOT EXISTS "${TUXAPE_SDK}/plugin.h" OR NOT EXISTS "${TUXAPE_SDK}/TuxAPEPlugin.cmake")
    message(FATAL_ERROR "TuxAPE's SDK is not in ${TUXAPE_SDK}: set TUXAPE_SDK to TuxAPE's sdk folder.")
endif()
include("${TUXAPE_SDK}/TuxAPEPlugin.cmake")

option(TUXCMP_BUILD_TESTS "Build the test programs" ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# The compressor and what the CPC is given to decompress with, without Qt:
# tested by themselves, and what the command-line program is made of.
add_subdirectory(src/engine)
# tuxcmp: PackZX0's command line.
add_subdirectory(src/cli)
# What TuxAPE loads: the windows, and the plugin's class.
add_subdirectory(src/plugin)

# The archive users install: dist/tuxcmp-<version>-<platform>.zip, with the
# command-line program in its bin folder and the plugin's translations
# built into its library; signed when the author's secret key is named
# (TUXAPE_SIGN_KEY).
tuxape_add_plugin(tuxcmp
    ID tuxcmp
    NAME "TuxCMP – ZX0 Compressor"
    VERSION ${PROJECT_VERSION}
    AUTHOR "TuxAPE"
    LICENCE "BSD-3-Clause"
    DESCRIPTION "Compresses any file with ZX0 (Einar Saukas), as PackZX0 by Ludovic Deplanque (Demoniak) does, puts it on the disc in a drive of TuxAPE or on TuxDSK's, and writes beside it a BASIC loader and the Z80 program that decompresses it."
    API 4
    FILES LICENSE
    PROGRAMS tuxcmp_cli
    TRANSLATIONS translations/tuxcmp_fr.ts
    TRANSLATE_TARGETS tuxcmp_ui)

if(TUXCMP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
