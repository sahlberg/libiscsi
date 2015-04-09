#!/bin/sh

. ./functions.sh

echo "Login tests"

start_target
create_lun

echo -n "Test logging in to target ... "
../utils/iscsi-inq -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 > /dev/null || failure
success

shutdown_target
delete_lun

exit 0
