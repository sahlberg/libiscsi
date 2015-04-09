#!/bin/sh

. ./functions.sh

echo "Basic Reconnect test"

start_target
create_lun

echo -n "Test reading from the LUN when a reconnect happens ... "
./prog_reconnect -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 > /dev/null || failure
success

shutdown_target
delete_lun

exit 0
