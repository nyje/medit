# begin ugly-pre.mk
# -%- lang: makefile; indent-width: 8; use-tabs: true -%-

EXTRA_DIST =
BUILT_SOURCES =
CLEANFILES =
DISTCLEANFILES =

EXTRA_DIST +=					\
    Makefile.ug

BDIST_FILES =
BDIST_EXTRA =

UGLY = $(top_srcdir)/ugly/ugly
UGLY_DEPS =					\
	$(UGLY)					\
	$(top_srcdir)/ugly/repo/ugly-pre.mk	\
	$(top_srcdir)/ugly/repo/ugly-post.mk	\
	$(top_srcdir)/ugly/repo/bdist.mk

BUILT_SOURCES += ugly-pre-build-stamp
UGLY_PRE_BUILD_TARGETS =
UGLY_CLEAN_TARGETS =
ugly-pre-build-stamp: $(UGLY_PRE_BUILD_TARGETS)
	@echo stamp > ugly-pre-build-stamp
clean-local: $(UGLY_CLEAN_TARGETS)

UGLY_SUBDIRS =
UGLY_PRE_BUILD_TARGETS += ugly-subdirs-stamp
UGLY_CLEAN_TARGETS += delete-ugly-subdir-makefile
ugly-subdirs-stamp: Makefile $(top_srcdir)/ugly/repo/ugly-subdir-Makefile
	@if test -n "$(UGLY_SUBDIRS)"; then \
	  for d in $(UGLY_SUBDIRS); do \
	    mkdir -p $$d || exit 1; \
	    cp $(top_srcdir)/ugly/repo/ugly-subdir-Makefile $$d/Makefile || exit 1; \
	  done; \
	fi
	@echo stamp > ugly-subdirs-stamp
delete-ugly-subdir-makefile:
	@if test -n "$(UGLY_SUBDIRS)"; then \
	  for d in $(UGLY_SUBDIRS); do \
	    rm -f $$d/Makefile || exit 1; \
	  done; \
	fi

CLEANFILES += ugly-pre-build-stamp ugly-subdirs-stamp

# end ugly-pre.mk
