AM_CFLAGS = \
	$(GLOBAL_CFLAGS) \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/libcnary/include

AM_LDFLAGS =

noinst_PROGRAMS = \
	plist_cmp \
	plist_test

plist_cmp_SOURCES = plist_cmp.c
plist_cmp_LDADD = \
	$(top_builddir)/src/libplist-2.0.la \
	$(top_builddir)/libcnary/libcnary.la

plist_test_SOURCES = plist_test.c
plist_test_LDADD = $(top_builddir)/src/libplist-2.0.la

TESTS = \
	empty.test \
	small.test \
	medium.test \
	large.test \
	huge.test \
	bigarray.test \
	dates.test \
	timezone1.test \
	timezone2.test \
	signedunsigned1.test \
	signedunsigned2.test \
	signedunsigned3.test \
	hex.test \
	order.test \
	recursion.test \
	entities.test \
	empty_keys.test \
	amp.test \
	invalid_tag.test \
	cdata.test \
	offsetsize.test \
	refsize.test \
	malformed_dict.test

EXTRA_DIST = \
	$(TESTS) \
	data/1.plist \
	data/2.plist \
	data/3.plist \
	data/4.plist \
	data/5.plist \
	data/6.plist \
	data/7.plist \
	data/amp.plist \
	data/cdata.plist \
	data/dictref1byte.bplist \
	data/dictref2bytes.bplist \
	data/dictref3bytes.bplist \
	data/dictref4bytes.bplist \
	data/dictref5bytes.bplist \
	data/dictref6bytes.bplist \
	data/dictref7bytes.bplist \
	data/dictref8bytes.bplist \
	data/empty_keys.plist \
	data/entities.plist \
	data/hex.plist \
	data/invalid_tag.plist \
	data/malformed_dict.bplist \
	data/off1byte.bplist \
	data/off2bytes.bplist \
	data/off3bytes.bplist \
	data/off4bytes.bplist \
	data/off5bytes.bplist \
	data/off6bytes.bplist \
	data/off7bytes.bplist \
	data/off8bytes.bplist \
	data/offxml.plist \
	data/order.bplist \
	data/order.plist \
	data/recursion.bplist \
	data/signed.bplist \
	data/signed.plist \
	data/signedunsigned.bplist \
	data/signedunsigned.plist \
	data/unsigned.bplist \
	data/unsigned.plist

TESTS_ENVIRONMENT = \
	top_srcdir=$(top_srcdir) \
	top_builddir=$(top_builddir)

clean-local:
	if test -d $(top_builddir)/test/data; then cd $(top_builddir)/test/data && rm -f *.out *.bin *.xml; fi
