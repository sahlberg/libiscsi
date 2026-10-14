#!/bin/sh

. ./functions.sh

echo "Reconnect test with the multithreading service thread"

if [ ! -x ./prog_mt_reconnect ]; then
    echo "prog_mt_reconnect was not built, skipping"
    exit 0
fi

start_target
create_lun

echo -n "Test reading from the LUN when a reconnect happens ... "
./prog_mt_reconnect -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 > /dev/null || failure
success

shutdown_target
delete_lun

exit 0
