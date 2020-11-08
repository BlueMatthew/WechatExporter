if BUILD_FUZZERS

libFuzzer.a: Fuzzer/build.sh
	@echo "Building $@"
	@./Fuzzer/build.sh

Fuzzer/build.sh: LIBFUZZER_SRC

LIBFUZZER_SRC:
	@if test -d Fuzzer ; then \
		if test -d Fuzzer/.git ; then \
			echo Making sure libFuzzer source tree is up-to-date... ; \
			cd Fuzzer && git checkout . && git pull && cd .. ; \
		fi \
	else \
		echo Checking out libFuzzer source code... ; \
		git clone https://chromium.googlesource.com/chromium/llvm-project/llvm/lib/Fuzzer ; \
	fi

CLEANFILES = libFuzzer.a

noinst_PROGRAMS = \
	xplist_fuzzer \
	bplist_fuzzer

xplist_fuzzer_SOURCES = xplist_fuzzer.cc
xplist_fuzzer_LDFLAGS = -static
xplist_fuzzer_LDADD = $(top_builddir)/src/libplist-2.0.la libFuzzer.a

bplist_fuzzer_SOURCES = bplist_fuzzer.cc
bplist_fuzzer_LDFLAGS = -static
bplist_fuzzer_LDADD = $(top_builddir)/src/libplist-2.0.la libFuzzer.a

TESTS = fuzzers.test

EXTRA_DIST = \
	bplist.dict \
	xplist.dict \
	init-fuzzers.sh \
	test-fuzzers.sh \
	fuzzers.test

endif
