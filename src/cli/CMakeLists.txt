# tuxcmp: PackZX0's command line, without Qt.
add_executable(tuxcmp_cli main.cpp)
set_target_properties(tuxcmp_cli PROPERTIES OUTPUT_NAME tuxcmp)
target_link_libraries(tuxcmp_cli PRIVATE tuxcmp_cpc tuxcmp_zx0 tuxcmp_base)
