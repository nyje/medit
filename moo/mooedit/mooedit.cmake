LIST(APPEND moo_edit_enum_headers mooedit/mooedit-enums.h)

SET(mooedit_gtksourceview_sources
    mooedit/native/mooeditbookmark.cpp
    mooedit/native/mooeditbookmark.h
    mooedit/native/moolang-private.h
    mooedit/native/moolang.cpp
    mooedit/native/moolang.h	
    mooedit/native/moolangmgr-private.h
    mooedit/native/moolangmgr.cpp
    mooedit/native/moolangmgr.h	
    mooedit/native/mootextstylescheme.c
    mooedit/native/mootextstylescheme.h
)

SET(mooedit_sci_sources
)

SET(mooedit_sources
    mooedit/mooedit.cmake
    ${moo_edit_enum_headers}
    mooedit/mooedit-accels.h
    mooedit/mooedit-enum-types.c
    mooedit/mooedit-enum-types.h
    mooedit/mooedit-enums.h	
    mooedit/mooedit-fileops.cpp
    mooedit/mooedit-fileops.h
    mooedit/mooedit-impl.h	
    mooedit/mooedit-private.h
    mooedit/mooedit-script.cpp
    mooedit/mooedit-script.h
    mooedit/mooedit.cpp
    mooedit/mooedit.h
    mooedit/mooeditaction-factory.cpp
    mooedit/mooeditaction-factory.h
    mooedit/mooeditaction.cpp
    mooedit/mooeditaction.h
    mooedit/mooeditconfig.cpp
    mooedit/mooeditconfig.h
    mooedit/mooeditdialogs.cpp
    mooedit/mooeditdialogs.h
    mooedit/mooeditfileinfo-impl.h
    mooedit/mooeditfileinfo.cpp
    mooedit/mooeditfileinfo.h
    mooedit/mooeditfiltersettings.cpp
    mooedit/mooeditfiltersettings.h
    mooedit/mooedithistoryitem.cpp
    mooedit/mooedithistoryitem.h
    mooedit/mooeditor-impl.h
    mooedit/mooeditor-private.h
    mooedit/mooeditor-tests.cpp
    mooedit/mooeditor-tests.h
    mooedit/mooeditor-tests.h
    mooedit/mooeditor.cpp
    mooedit/mooeditor.h
    mooedit/mooeditprefs.cpp
    mooedit/mooeditprefs.h
    mooedit/mooeditprefspage.cpp
    mooedit/mooeditprogress.c
    mooedit/mooeditprogress.h
    mooedit/mooedittab-impl.h
    mooedit/mooedittab.cpp
    mooedit/mooedittab.h	
    mooedit/mooedittypes.h
    mooedit/mooeditview-impl.h
    mooedit/mooeditview-priv.h
    mooedit/mooeditview-script.cpp
    mooedit/mooeditview-script.h
    mooedit/mooeditview.cpp
    mooedit/mooeditview.h
    mooedit/mooeditwindow-impl.h
    mooedit/mooeditwindow.cpp
    mooedit/mooeditwindow.h
    mooedit/moofold.cpp
    mooedit/moofold.h	
    mooedit/mooindenter.cpp
    mooedit/mooindenter.h	
    mooedit/moolinebuffer.c	
    mooedit/moolinebuffer.h	
    mooedit/moolinemark.c	
    mooedit/moolinemark.h	
    mooedit/mooplugin-loader.c
    mooedit/mooplugin-loader.h
    mooedit/mooplugin-macro.h
    mooedit/mooplugin.c	
    mooedit/mooplugin.h
    mooedit/mootext-private.h
    mooedit/mootextbtree.c	
    mooedit/mootextbtree.h	
    mooedit/mootextbuffer.c	
    mooedit/mootextbuffer.h
    mooedit/mootextfind.c	
    mooedit/mootextfind.h
    mooedit/mootextiter.h
    mooedit/mootextprint-private.h
    mooedit/mootextprint.c	
    mooedit/mootextprint.h
    mooedit/mootextsearch-private.h
    mooedit/mootextsearch.c	
    mooedit/mootextsearch.h
    mooedit/mootextview-input.c
    mooedit/mootextview-private.h
    mooedit/mootextview.c	
    mooedit/mootextview.h
)

if(MOO_USE_SCI)
    list(APPEND mooedit_sources ${mooedit_sci_sources})
else()
    list(APPEND mooedit_sources ${mooedit_gtksourceview_sources})
endif()

# SET(built_mooedit_sources mooedit/mooedit-enum-types.h.stamp mooedit/mooedit-enum-types.c.stamp)

# add_custom_command(OUTPUT mooedit/mooedit-enum-types.h.stamp
#     COMMAND ${MOO_PYTHON} ${CMAKE_SOURCE_DIR}/tools/xml2h.py ${CMAKE_CURRENT_SOURCE_DIR}/${input} zzz_ui_xml -o ${_ui_output}
#     COMMAND 
#     $(AM_V_GEN)( cd $(srcdir) && \
#     	$(GLIB_MKENUMS) --template mooedit/mooedit-enum-types.h.tmpl $(moo_edit_enum_headers) ) > mooedit/mooedit-enum-types.h.tmp
#     $(AM_V_at)cmp -s mooedit/mooedit-enum-types.h.tmp $(srcdir)/mooedit/mooedit-enum-types.h || \
#     	mv mooedit/mooedit-enum-types.h.tmp $(srcdir)/mooedit/mooedit-enum-types.h
#     $(AM_V_at)rm -f mooedit/mooedit-enum-types.h.tmp
#     $(AM_V_at)echo stamp > mooedit/mooedit-enum-types.h.stamp
#     MAIN_DEPENDENCY mooedit/mooedit-enum-types.h.tmpl
#     DEPENDS ${moo_edit_enum_headers}
#     COMMENT "Generating ${_ui_output} from ${input}")

# :  Makefile 
# mooedit/mooedit-enum-types.c.stamp: $(moo_edit_enum_headers) Makefile mooedit/mooedit-enum-types.c.tmpl
# 	$(AM_V_at)$(MKDIR_P) mooedit
# 	$(AM_V_GEN)( cd $(srcdir) && \
# 		$(GLIB_MKENUMS) --template mooedit/mooedit-enum-types.c.tmpl $(moo_edit_enum_headers) ) > mooedit/mooedit-enum-types.c.tmp
# 	$(AM_V_at)cmp -s mooedit/mooedit-enum-types.c.tmp $(srcdir)/mooedit/mooedit-enum-types.c || \
# 		mv mooedit/mooedit-enum-types.c.tmp $(srcdir)/mooedit/mooedit-enum-types.c
# 	$(AM_V_at)rm -f mooedit/mooedit-enum-types.c.tmp
# 	$(AM_V_at)echo stamp > mooedit/mooedit-enum-types.c.stamp

# zzz
# include mooedit/langs/Makefile.incl

foreach(input_file
    mooedit/glade/moopluginprefs.glade		
    mooedit/glade/mooeditprefs-view.glade	
    mooedit/glade/mooeditprefs-file.glade	
    mooedit/glade/mooeditprefs-filters.glade	
    mooedit/glade/mooeditprefs-general.glade	
    mooedit/glade/mooeditprefs-langs.glade	
    mooedit/glade/mooeditprogress.glade		
    mooedit/glade/mooeditsavemult.glade		
    mooedit/glade/mootryencoding.glade		
    mooedit/glade/mooprint.glade			
    mooedit/glade/mootextfind.glade		
    mooedit/glade/mootextfind-prompt.glade	
    mooedit/glade/mootextgotoline.glade		
    mooedit/glade/mooquicksearch.glade		
    mooedit/glade/moostatusbar.glade
)
    ADD_GXML(${input_file})
endforeach(input_file)

ADD_UI(mooedit/medit.xml)
ADD_UI(mooedit/mooedit.xml)

install(FILES
    mooedit/langs/actionscript.lang
    mooedit/langs/ada.lang
    mooedit/langs/asp.lang
    mooedit/langs/automake.lang
    mooedit/langs/awk.lang
    mooedit/langs/bennugd.lang
    mooedit/langs/bibtex.lang
    mooedit/langs/bluespec.lang
    mooedit/langs/boo.lang
    mooedit/langs/cg.lang
    mooedit/langs/changelog.lang
    mooedit/langs/chdr.lang
    mooedit/langs/c.lang
    mooedit/langs/cmake.lang
    mooedit/langs/cobol.lang
    mooedit/langs/cpp.lang
    mooedit/langs/csharp.lang
    mooedit/langs/css.lang
    mooedit/langs/cuda.lang
    mooedit/langs/def.lang
    mooedit/langs/desktop.lang
    mooedit/langs/diff.lang
    mooedit/langs/d.lang
    mooedit/langs/docbook.lang
    mooedit/langs/dosbatch.lang
    mooedit/langs/dot.lang
    mooedit/langs/dpatch.lang
    mooedit/langs/dtd.lang
    mooedit/langs/eiffel.lang
    mooedit/langs/erlang.lang
    mooedit/langs/fcl.lang
    mooedit/langs/forth.lang
    mooedit/langs/fortran.lang
    mooedit/langs/fsharp.lang
    mooedit/langs/gap.lang
    mooedit/langs/gdb-log.lang
    mooedit/langs/glsl.lang
    mooedit/langs/go.lang
    mooedit/langs/gtk-doc.lang
    mooedit/langs/gtkrc.lang
    mooedit/langs/haddock.lang
    mooedit/langs/haskell.lang
    mooedit/langs/haskell-literate.lang
    mooedit/langs/html.lang
    mooedit/langs/idl-exelis.lang
    mooedit/langs/idl.lang
    mooedit/langs/imagej.lang
    mooedit/langs/ini.lang
    mooedit/langs/java.lang
    mooedit/langs/javascript.lang
    mooedit/langs/j.lang
    mooedit/langs/json.lang
    mooedit/langs/julia.lang
    mooedit/langs/latex.lang
    mooedit/langs/libtool.lang
    mooedit/langs/lua.lang
    mooedit/langs/m4.lang
    mooedit/langs/makefile.lang
    mooedit/langs/mallard.lang
    mooedit/langs/markdown.lang
    mooedit/langs/matlab.lang
    mooedit/langs/mediawiki.lang
    mooedit/langs/modelica.lang
    mooedit/langs/msil.lang
    mooedit/langs/mxml.lang
    mooedit/langs/nemerle.lang
    mooedit/langs/netrexx.lang
    mooedit/langs/nsis.lang
    mooedit/langs/objc.lang
    mooedit/langs/objj.lang
    mooedit/langs/ocaml.lang
    mooedit/langs/ocl.lang
    mooedit/langs/octave.lang
    mooedit/langs/ooc.lang
    mooedit/langs/opal.lang
    mooedit/langs/opencl.lang
    mooedit/langs/pascal.lang
    mooedit/langs/perl.lang
    mooedit/langs/php.lang
    mooedit/langs/pkgconfig.lang
    mooedit/langs/po.lang
    mooedit/langs/prolog.lang
    mooedit/langs/protobuf.lang
    mooedit/langs/puppet.lang
    mooedit/langs/python3.lang
    mooedit/langs/python-console.lang
    mooedit/langs/python.lang
    mooedit/langs/R.lang
    mooedit/langs/rpmspec.lang
    mooedit/langs/ruby.lang
    mooedit/langs/scala.lang
    mooedit/langs/scheme.lang
    mooedit/langs/scilab.lang
    mooedit/langs/sh.lang
    mooedit/langs/sml.lang
    mooedit/langs/sparql.lang
    mooedit/langs/sql.lang
    mooedit/langs/systemverilog.lang
    mooedit/langs/t2t.lang
    mooedit/langs/tcl.lang
    mooedit/langs/texinfo.lang
    mooedit/langs/vala.lang
    mooedit/langs/vbnet.lang
    mooedit/langs/verilog.lang
    mooedit/langs/vhdl.lang
    mooedit/langs/xml.lang
    mooedit/langs/xslt.lang
    mooedit/langs/yacc.lang
    mooedit/langs/kate.xml
    mooedit/langs/classic.xml
    mooedit/langs/cobalt.xml
    mooedit/langs/oblivion.xml
    mooedit/langs/tango.xml
    mooedit/langs/medit.xml
    mooedit/langs/language2.rng
    mooedit/langs/check.sh
DESTINATION ${MOO_TEXT_LANG_FILES_DIR})
