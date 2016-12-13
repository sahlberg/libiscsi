#!/bin/sh

. ./functions.sh

echo "Test that a single call to iscsi_service will process all queued PDUs"

start_target
create_lun

echo -n "Test reading all queued PDUs in a single iscsi_service() call ... "
./prog_read_all_pdus -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 || failure
success

shutdown_target
delete_lun

exit 0
