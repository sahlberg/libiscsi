#!/bin/sh
#
# makerpms.sh  -  build RPM packages from the git sources
#
# Copyright (C) John H Terpstra 1998-2002
# Copyright (C) Gerald (Jerry) Carter 2003
# Copyright (C) Jim McDonough 2007
# Copyright (C) Andrew Tridgell 2007
# Copyright (C) Michael Adam 2008-2009
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 3 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <http://www.gnu.org/licenses/>.
#

#
# The following allows environment variables to override the target directories
#   the alternative is to have a file in your home directory calles .rpmmacros
#   containing the following:
#   %_topdir  /home/mylogin/redhat
#
# Note: Under this directory rpm expects to find the same directories that are under the
#   /usr/src/redhat directory
#

EXTRA_OPTIONS="$1"

DIRNAME=$(dirname $0)
TOPDIR=${DIRNAME}/../..

SPECDIR=`rpm --eval %_specdir`
SRCDIR=`rpm --eval %_sourcedir`

SPECFILE="libiscsi.spec"
SPECFILE_IN="libiscsi.spec.in"
RPMBUILD="rpmbuild"

GITHASH=".$(git log --pretty=format:%h -1)"

if test "x$USE_GITHASH" = "xno" ; then
	GITHASH=""
fi

sed -e s/GITHASH/${GITHASH}/g \
	< ${DIRNAME}/${SPECFILE_IN} \
	> ${DIRNAME}/${SPECFILE}

VERSION=$(grep ^Version ${DIRNAME}/${SPECFILE} | sed -e 's/^Version:\ \+//')
RELEASE=$(grep ^Release ${DIRNAME}/${SPECFILE} | sed -e 's/^Release:\ \+//')

if echo | gzip -c --rsyncable - > /dev/null 2>&1 ; then
	GZIP_ENV="-9 --rsyncable"
else
	GZIP_ENV="-9"
fi

pushd ${TOPDIR}
echo -n "Creating libiscsi-${VERSION}.tar.gz ... "
sh autogen.sh
make dist GZIP_ENV="\"$GZIP_ENV\""
RC=$?
popd
echo "Done."
if [ $RC -ne 0 ]; then
        echo "Build failed!"
        exit 1
fi

# At this point the SPECDIR and SRCDIR vaiables must have a value!

##
## copy additional source files
##
cp -p ${TOPDIR}/libiscsi-${VERSION}.tar.gz ${SRCDIR}
cp -p ${DIRNAME}/${SPECFILE} ${SPECDIR}

##
## Build
##
echo "$(basename $0): Getting Ready to build release package"
${RPMBUILD} -ba --clean --rmsource ${EXTRA_OPTIONS} ${SPECDIR}/${SPECFILE} || exit 1

echo "$(basename $0): Done."

exit 0
