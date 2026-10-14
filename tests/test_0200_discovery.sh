#!/bin/sh

. ./functions.sh

echo "Discovery tests"

start_target
create_lun

TEST_TMP=${0}.tmp
echo -n "Test discovery ... "
../utils/iscsi-ls -i ${IQNINITIATOR} iscsi://${TGTPORTAL} > ${TEST_TMP} &&
grep ${IQNTARGET} ${TEST_TMP} > /dev/null || failure
success

# tgtd lists an iSER target to the TCP discovery session, but it can't be
# logged in to over TCP. iscsi-ls -s must report it and still list the
# luns of the other target.
echo -n "Test listing luns after a target that can't be logged in to ... "
if ${TGTADM} --lld iser --op new --mode target --tid 2 -T ${IQNTARGET}.iser 2>/dev/null &&
   ${TGTADM} --lld iser --op bind --mode target --tid 2 -I ALL; then
    ../utils/iscsi-ls -s -i ${IQNINITIATOR} iscsi://${TGTPORTAL} > ${TEST_TMP} 2>&1 &&
        failure
    grep "${IQNTARGET}.iser failed" ${TEST_TMP} > /dev/null || failure
    grep "^Lun:1 .*DIRECT_ACCESS" ${TEST_TMP} > /dev/null || failure
    ${TGTADM} --lld iser --op delete --force --mode target --tid 2
    success
else
    echo "SKIPPED (tgtd has no iSER support)"
fi
rm -f ${TEST_TMP}


shutdown_target
delete_lun

exit 0
