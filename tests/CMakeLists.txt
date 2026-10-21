find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

# The tests of the engine's parts: every engine_zx0_*.cpp and
# engine_cpc_*.cpp of this folder is a program of its own, linked with its
# part and nothing else. What PackZX0 itself made of the same files is in
# tests/golden, for them to compare with.
foreach(part zx0 cpc)
    file(GLOB TUXCMP_PART_TESTS CONFIGURE_DEPENDS engine_${part}_*.cpp)
    foreach(source ${TUXCMP_PART_TESTS})
        get_filename_component(test ${source} NAME_WE)
        add_executable(${test} ${source})
        target_link_libraries(${test} PRIVATE tuxcmp_${part})
        target_compile_definitions(${test} PRIVATE TUXCMP_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
        add_test(NAME ${test} COMMAND ${test})
    endforeach()
endforeach()

# The command-line program, run as a user runs it.
add_executable(cli_tuxcmp cli_tuxcmp.cpp)
target_link_libraries(cli_tuxcmp PRIVATE tuxcmp_cpc)
target_compile_definitions(cli_tuxcmp PRIVATE
    TUXCMP_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
    TUXCMP_PROGRAM="$<TARGET_FILE:tuxcmp_cli>")
add_dependencies(cli_tuxcmp tuxcmp_cli)
add_test(NAME cli_tuxcmp COMMAND cli_tuxcmp)

# The plugin as TuxAPE loads it: found as a library, started with a host
# of the test's own, its window shown; in English, then in French.
add_executable(plugin_window plugin_window.cpp)
target_include_directories(plugin_window PRIVATE ${TUXAPE_SDK})
target_link_libraries(plugin_window PRIVATE Qt6::Widgets Qt6::Test)
add_dependencies(plugin_window tuxcmp)
target_compile_definitions(plugin_window PRIVATE
    TUXCMP_PLUGIN="$<TARGET_FILE:tuxcmp>"
    TUXCMP_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
add_test(NAME plugin_window COMMAND plugin_window)
set_tests_properties(plugin_window PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# The windows, driven as a user drives them, with a TuxAPE and a TuxDSK of
# the test's own to take the files.
add_executable(gui_compressor gui_compressor.cpp)
set_target_properties(gui_compressor PROPERTIES AUTOMOC ON)
target_link_libraries(gui_compressor PRIVATE tuxcmp_ui Qt6::Test)
target_compile_definitions(gui_compressor PRIVATE TUXCMP_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
add_test(NAME gui_compressor COMMAND gui_compressor)
set_tests_properties(gui_compressor PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# Every window test in English as TuxAPE runs the plugin: with English's
# plurals, which the plugin's build makes (tests/english.h).
foreach(test gui_compressor)
    target_compile_definitions(${test} PRIVATE
        TUXCMP_ENGLISH="$<FILTER:$<TARGET_PROPERTY:tuxcmp,TUXAPE_QM_FILES>,INCLUDE,_en[.]qm$>")
    add_dependencies(${test} tuxcmp)
endforeach()
