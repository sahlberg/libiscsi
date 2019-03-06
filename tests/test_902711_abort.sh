#!/bin/sh

. ./functions.sh

echo "iscsi-test-cu Read6 test"

python3 ./utils/tgt_crash_test.py
echo -n "SCSI.iSCSITMF ... "
../test-tool/iscsi-test-cu -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 -t SCSI.iSCSITMF --dataloss || failure
#../test-tool/iscsi-test-cu -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 -t SCSI.Unmap --dataloss || failure
success

shutdown_target
iscsiadm -m node --logout
iscsiadm -m node -o delete

exit 0
