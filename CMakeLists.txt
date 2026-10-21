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

# TuxAPE's sources: all the plugin needs of them is its plugin SDK, sdk/plugin.h.
set(TUXAPE_DIR "${CMAKE_SOURCE_DIR}/../TuxAPE" CACHE PATH "TuxAPE's source tree")
if(NOT EXISTS "${TUXAPE_DIR}/sdk/plugin.h")
    message(FATAL_ERROR "TuxAPE's plugin.h is not in ${TUXAPE_DIR}/sdk: set TUXAPE_DIR to TuxAPE's source tree.")
endif()

option(TUXCMP_BUILD_TESTS "Build the test programs" ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# What the machine is called in a plugin's plugin.json: as TuxAPE's
# PluginManager::platform() gives it.
string(TOLOWER "${CMAKE_SYSTEM_NAME}" TUXCMP_SYSTEM)
if(TUXCMP_SYSTEM STREQUAL "darwin")
    set(TUXCMP_SYSTEM "macos")
endif()
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" TUXCMP_PROCESSOR)
if(TUXCMP_PROCESSOR STREQUAL "amd64")
    set(TUXCMP_PROCESSOR "x86_64")
elseif(TUXCMP_PROCESSOR STREQUAL "aarch64")
    set(TUXCMP_PROCESSOR "arm64")
endif()
set(TUXCMP_PLATFORM "${TUXCMP_SYSTEM}-${TUXCMP_PROCESSOR}")

# The compressor and what the CPC is given to decompress with have no
# dependency on Qt: they are tested by themselves and are what the
# command-line program is made of.
add_subdirectory(src/base)
add_subdirectory(src/zx0)
add_subdirectory(src/cpc)
add_subdirectory(src/cli)
add_subdirectory(src/plugin)

if(TUXCMP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---- The archive users install: dist/tuxcmp-<version>-<platform>.zip ----
# It holds the plugin's folder, as TuxAPE's plugins folder does.
set(TUXCMP_STAGE "${CMAKE_BINARY_DIR}/stage")
set(TUXCMP_ARCHIVE "${CMAKE_SOURCE_DIR}/dist/tuxcmp-${PROJECT_VERSION}-${TUXCMP_PLATFORM}.zip")
configure_file(plugin.json.in "${TUXCMP_STAGE}/tuxcmp/plugin.json" @ONLY)
# The command-line program goes with it, in the folder's bin: TuxAPE makes
# what it unpacks there a program that can be run.
# Signed with its author's secret key when there is one (TUXAPE_SIGN_KEY,
# made by TuxAPE's tuxape-plugin-sign, which TUXAPE_SIGN_TOOL names): the
# signature goes in the folder, over its other files, before it is packed.
set(TUXAPE_SIGN_KEY "$ENV{HOME}/.config/tuxape-signing/tuxape.key" CACHE FILEPATH "The secret key the plugin is signed with")
set(TUXAPE_SIGN_TOOL "${TUXAPE_DIR}/build/src/sign/tuxape-plugin-sign" CACHE FILEPATH "TuxAPE's tuxape-plugin-sign")
if(EXISTS "${TUXAPE_SIGN_KEY}" AND EXISTS "${TUXAPE_SIGN_TOOL}")
    set(TUXCMP_SIGN "${TUXAPE_SIGN_TOOL}" sign "${TUXCMP_STAGE}/tuxcmp" "${TUXAPE_SIGN_KEY}")
else()
    message(STATUS "The archive is not signed: no secret key (TUXAPE_SIGN_KEY) or no tuxape-plugin-sign (TUXAPE_SIGN_TOOL)")
    set(TUXCMP_SIGN "${CMAKE_COMMAND}" -E rm -f "${TUXCMP_STAGE}/tuxcmp/signature.json")
endif()
add_custom_target(plugin_zip ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${TUXCMP_STAGE}/tuxcmp/${TUXCMP_PLATFORM}" "${TUXCMP_STAGE}/tuxcmp/bin"
            "${CMAKE_SOURCE_DIR}/dist"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:tuxcmp>" "${TUXCMP_STAGE}/tuxcmp/${TUXCMP_PLATFORM}/"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:tuxcmp_cli>" "${TUXCMP_STAGE}/tuxcmp/bin/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/LICENSE" "${TUXCMP_STAGE}/tuxcmp/"
    COMMAND ${TUXCMP_SIGN}
    COMMAND ${CMAKE_COMMAND} -E chdir "${TUXCMP_STAGE}"
            ${CMAKE_COMMAND} -E tar cf "${TUXCMP_ARCHIVE}" --format=zip tuxcmp
    DEPENDS tuxcmp tuxcmp_cli
    COMMENT "Packing ${TUXCMP_ARCHIVE}"
    VERBATIM)
