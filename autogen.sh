#!/bin/sh

rm -rf autom4te.cache
rm -f configure config.h.in ctdb.pc

IPATHS="-I ./include -I ../include"

autoheader $IPATHS || exit 1
autoconf $IPATHS || exit 1

rm -rf autom4te.cache

echo "Now run ./configure and then make."
exit 0

