# -*- makefile -*-

# $(mime_in_files) should be a list of .xml.in files
# call MOO_AM_MIME_MK autoconf macro

mimepackagesdir = $(mimedir)/packages
mime_files = $(patsubst %.xml.in,%.xml,$(mime_in_files))
mimepackages_DATA = $(mime_files)

update_mime = update-mime-database $(DESTDIR)${mimedir}

@MOO_INTLTOOL_XML_RULE@

EXTRA_DIST += $(mime_in_files)
CLEANFILES += $(mime_files)

if MOO_ENABLE_GENERATED_FILES
install_data_hook_dummy_var =

if !MOO_OS_MINGW

uninstall_hook_dummy_var =

install-data-hook:
	if echo "Updating mime database... " && $(update_mime); then		\
		echo "Done.";							\
	else									\
		echo "*** ";							\
		echo "*** Mime database not updated. After install, run this:";	\
		echo $(update_mime);						\
		echo "*** ";							\
	fi
uninstall-hook:
	if echo "Updating mime database" && $(update_mime); then echo "Done."; else echo "Failed."; fi

else MOO_OS_MINGW

install-data-hook:
	if [ -e /usr/share/mime/packages/freedesktop.org.xml ]; then \
	  cp /usr/share/mime/packages/freedesktop.org.xml $(DESTDIR)${mimedir}/packages/ ; \
	elif [ -e /opt/gtk/share/mime/packages/freedesktop.org.xml ]; then \
	  cp /opt/gtk/share/mime/packages/freedesktop.org.xml $(DESTDIR)${mimedir}/packages/ ; \
	fi; \
	$(update_mime)

endif MOO_OS_MINGW
endif MOO_ENABLE_GENERATED_FILES
