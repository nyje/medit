SET(plugins_sources
    plugins/plugins.cmake
    plugins/moofileselector-prefs.cpp
    plugins/moofileselector.cpp
    plugins/moofileselector.h
    plugins/mooplugin-builtin.h
    plugins/mooplugin-builtin.cpp
    plugins/moofilelist.cpp
    plugins/moofind.cpp
)

foreach(input_file
	plugins/glade/moofileselector-prefs.glade
	plugins/glade/moofileselector.glade
	plugins/glade/moofind.glade
	plugins/glade/moogrep.glade
)
    ADD_GXML(${input_file})
endforeach(input_file)

# zzz include plugins/ctags/Makefile.incl
include(${CMAKE_CURRENT_SOURCE_DIR}/plugins/usertools/usertools.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/plugins/support/support.cmake)
