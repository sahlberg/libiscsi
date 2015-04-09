#!/bin/sh

. ./functions.sh

echo "Discovery tests"

start_target
create_lun

TEST_TMP=${0}.tmp
echo -n "Test discovery ... "
../utils/iscsi-ls -i ${IQNINITIATOR} iscsi://${TGTPORTAL} > ${TEST_TMP} &&
grep ${IQNTARGET} ${TEST_TMP} > /dev/null || failure
success


shutdown_target
delete_lun

exit 0
