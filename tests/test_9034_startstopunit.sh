#!/bin/sh

. ./functions.sh

echo "iscsi-test-cu StartStopUnit test for fixed media"

start_target
create_disk_lun 1 100M

echo -n "SCSI.StartStopUnit ... "
../test-tool/iscsi-test-cu -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 -t SCSI.StartStopUnit --dataloss > /dev/null || failure
success

shutdown_target
delete_disk_lun 1

exit 0
