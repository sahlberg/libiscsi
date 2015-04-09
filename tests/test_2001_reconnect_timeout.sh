#!/bin/sh

. ./functions.sh

echo "Basic Reconnect test"

start_target
create_lun

echo -n "Test iscsi_which_events return 0 on reconnect failure ... "
./prog_reconnect_timeout -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 || failure
success

shutdown_target
delete_lun

exit 0
