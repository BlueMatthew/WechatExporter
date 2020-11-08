AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libcnary/include

AM_CFLAGS = $(GLOBAL_CFLAGS)
AM_LDFLAGS = $(GLOBAL_LDFLAGS)

lib_LTLIBRARIES = \
	libplist-2.0.la \
	libplist++-2.0.la

libplist_2_0_la_LIBADD = $(top_builddir)/libcnary/libcnary.la
libplist_2_0_la_LDFLAGS = $(AM_LDFLAGS) -version-info $(LIBPLIST_SO_VERSION) -no-undefined
libplist_2_0_la_SOURCES = \
	base64.c base64.h \
	bytearray.c bytearray.h \
	strbuf.h \
	hashtable.c hashtable.h \
	ptrarray.c ptrarray.h \
	time64.c time64.h \
	time64_limits.h \
	xplist.c \
	bplist.c \
	plist.c plist.h

libplist___2_0_la_LIBADD = libplist-2.0.la
libplist___2_0_la_LDFLAGS = $(AM_LDFLAGS) -version-info $(LIBPLIST_SO_VERSION) -no-undefined
libplist___2_0_la_SOURCES = \
	Node.cpp \
	Structure.cpp \
	Array.cpp \
	Boolean.cpp \
	Data.cpp \
	Date.cpp \
	Dictionary.cpp \
	Integer.cpp \
	Key.cpp \
	Real.cpp \
	String.cpp \
	Uid.cpp \
	$(top_srcdir)/include/plist/Node.h \
	$(top_srcdir)/include/plist/Structure.h \
	$(top_srcdir)/include/plist/Array.h \
	$(top_srcdir)/include/plist/Boolean.h \
	$(top_srcdir)/include/plist/Data.h \
	$(top_srcdir)/include/plist/Date.h \
	$(top_srcdir)/include/plist/Dictionary.h \
	$(top_srcdir)/include/plist/Integer.h \
	$(top_srcdir)/include/plist/Key.h \
	$(top_srcdir)/include/plist/Real.h \
	$(top_srcdir)/include/plist/String.h \
	$(top_srcdir)/include/plist/Uid.h

if WIN32
libplist_2_0_la_LDFLAGS += -avoid-version -static-libgcc
libplist___2_0_la_LDFLAGS += -avoid-version -static-libgcc
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = \
	libplist-2.0.pc \
	libplist++-2.0.pc
