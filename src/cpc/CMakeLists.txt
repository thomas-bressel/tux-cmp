# What the CPC is given beside the compressed file: the AMSDOS header, the
# BASIC loader, the Z80 program that decompresses, and where all of them go
# in the machine's memory.
add_library(tuxcmp_cpc STATIC
    amsdos.cpp
    amsdos.h
    basic.cpp
    basic.h
    depacker.cpp
    depacker.h
    loader.cpp
    loader.h
)
target_link_libraries(tuxcmp_cpc PUBLIC tuxcmp_zx0 tuxcmp_base)
