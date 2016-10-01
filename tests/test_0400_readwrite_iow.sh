#!/bin/sh

. ./functions.sh

echo "Test Read/Write using iovectors"

start_target
create_lun

echo -n "Test read/write using iovectors ... "
./prog_readwrite_iov -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 || failure
success

shutdown_target
delete_lun

exit 0
