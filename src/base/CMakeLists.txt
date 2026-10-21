# What the rest is built on: the version.
add_library(tuxcmp_base STATIC
    version.cpp
    version.h
)
target_include_directories(tuxcmp_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions(tuxcmp_base PRIVATE TUXCMP_VERSION="${PROJECT_VERSION}")
