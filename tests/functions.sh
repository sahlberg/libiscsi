export TGT_IPC_SOCKET=`pwd`/tgtd.socket 

TGTD="/usr/sbin/tgtd"
TGTADM="tgtadm"
TGTLUN=`pwd`/LUN
TGTPORTAL=127.0.0.1:3260
ETCDIP="http://127.0.0.1:2379"
SVC_LABLE="tgt_svc"
TGT_VERSION="v1.0"
HA_SVC_PORT=9001
STORD_IP="127.0.0.1"
STORD_PORT="9876"

IQNTARGET=test-1
IQNINITIATOR=iqn.libiscsi.unittest.initiator
TGTURL=iscsi://${TGTPORTAL}/${IQNTARGET}/1

start_target() {
    # in case we have one still running from a previous run
    ${TGTADM} --op delete --force --mode target --tid 1 2>/dev/null
    ${TGTADM} --op delete --mode system 2>/dev/null
    tgtadm --op new --mode target --tid 1 -T iqn.libiscsi.unittest.target
    echo "status:$?"
    echo "${TGTADM} --op new --mode target --tid 1 -T ${IQNTARGET}"
    ${TGTADM} --op bind --mode target --tid 1 -I ALL
    echo "status:$?"
    echo "${TGTADM} --op bind --mode target --tid 1 -I ALL"
}

shutdown_target() {
    # Remove target
    echo "Shutting down iSCSI target"
    ${TGTADM} --op delete --force --mode target --tid 1
    ${TGTADM} --op delete --mode system
}

enable_header_digest() {
    ${TGTADM} --op update --mode target --tid 1 -n HeaderDigest -v CRC32C
}

create_lun() {
    # Setup LUN
    truncate --size=100M ${TGTLUN}
    ${TGTADM} --op new --mode logicalunit --tid 1 --lun 1 -b ${TGTLUN} --blocksize=4096
    ${TGTADM} --op update --mode logicalunit --tid 1 --lun 1 --params thin_provisioning=1
}

delete_lun() {
    # Remove LUN
    rm ${TGTLUN}
}

create_disk_lun() {
    # Setup LUN
    truncate --size=$2 ${TGTLUN}.$1
    ${TGTADM} --op new --mode logicalunit --tid 1 --lun $1 -b ${TGTLUN}.$1 --blocksize=512

    iscsiadm -m discovery -t sendtargets -p 127.0.0.1:3260
    iscsiadm -m node --targetname "iqn.libiscsi.unittest.target" --portal "127.0.0.1:3260" --login
}

delete_disk_lun() {
    # Remove LUN
    rm ${TGTLUN}.$1
}

add_disk_lun() {
    ${TGTADM} --op new --mode logicalunit --tid 1 --lun $1 -b ${TGTLUN}.$1 --blocksize=512
}

remove_disk_lun() {
    ${TGTADM} --op delete --mode logicalunit --tid 1 --lun $1
}

set_lun_removable() {
    ${TGTADM} --op update --mode logicalunit --tid 1 --lun $1 --params removable=1
}

setup_chap() {
    ${TGTADM} --op new --mode account --user libiscsi --password libiscsi
    ${TGTADM} --op bind --mode account --tid 1 --user libiscsi

    ${TGTADM} --op new --mode account --user outgoing --password outgoing
    ${TGTADM} --op bind --mode account --tid 1 --user outgoing --outgoing
}

success() {
    echo "[OK]"
    rm ${TEST_TMP} 2> /dev/null
}

failure() {
    echo "[FAILED]"
    exit 1
}
