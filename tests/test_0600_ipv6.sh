#!/bin/sh

. ./functions.sh

echo "IPv6 tests"

start_target "nop_interval=1,nop_count=3,portal=[::1]:3269"
create_lun

echo -n "Test that we can connect to an IPv6 target ..."
../utils/iscsi-inq -i ${IQNINITIATOR} iscsi://[::1]:3269/${IQNTARGET}/1 > /dev/null || failure
success

shutdown_target
delete_lun

exit 0
