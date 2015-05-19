#!/bin/sh

. ./functions.sh

echo "Basic iSCSI Timeout test"

start_target
create_lun

echo -n "Test that timeouts trigger when we get no reply to an iSCSI PDU..."
./prog_timeout -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 || failure
success

shutdown_target
delete_lun

exit 0
