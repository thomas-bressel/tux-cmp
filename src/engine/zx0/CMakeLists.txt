# ZX0: the compressor, and a decompressor that does what the Z80's does.
add_library(tuxcmp_zx0 STATIC
    zx0.cpp
    zx0.h
)
target_link_libraries(tuxcmp_zx0 PUBLIC tuxcmp_base)
