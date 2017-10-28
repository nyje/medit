find_program(XSLTPROC_COMMAND xsltproc)

configure_file(medit-defines.ent.in ${CMAKE_CURRENT_SOURCE_DIR}/built/medit-defines.ent @ONLY)

set(gendocbook_files
	${CMAKE_SOURCE_DIR}/api/gendocbook.py
	${CMAKE_SOURCE_DIR}/api/mpi/__init__.py
	${CMAKE_SOURCE_DIR}/api/mpi/module.py
	${CMAKE_SOURCE_DIR}/api/mpi/docbookwriter.py
)

set(script_docbook_sources
	${CMAKE_CURRENT_SOURCE_DIR}/built/medit-defines.ent
	${CMAKE_CURRENT_SOURCE_DIR}/built/script-python.docbook
	${CMAKE_CURRENT_SOURCE_DIR}/built/script-lua.docbook
	${CMAKE_CURRENT_SOURCE_DIR}/built/script-lua-gtk.docbook
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/built/script-python.docbook
	COMMAND ${MOO_PYTHON} ARGS 
		${CMAKE_SOURCE_DIR}/api/gendocbook.py --python
		--template ${CMAKE_CURRENT_SOURCE_DIR}/script-python.tmpl.docbook
		--output ${CMAKE_CURRENT_SOURCE_DIR}/built/script-python.docbook
		${CMAKE_SOURCE_DIR}/api/moo.xml
	MAIN_DEPENDENCY script-python.tmpl.docbook
	DEPENDS apixml ${gendocbook_files} ${CMAKE_SOURCE_DIR}/api/moo.xml
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/built/script-lua.docbook
	COMMAND ${MOO_PYTHON} ARGS 
		${CMAKE_SOURCE_DIR}/api/gendocbook.py --lua
		--template ${CMAKE_CURRENT_SOURCE_DIR}/script-lua.tmpl.docbook
		--import ${CMAKE_SOURCE_DIR}/api/gtk.xml
		--output ${CMAKE_CURRENT_SOURCE_DIR}/built/script-lua.docbook
		${CMAKE_SOURCE_DIR}/api/moo.xml
	MAIN_DEPENDENCY script-lua.tmpl.docbook
	DEPENDS apixml ${gendocbook_files} ${CMAKE_SOURCE_DIR}/api/moo.xml
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/built/script-lua-gtk.docbook
	COMMAND ${MOO_PYTHON} ARGS 
		${CMAKE_SOURCE_DIR}/api/gendocbook.py --lua
		--template ${CMAKE_CURRENT_SOURCE_DIR}/script-lua-gtk.tmpl.docbook
		--output ${CMAKE_CURRENT_SOURCE_DIR}/built/script-lua-gtk.docbook
		${CMAKE_SOURCE_DIR}/api/gtk.xml
	MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/script-lua.tmpl.docbook
	DEPENDS ${gendocbook_files} ${CMAKE_SOURCE_DIR}/api/gtk.xml
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/help/script/index.html
	COMMAND ${XSLTPROC_COMMAND} ARGS
		--xinclude script-book.xsl script.docbook
	MAIN_DEPENDENCY script.docbook
	DEPENDS ${script_docbook_sources} script-book.xsl
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/help/medit.css
	COMMAND ${CMAKE_COMMAND} ARGS
		-E copy ${CMAKE_CURRENT_SOURCE_DIR}/medit.css ${CMAKE_CURRENT_SOURCE_DIR}/help/medit.css
	MAIN_DEPENDENCY medit.css
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/built/lgpl.no-fancy-chars
	COMMAND ${MOO_PYTHON} ARGS ${CMAKE_CURRENT_SOURCE_DIR}/trim.py
		--input ${CMAKE_SOURCE_DIR}/COPYING
		--output ${CMAKE_CURRENT_SOURCE_DIR}/built/lgpl.no-fancy-chars
	MAIN_DEPENDENCY ${CMAKE_SOURCE_DIR}/COPYING
)

set(docbook_files
	medit.docbook
	prefs.docbook
	regex.docbook
	user-tools.docbook
	license.docbook
)

set(docbook_sources
	${docbook_files}
	built/lgpl.no-fancy-chars
	built/medit-defines.ent
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/help/index.html
	COMMAND ${XSLTPROC_COMMAND} ARGS
		--xinclude medit.xsl medit.docbook
	MAIN_DEPENDENCY medit.docbook
	DEPENDS ${docbook_sources} medit.xsl medit-common.xsl
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/help/help.html
	COMMAND ${XSLTPROC_COMMAND} ARGS
		--xinclude --output help/help.html medit-single.xsl medit.docbook
	MAIN_DEPENDENCY medit.docbook
	DEPENDS ${docbook_sources} medit-single.xsl medit-common.xsl
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_command(OUTPUT toc.xml
	COMMAND ${XSLTPROC_COMMAND} ARGS
		--output toc.xml --xinclude
		--stringparam chunk.first.sections 1
		http://docbook.sourceforge.net/release/xsl/current/html/maketoc.xsl ${CMAKE_CURRENT_SOURCE_DIR}/medit.docbook
	MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/medit.docbook
	DEPENDS ${docbook_sources}
)

add_custom_command(OUTPUT moo-help-sections.h
	COMMAND ${MOO_PYTHON} ARGS
		${CMAKE_CURRENT_SOURCE_DIR}/genhelpsectionsh.py --toc=toc.xml
		--output=moo-help-sections.h
		--srcdir=${CMAKE_CURRENT_SOURCE_DIR} ${docbook_files}
	MAIN_DEPENDENCY toc.xml
	DEPENDS ${docbook_files} genhelpsectionsh.py
)

set(dest_png_files
	${CMAKE_CURRENT_SOURCE_DIR}/help/img/prefs-file-filters.png
	${CMAKE_CURRENT_SOURCE_DIR}/help/img/prefs-file-selector.png
)
add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/help/img/prefs-file-filters.png
	COMMAND ${CMAKE_COMMAND} ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/img/prefs-file-filters.png ${CMAKE_CURRENT_SOURCE_DIR}/help/img/prefs-file-filters.png
	MAIN_DEPENDENCY img/prefs-file-filters.png
)
add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/help/img/prefs-file-selector.png
	COMMAND ${CMAKE_COMMAND} ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/img/prefs-file-selector.png ${CMAKE_CURRENT_SOURCE_DIR}/help/img/prefs-file-selector.png
	MAIN_DEPENDENCY img/prefs-file-selector.png
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/help DESTINATION ${MOO_DOC_DIR})

#EXTRA_DIST = help built/moo-help-sections.h built/medit.1

#install-data-local:
#	$(MKDIR_P) $(DESTDIR)$(MOO_HELP_DIR)/img $(DESTDIR)$(MOO_HELP_DIR)/script
#	cd $(srcdir) && $(INSTALL_DATA) help/*.html help/*.css $(DESTDIR)$(MOO_HELP_DIR)
#	cd $(srcdir) && $(INSTALL_DATA) help/script/*.html $(DESTDIR)$(MOO_HELP_DIR)/script
#	cd $(srcdir) && $(INSTALL_DATA) help/img/*.png $(DESTDIR)$(MOO_HELP_DIR)/img

#uninstall-local:
#	rm -f $(DESTDIR)$(MOO_HELP_DIR)/*.html \
#		$(DESTDIR)$(MOO_HELP_DIR)/*.css \
#		$(DESTDIR)$(MOO_HELP_DIR)/script/*.html \
#		$(DESTDIR)$(MOO_HELP_DIR)/img/*.png

add_custom_target(docs ALL DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/help/help.html
	${CMAKE_CURRENT_SOURCE_DIR}/help/index.html
	${CMAKE_CURRENT_SOURCE_DIR}/help/script/index.html
	${CMAKE_CURRENT_SOURCE_DIR}/help/medit.css
	moo-help-sections.h
	${dest_png_files}
)
