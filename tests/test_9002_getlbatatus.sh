#!/bin/sh

. ./functions.sh

echo "iscsi-test-cu GetLBAStatus test"

start_target
create_disk_lun 1 100M

echo -n "SCSI.GetLBAStatus ... "
../test-tool/iscsi-test-cu -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 -t SCSI.GetLBAStatus.Simple --dataloss > /dev/null || failure
success

shutdown_target
delete_disk_lun 1

exit 0
