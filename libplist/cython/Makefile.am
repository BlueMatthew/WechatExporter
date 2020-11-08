AM_CPPFLAGS = -I$(top_srcdir)/include

AM_CFLAGS = $(GLOBAL_CFLAGS)
AM_LDFLAGS = $(GLOBAL_LDFLAGS)

EXTRA_DIST = \
	plist.pyx \
	plist.pxd

if HAVE_CYTHON

BUILT_SOURCES = plist.c

PXDINCLUDES = \
	plist.pxd \
	$(CYTHON_PLIST_INCLUDE_DIR)/plist.pxd

CLEANFILES = \
	*.pyc \
	*.pyo \
	plist.c

plistdir = $(pyexecdir)
plist_LTLIBRARIES = plist.la
plist_la_SOURCES = \
	plist_util.c \
	plist_util.h \
	plist.pyx

plist_la_CFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/src \
	$(PYTHON_CPPFLAGS) \
	$(AM_CFLAGS) \
	-Wno-shadow \
	-Wno-redundant-decls \
	-Wno-switch-default \
	-Wno-strict-aliasing \
	-Wno-implicit-function-declaration \
	-fvisibility=default

plist_la_LDFLAGS = -module -avoid-version $(PYTHON_LIBS) $(AM_LDFLAGS)
plist_la_LIBADD = $(top_builddir)/src/libplist-2.0.la

plist.c: plist.pyx $(PXDINCLUDES) $(PXIINCLUDES)

.pyx.c:
	$(CYTHON) -I$(CYTHON_PLIST_INCLUDE_DIR) -I$(top_srcdir)/src -o $@ $<

pxddir = $(includedir)/plist/cython
pxd_DATA = plist.pxd

endif
