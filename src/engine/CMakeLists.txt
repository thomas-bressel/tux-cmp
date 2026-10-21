# What the plugin does, without Qt nor TuxAPE: plain C++, in three parts,
# each tested alone.
#   base   the version
#   zx0    the compressor, and a decompressor that does what the Z80's does
#   cpc    what the CPC is given beside the compressed file: the AMSDOS
#          header, the BASIC loader, the Z80 program that decompresses
add_subdirectory(base)
add_subdirectory(zx0)
add_subdirectory(cpc)
