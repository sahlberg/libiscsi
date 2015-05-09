#!/bin/sh

. ./functions.sh

echo "CHAP tests"

start_target
create_lun
setup_chap

echo -n "Test logging in without credentials (should fail) ... "
../utils/iscsi-inq -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 > /dev/null && failure
success


echo -n "Test logging in with invalid user (should fail) ... "
LIBISCSI_CHAP_USERNAME=wrong \
LIBISCSI_CHAP_PASSWORD=libiscsi \
../utils/iscsi-inq -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 > /dev/null && failure
success


echo -n "Test logging in with wrong password (should fail) ... "
LIBISCSI_CHAP_USERNAME=libiscsi \
LIBISCSI_CHAP_PASSWORD=wrong \
../utils/iscsi-inq -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 > /dev/null && failure
success


echo -n "Test logging in with correct credentials (ENV) ... "
LIBISCSI_CHAP_USERNAME=libiscsi \
LIBISCSI_CHAP_PASSWORD=libiscsi \
../utils/iscsi-inq -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 > /dev/null || failure
success


echo -n "Test logging in with correct credentials (URL) ... "
../utils/iscsi-inq -i ${IQNINITIATOR} iscsi://libiscsi%libiscsi@${TGTPORTAL}/${IQNTARGET}/1 > /dev/null || failure
success


echo -n "Test bidir-chap with incorrect user (should fail) ... "
LIBISCSI_CHAP_USERNAME=libiscsi \
LIBISCSI_CHAP_PASSWORD=libiscsi \
LIBISCSI_CHAP_TARGET_USERNAME=wrong \
LIBISCSI_CHAP_TARGET_PASSWORD=outgoing \
../utils/iscsi-inq -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 > /dev/null && failure
success


echo -n "Test bidir-chap with incorrect password (should fail) ... "
LIBISCSI_CHAP_USERNAME=libiscsi \
LIBISCSI_CHAP_PASSWORD=libiscsi \
LIBISCSI_CHAP_TARGET_USERNAME=outgoing \
LIBISCSI_CHAP_TARGET_PASSWORD=wrong \
../utils/iscsi-inq -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 > /dev/null && failure
success


echo -n "Test bidir-chap with correct credentials we get from... "
LIBISCSI_CHAP_USERNAME=libiscsi \
LIBISCSI_CHAP_PASSWORD=libiscsi \
LIBISCSI_CHAP_TARGET_USERNAME=outgoing \
LIBISCSI_CHAP_TARGET_PASSWORD=outgoing \
../utils/iscsi-inq -i ${IQNINITIATOR} iscsi://${TGTPORTAL}/${IQNTARGET}/1 > /dev/null || failure
success


shutdown_target
delete_lun

exit 0
