SET(docparser_files 
    parsedocs.py
    mdp/__init__.py
    mdp/module.py
    mdp/docparser.py
    mdp/xmlwriter.py
)

SET(gendefs_files
    gendefs.py
    mpi/__init__.py
    mpi/module.py
    mpi/defswriter.py
)

SET(genlua_files
    genlua.py
    mpi/__init__.py
    mpi/module.py
    mpi/luawriter.py
)

SET(gendocs_files
    gendocs.py
    gendocbook.py
    mpi/__init__.py
    mpi/module.py
    mpi/docbookwriter.py
)

SET(source_files
    ../moo/moolua/medit-lua.h
    ../moo/moolua/medit-lua.cpp
    ../moo/mooapp/mooappabout.c
    ../moo/mooapp/mooappabout.h
    ../moo/mooapp/mooapp-accels.h
    ../moo/mooapp/mooapp.c
    ../moo/mooapp/mooapp.h
    ../moo/mooapp/mooapp-info.h
    ../moo/mooapp/mooapp-private.h
    ../moo/mooapp/moohtml.c
    ../moo/mooapp/moohtml.h
    ../moo/mooapp/moolinklabel.c
    ../moo/mooapp/moolinklabel.h
    ../moo/mooedit/mooedit-accels.h
    ../moo/mooedit/mooeditaction.c
    ../moo/mooedit/mooeditaction-factory.c
    ../moo/mooedit/mooeditaction-factory.h
    ../moo/mooedit/mooeditaction.h
    ../moo/mooedit/mooeditbookmark.c
    ../moo/mooedit/mooeditbookmark.h
    ../moo/mooedit/mooedit.cpp
    ../moo/mooedit/mooeditconfig.c
    ../moo/mooedit/mooeditconfig.h
    ../moo/mooedit/mooeditdialogs.cpp
    ../moo/mooedit/mooeditdialogs.h
    ../moo/mooedit/mooedit-enum-types.c
    ../moo/mooedit/mooedit-enum-types.h
    ../moo/mooedit/mooedit-enums.h
    ../moo/mooedit/mooeditfileinfo.c
    ../moo/mooedit/mooeditfileinfo.c
    ../moo/mooedit/mooeditfileinfo.h
    ../moo/mooedit/mooedit-fileops.cpp
    ../moo/mooedit/mooedit-fileops.h
    ../moo/mooedit/mooeditfiltersettings.c
    ../moo/mooedit/mooeditfiltersettings.h
    ../moo/mooedit/mooedit.h
    ../moo/mooedit/mooedithistoryitem.c
    ../moo/mooedit/mooedithistoryitem.h
    ../moo/mooedit/mooedit-impl.h
    ../moo/mooedit/mooeditor.cpp
    ../moo/mooedit/mooeditor.h
    ../moo/mooedit/mooeditor-impl.h
    ../moo/mooedit/mooeditor-private.h
    ../moo/mooedit/mooeditor-tests.c
    ../moo/mooedit/mooeditor-tests.h
    ../moo/mooedit/mooeditprefs.c
    ../moo/mooedit/mooeditprefs.h
    ../moo/mooedit/mooeditprefspage.c
    ../moo/mooedit/mooedit-private.h
    ../moo/mooedit/mooedit-script.c
    ../moo/mooedit/mooedit-script.h
    ../moo/mooedit/mooedittab.c
    ../moo/mooedit/mooedittab.h
    ../moo/mooedit/mooedittypes.h
    ../moo/mooedit/mooeditview.c
    ../moo/mooedit/mooeditview.h
    ../moo/mooedit/mooeditview-script.c
    ../moo/mooedit/mooeditview-script.h
    ../moo/mooedit/mooeditwindow.c
    ../moo/mooedit/mooeditwindow.h
    ../moo/mooedit/mooeditwindow-impl.h
    ../moo/mooedit/moofold.c
    ../moo/mooedit/moofold.h
    ../moo/mooedit/mooindenter.c
    ../moo/mooedit/mooindenter.h
    ../moo/mooedit/moolang.c
    ../moo/mooedit/moolang.h
    ../moo/mooedit/moolangmgr.c
    ../moo/mooedit/moolangmgr.h
    ../moo/mooedit/moolangmgr-private.h
    ../moo/mooedit/moolang-private.h
    ../moo/mooedit/moolinebuffer.c
    ../moo/mooedit/moolinebuffer.h
    ../moo/mooedit/moolinemark.c
    ../moo/mooedit/moolinemark.h
    ../moo/mooedit/mooplugin.c
    ../moo/mooedit/mooplugin.h
    ../moo/mooedit/mooplugin-loader.c
    ../moo/mooedit/mooplugin-loader.h
    ../moo/mooedit/mooplugin-macro.h
    ../moo/mooedit/mootextbtree.c
    ../moo/mooedit/mootextbtree.h
    ../moo/mooedit/mootextbuffer.c
    ../moo/mooedit/mootextfind.c
    ../moo/mooedit/mootextfind.h
    ../moo/mooedit/mootextiter.h
    ../moo/mooedit/mootextprint.c
    ../moo/mooedit/mootextprint.h
    ../moo/mooedit/mootextprint-private.h
    ../moo/mooedit/mootext-private.h
    ../moo/mooedit/mootextsearch.c
    ../moo/mooedit/mootextsearch.h
    ../moo/mooedit/mootextsearch-private.h
    ../moo/mooedit/mootextstylescheme.c
    ../moo/mooedit/mootextstylescheme.h
    ../moo/mooedit/mootextview.c
    ../moo/mooedit/mootextview.h
    ../moo/mooedit/mootextview-input.c
    ../moo/mooedit/mootextview-private.h
    ../moo/moofileview/moobookmarkmgr.c
    ../moo/moofileview/moobookmarkmgr.h
    ../moo/moofileview/moobookmarkview.c
    ../moo/moofileview/moobookmarkview.h
    ../moo/moofileview/moofile.c
    ../moo/moofileview/moofileentry.c
    ../moo/moofileview/moofileentry.h
    ../moo/moofileview/moofile.h
    ../moo/moofileview/moofile-private.h
    ../moo/moofileview/moofilesystem.c
    ../moo/moofileview/moofilesystem.h
    ../moo/moofileview/moofileview-accels.h
    ../moo/moofileview/moofileview-aux.h
    ../moo/moofileview/moofileview.c
    ../moo/moofileview/moofileview-dialogs.c
    ../moo/moofileview/moofileview-dialogs.h
    ../moo/moofileview/moofileview.h
    ../moo/moofileview/moofileview-impl.h
    ../moo/moofileview/moofileview-private.h
    ../moo/moofileview/moofileview-tools.c
    ../moo/moofileview/moofileview-tools.h
    ../moo/moofileview/moofolder.c
    ../moo/moofileview/moofolder.h
    ../moo/moofileview/moofoldermodel.c
    ../moo/moofileview/moofoldermodel.h
    ../moo/moofileview/moofoldermodel-private.h
    ../moo/moofileview/moofolder-private.h
    ../moo/moofileview/mooiconview.c
    ../moo/moofileview/mooiconview.h
    ../moo/moofileview/mootreeview.c
    ../moo/moofileview/mootreeview.h
    ../moo/mooutils/mooaccelbutton.c
    ../moo/mooutils/mooaccelbutton.h
    ../moo/mooutils/mooaccel.c
    ../moo/mooutils/mooaccel.h
    ../moo/mooutils/mooaccelprefs.c
    ../moo/mooutils/mooaccelprefs.h
    ../moo/mooutils/mooactionbase.c
    ../moo/mooutils/mooactionbase.h
    ../moo/mooutils/mooactionbase-private.h
    ../moo/mooutils/mooaction.c
    ../moo/mooutils/mooactioncollection.c
    ../moo/mooutils/mooactioncollection.h
    ../moo/mooutils/mooactionfactory.c
    ../moo/mooutils/mooactionfactory.h
    ../moo/mooutils/mooactiongroup.c
    ../moo/mooutils/mooactiongroup.h
    ../moo/mooutils/mooaction.h
    ../moo/mooutils/mooaction-private.h
    ../moo/mooutils/mooappinput-common.c
    ../moo/mooutils/mooappinput.h
    ../moo/mooutils/mooapp-ipc.c
    ../moo/mooutils/mooapp-ipc.h
    ../moo/mooutils/mooarray.h
    ../moo/mooutils/mooatom.h
    ../moo/mooutils/moobigpaned.c
    ../moo/mooutils/moobigpaned.h
    ../moo/mooutils/mooclosure.c
    ../moo/mooutils/mooclosure.h
    ../moo/mooutils/moocombo.c
    ../moo/mooutils/moocombo.h
    ../moo/mooutils/moocompat.h
    ../moo/mooutils/moodialogs.c
    ../moo/mooutils/moodialogs.h
    ../moo/mooutils/mooeditops.c
    ../moo/mooutils/mooeditops.h
    ../moo/mooutils/mooencodings.c
    ../moo/mooutils/mooencodings-data.h
    ../moo/mooutils/mooencodings.h
    ../moo/mooutils/mooentry.cpp
    ../moo/mooutils/mooentry.h
    ../moo/mooutils/moo-environ.h
    ../moo/mooutils/moofiledialog.c
    ../moo/mooutils/moofiledialog.h
    ../moo/mooutils/moofileicon.c
    ../moo/mooutils/moofileicon.h
    ../moo/mooutils/moofilewatch.c
    ../moo/mooutils/moofilewatch.h
    ../moo/mooutils/moofilewriter.c
    ../moo/mooutils/moofilewriter.h
    ../moo/mooutils/moofilewriter-private.h
    ../moo/mooutils/moofiltermgr.c
    ../moo/mooutils/moofiltermgr.h
    ../moo/mooutils/moofontsel.c
    ../moo/mooutils/moofontsel.h
    ../moo/mooutils/mooglade.c
    ../moo/mooutils/mooglade.h
    ../moo/mooutils/moohelp.c
    ../moo/mooutils/moohelp.h
    ../moo/mooutils/moohistorycombo.c
    ../moo/mooutils/moohistorycombo.h
    ../moo/mooutils/moohistorylist.c
    ../moo/mooutils/moohistorylist.h
    ../moo/mooutils/moohistorymgr.c
    ../moo/mooutils/moohistorymgr.h
    ../moo/mooutils/mooi18n.c
    ../moo/mooutils/mooi18n.h
    ../moo/mooutils/moolist.h
    ../moo/mooutils/moomarkup.c
    ../moo/mooutils/moomarkup.h
    ../moo/mooutils/moomenuaction.c
    ../moo/mooutils/moomenuaction.h
    ../moo/mooutils/moomenu.c
    ../moo/mooutils/moomenu.h
    ../moo/mooutils/moomenumgr.c
    ../moo/mooutils/moomenumgr.h
    ../moo/mooutils/moomenutoolbutton.c
    ../moo/mooutils/moomenutoolbutton.h
    ../moo/mooutils/moo-mime.c
    ../moo/mooutils/moo-mime.h
    ../moo/mooutils/moonotebook.c
    ../moo/mooutils/moonotebook.h
    ../moo/mooutils/mooonce.h
    ../moo/mooutils/moopane.c
    ../moo/mooutils/moopaned.c
    ../moo/mooutils/moopaned.h
    ../moo/mooutils/moopane.h
    ../moo/mooutils/mooprefs.c
    ../moo/mooutils/mooprefsdialog.c
    ../moo/mooutils/mooprefsdialog.h
    ../moo/mooutils/mooprefs.h
    ../moo/mooutils/mooprefspage.c
    ../moo/mooutils/mooprefspage.h
    ../moo/mooutils/moospawn.c
    ../moo/mooutils/moospawn.h
    ../moo/mooutils/moostock.c
    ../moo/mooutils/moostock.h
    ../moo/mooutils/moo-test-macros.h
    ../moo/mooutils/moo-test-utils.c
    ../moo/mooutils/moo-test-utils.h
    ../moo/mooutils/mootypedecl-macros.h
    ../moo/mooutils/mootype-macros.h
    ../moo/mooutils/moouixml.c
    ../moo/mooutils/moouixml.h
    ../moo/mooutils/mooundo.c
    ../moo/mooutils/mooundo.h
    ../moo/mooutils/mooutils-debug.h
    ../moo/mooutils/mooutils-enums.c
    ../moo/mooutils/mooutils-enums.h
    ../moo/mooutils/mooutils-file.c
    ../moo/mooutils/mooutils-file.h
    ../moo/mooutils/mooutils-fs.c
    ../moo/mooutils/mooutils-fs.h
    ../moo/mooutils/mooutils-gobject.c
    ../moo/mooutils/mooutils-gobject.h
    ../moo/mooutils/mooutils-gobject-private.h
    ../moo/mooutils/mooutils.h
    ../moo/mooutils/mooutils-macros.h
    ../moo/mooutils/mooutils-mem.h
    ../moo/mooutils/mooutils-messages.h
    ../moo/mooutils/mooutils-misc.cpp
    ../moo/mooutils/mooutils-misc.h
    ../moo/mooutils/mooutils-script.c
    ../moo/mooutils/mooutils-script.h
    ../moo/mooutils/mooutils-tests.h
    ../moo/mooutils/mooutils-thread.cpp
    ../moo/mooutils/mooutils-thread.h
    ../moo/mooutils/mooutils-treeview.c
    ../moo/mooutils/mooutils-treeview.h
    ../moo/mooutils/mooutils-win32.c
    ../moo/mooutils/moowin32/mingw/fnmatch.h
    ../moo/mooutils/moowin32/mingw/netinet/in.h
    ../moo/mooutils/moowin32/ms/sys/time.h
    ../moo/mooutils/moowin32/ms/unistd.h
    ../moo/mooutils/moowindow.c
    ../moo/mooutils/moowindow.h
    ../moo/plugins/support/moocmdview.cpp
    ../moo/plugins/support/moocmdview.h
    ../moo/plugins/support/mooeditwindowoutput.c
    ../moo/plugins/support/mooeditwindowoutput.h
    ../moo/plugins/support/moolineview.cpp
    ../moo/plugins/support/moolineview.h
    ../moo/plugins/support/moooutputfilter.c
    ../moo/plugins/support/moooutputfilter.h
    ../moo/plugins/usertools/moocommand.c
    ../moo/plugins/usertools/moocommanddisplay.c
    ../moo/plugins/usertools/moocommanddisplay.h
    ../moo/plugins/usertools/moocommand.h
    ../moo/plugins/usertools/moocommand-private.h
    ../moo/plugins/usertools/moooutputfilterregex.c
    ../moo/plugins/usertools/moooutputfilterregex.h
    ../moo/plugins/usertools/moousertools.c
    ../moo/plugins/usertools/moousertools-enums.c
    ../moo/plugins/usertools/moousertools-enums.h
    ../moo/plugins/usertools/moousertools.h
    ../moo/plugins/usertools/moousertools-prefs.c
    ../moo/plugins/usertools/moousertools-prefs.h
)

FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/genmooxml.cmake
"EXECUTE_PROCESS(COMMAND ${MOO_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/parsedocs.py 
		--source-dir ${CMAKE_SOURCE_DIR}/moo/mooapp 
		--source-dir ${CMAKE_SOURCE_DIR}/moo/mooedit 
		--source-dir ${CMAKE_SOURCE_DIR}/moo/moofileview 
		--source-dir ${CMAKE_SOURCE_DIR}/moo/mooutils 
		--source-dir ${CMAKE_SOURCE_DIR}/moo/moopython 
		--source-dir ${CMAKE_SOURCE_DIR}/moo/plugins/usertools 
		--source-dir ${CMAKE_SOURCE_DIR}/moo/plugins/support 
		--source-file ${CMAKE_SOURCE_DIR}/moo/moolua/medit-lua.cpp 
		--source-file ${CMAKE_SOURCE_DIR}/moo/moolua/medit-lua.h 
                --skip moofontsel.*
		--output ${CMAKE_CURRENT_SOURCE_DIR}/moo.xml
        RESULT_VARIABLE RESULT)
if(NOT RESULT EQUAL 0)
    MESSAGE(FATAL_ERROR \"Error: \${RESULT}\")
endif()
file(WRITE moo.xml.stamp moo.xml.stamp)
")

add_custom_command(OUTPUT moo.xml.stamp
    COMMAND ${CMAKE_COMMAND} -P genmooxml.cmake
    DEPENDS ${docparser_files} ${source_files} ${CMAKE_CURRENT_BINARY_DIR}/genmooxml.cmake ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt)
LIST(APPEND built_moo_sources moo.xml.stamp)

FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/gengtkxml.cmake
"EXECUTE_PROCESS(COMMAND ${MOO_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/parsedocs.py 
		--source-file ${CMAKE_SOURCE_DIR}/moo/moolua/gtk-api.c 
		--source-file ${CMAKE_SOURCE_DIR}/moo/moolua/gtk-api.h 
		--module Gtk 
		--output ${CMAKE_CURRENT_SOURCE_DIR}/gtk.xml
        RESULT_VARIABLE RESULT)
if(NOT RESULT EQUAL 0)
    MESSAGE(FATAL_ERROR \"Error: \${RESULT}\")
endif()
file(WRITE gtk.xml.stamp gtk.xml.stamp)
")

add_custom_command(OUTPUT gtk.xml.stamp
    COMMAND ${CMAKE_COMMAND} -P gengtkxml.cmake
    DEPENDS ${docparser_files} ${source_files} ${CMAKE_CURRENT_BINARY_DIR}/gengtkxml.cmake ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt)
LIST(APPEND built_moo_sources gtk.xml.stamp)

add_custom_target(apixml ALL DEPENDS moo.xml.stamp gtk.xml.stamp)
