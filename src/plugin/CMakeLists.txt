set(CMAKE_AUTOMOC ON)

# The windows are a library, so that tests can drive them; the plugin itself
# is no more than what TuxAPE loads and talks to.
add_library(tuxcmp_ui STATIC
    # The compressor's window, and the one that makes a loader.
    compressorwindow.cpp
    compressorwindow.h
    loaderdialog.cpp
    loaderdialog.h
    # Where the files go: a disc in a drive of TuxAPE, or TuxDSK's.
    discs.cpp
    discs.h
    # What they share.
    addresses.h
    prompts.cpp
    prompts.h
    preferences.cpp
    preferences.h
    icons.cpp
    icons.h
)
target_include_directories(tuxcmp_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${TUXAPE_SDK})
target_link_libraries(tuxcmp_ui PUBLIC tuxcmp_cpc tuxcmp_zx0 tuxcmp_base Qt6::Widgets)

# What TuxAPE loads: the plugin's class, and no more. tuxape_add_plugin()
# names it after the plugin's id.
add_library(tuxcmp MODULE
    tuxcmpplugin.cpp
    tuxcmpplugin.h
)
target_link_libraries(tuxcmp PRIVATE tuxcmp_ui)
