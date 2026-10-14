#!/bin/sh

. ./functions.sh

echo "Commands that keep dropping the connection"

if [ ! -x ./prog_resend_limit ]; then
    echo "prog_resend_limit was not built, skipping"
    exit 0
fi

start_target
create_lun

echo -n "Test that a command is given up on after 3 resends ... "
./prog_resend_limit -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 > /dev/null || failure
success

shutdown_target
delete_lun

exit 0
