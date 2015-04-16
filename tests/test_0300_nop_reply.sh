#!/bin/sh

. ./functions.sh

echo "NOP reply tests"

start_target "nop_interval=1,nop_count=3"
create_lun

echo -n "Test that we reply to target initiated NOPs correctly ... "
./prog_noop_reply -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 > /dev/null || failure
success

shutdown_target
delete_lun

exit 0
