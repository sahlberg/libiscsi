#!/bin/sh

. ./functions.sh

echo "Synchronous command timeouts"

if [ ! -x ./prog_sync_timeout ]; then
    echo "prog_sync_timeout was not built, skipping"
    exit 0
fi

start_target
create_lun

echo -n "Test that a timed out synchronous command leaves nothing behind ... "
./prog_sync_timeout -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 > /dev/null || failure
success

shutdown_target
delete_lun

exit 0
