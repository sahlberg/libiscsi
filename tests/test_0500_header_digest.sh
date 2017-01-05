#!/bin/sh

. ./functions.sh

echo "Header Digest tests"

start_target "nop_interval=1,nop_count=3"
enable_header_digest
create_lun

echo -n "Test that we can connect to a target requiring Header Digest ..."
./prog_header_digest -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 > /dev/null || failure
success

shutdown_target
delete_lun

exit 0
