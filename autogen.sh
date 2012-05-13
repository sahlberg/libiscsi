#!/bin/sh

rm -rf autom4te.cache
rm -f depcomp aclocal.m4 missing config.guess config.sub install-sh
rm -f configure config.h.in config.h.in~ m4/libtool.m4 m4/lt*.m4 Makefile.in
mkdir -p m4
autoreconf -fvi
(cd m4 && for i in libtool.m4 lt*.m4; do
  echo /$i
done) > m4/.gitignore
rm -rf autom4te.cache

echo "Now run ./configure and then make."
exit 0

