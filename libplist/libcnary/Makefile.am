AM_CFLAGS = \
	$(GLOBAL_CFLAGS) \
	-I$(top_srcdir)/libcnary/include

AM_LDFLAGS =

noinst_LTLIBRARIES = libcnary.la
libcnary_la_LIBADD =
libcnary_la_LDFLAGS = $(AM_LDFLAGS) -no-undefined
libcnary_la_SOURCES = \
	node.c \
	node_list.c \
	include/node.h \
	include/node_list.h \
	include/object.h
