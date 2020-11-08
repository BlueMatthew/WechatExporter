#!/bin/sh

CURDIR=`pwd`
FUZZDIR=`dirname $0`

cd ${FUZZDIR}

if ! test -x xplist_fuzzer || ! test -x bplist_fuzzer; then
	echo "ERROR: you need to build the fuzzers first."
	cd ${CURDIR}
	exit 1
fi

mkdir -p xplist-input
cp ../test/data/*.plist xplist-input/
./xplist_fuzzer -merge=1 xplist-input xplist-crashes xplist-leaks -dict=xplist.dict

mkdir -p bplist-input
cp ../test/data/*.bplist bplist-input/
./bplist_fuzzer -merge=1 bplist-input bplist-crashes bplist-leaks -dict=bplist.dict

cd ${CURDIR}
exit 0
