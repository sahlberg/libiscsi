#!/bin/bash -x

_fatal() {
	echo "Fatal Error $*"
	exit 1
}

_warn() {
	echo "WARNING $*"
}

TARGET_IQN="${TARGET_IQN:-iqn.2003-01.org.linux-iscsi:libiscsi:ci}"
INITIATOR_IQNS="iqn.2007-10.com.github:sahlberg:libiscsi:iscsi-test iqn.2007-10.com.github:sahlberg:libiscsi:iscsi-test-2"
# $ISCSI_USER and $ISCSI_PASS can also be set for CHAP configuration

modprobe -a iscsi_target_mod target_core_file configfs || _fatal

grep -m1 configfs /proc/mounts \
	|| mount -t configfs configfs /sys/kernel/config/

file_path=/lun_filer
file_size_b="$((1024 * 1024 * 1024))"
truncate --size="${file_size_b}" "$file_path" || _fatal

mkdir -p /sys/kernel/config/target/iscsi || _fatal
pushd /sys/kernel/config/target/core/ || _fatal
mkdir -p fileio_0/filer || _fatal
echo "fd_dev_name=${file_path}" > fileio_0/filer/control || _fatal
echo "fd_dev_size=${file_size_b}" > fileio_0/filer/control || _fatal
mkdir -p /var/target/alua/tpgs_myserial /var/target/pr || _fatal
echo "myserial" > fileio_0/filer/wwn/vpd_unit_serial || _fatal
echo "1" > fileio_0/filer/enable || _fatal
# unmap/discard can't be set before "enable" for some reason...
echo "1" > fileio_0/filer/attrib/emulate_tpu || _warn
popd

pushd /sys/kernel/config/target/iscsi || _fatal
echo -n 0 > discovery_auth/enforce_discovery_auth
mkdir -p "${TARGET_IQN}/tpgt_0/lun/lun_0" || _fatal
ln -s /sys/kernel/config/target/core/fileio_0/filer \
	"${TARGET_IQN}/tpgt_0/lun/lun_0/68c6222530" || _fatal

echo 0 > "${TARGET_IQN}/tpgt_0/attrib/t10_pi"
echo 0 > "${TARGET_IQN}/tpgt_0/attrib/default_erl"
echo 1 > "${TARGET_IQN}/tpgt_0/attrib/demo_mode_discovery"
echo 0 > "${TARGET_IQN}/tpgt_0/attrib/prod_mode_write_protect"
echo 0 > "${TARGET_IQN}/tpgt_0/attrib/demo_mode_write_protect"
echo 0 > "${TARGET_IQN}/tpgt_0/attrib/cache_dynamic_acls"
echo 64 > "${TARGET_IQN}/tpgt_0/attrib/default_cmdsn_depth"
echo 1 > "${TARGET_IQN}/tpgt_0/attrib/generate_node_acls"
echo 2 > "${TARGET_IQN}/tpgt_0/attrib/netif_timeout"
echo 15 > "${TARGET_IQN}/tpgt_0/attrib/login_timeout"

if [[ -n "${ISCSI_USER}${ISCSI_PASS}" ]]; then
	echo 1 > "${TARGET_IQN}/tpgt_0/attrib/authentication"
	echo -n "$ISCSI_USER" > "${TARGET_IQN}/tpgt_0/auth/userid"
	echo -n "$ISCSI_PASS" > "${TARGET_IQN}/tpgt_0/auth/password"
else
	# disable auth
	echo 0 > "${TARGET_IQN}/tpgt_0/attrib/authentication"
fi

echo "2048~65535" > "${TARGET_IQN}/tpgt_0/param/OFMarkInt"
echo "2048~65535" > "${TARGET_IQN}/tpgt_0/param/IFMarkInt"
echo "No" > "${TARGET_IQN}/tpgt_0/param/OFMarker"
echo "No" > "${TARGET_IQN}/tpgt_0/param/IFMarker"
echo "0" > "${TARGET_IQN}/tpgt_0/param/ErrorRecoveryLevel"
echo "Yes" > "${TARGET_IQN}/tpgt_0/param/DataSequenceInOrder"
echo "Yes" > "${TARGET_IQN}/tpgt_0/param/DataPDUInOrder"
echo "1" > "${TARGET_IQN}/tpgt_0/param/MaxOutstandingR2T"
echo "20" > "${TARGET_IQN}/tpgt_0/param/DefaultTime2Retain"
echo "2" > "${TARGET_IQN}/tpgt_0/param/DefaultTime2Wait"
echo "65536" > "${TARGET_IQN}/tpgt_0/param/FirstBurstLength"
echo "262144" > "${TARGET_IQN}/tpgt_0/param/MaxBurstLength"
echo "262144" > "${TARGET_IQN}/tpgt_0/param/MaxXmitDataSegmentLength"
echo "8192" > "${TARGET_IQN}/tpgt_0/param/MaxRecvDataSegmentLength"
echo "Yes" > "${TARGET_IQN}/tpgt_0/param/ImmediateData"
echo "Yes" > "${TARGET_IQN}/tpgt_0/param/InitialR2T"
echo "LIO Target" > "${TARGET_IQN}/tpgt_0/param/TargetAlias"
echo "1" > "${TARGET_IQN}/tpgt_0/param/MaxConnections"
echo "CRC32C,None" > "${TARGET_IQN}/tpgt_0/param/DataDigest"
echo "CRC32C,None" > "${TARGET_IQN}/tpgt_0/param/HeaderDigest"
echo "CHAP,None" > "${TARGET_IQN}/tpgt_0/param/AuthMethod"
popd

for i in $INITIATOR_IQNS; do
	mkdir -p "/sys/kernel/config/target/iscsi/${TARGET_IQN}/tpgt_0/acls/${i}" \
		|| _fatal
	pushd "/sys/kernel/config/target/iscsi/${TARGET_IQN}/tpgt_0/acls/${i}" \
		|| _fatal

	echo 64 > "cmdsn_depth"
	# per-initiator ACL auth needs explicit config. It's not inherited
	# from parent TPG.
	if [[ -n "${ISCSI_USER}${ISCSI_PASS}" ]]; then
		echo -n "$ISCSI_USER" > "auth/userid"
		echo -n "$ISCSI_PASS" > "auth/password"
	fi

	echo 0 > "attrib/random_r2t_offsets"
	echo 0 > "attrib/random_datain_seq_offsets"
	echo 0 > "attrib/random_datain_pdu_offsets"
	echo 30 > "attrib/nopin_response_timeout"
	echo 15 > "attrib/nopin_timeout"
	echo 0 > "attrib/default_erl"
	echo 5 > "attrib/dataout_timeout_retries"
	echo 3 > "attrib/dataout_timeout"

	mkdir -p "lun_0" || _fatal
	ln -s "/sys/kernel/config/target/iscsi/${TARGET_IQN}/tpgt_0/lun/lun_0" \
		"lun_0/cafecafe"
	echo 0 > "lun_0/write_protect"
	popd || _fatal
done

mkdir "/sys/kernel/config/target/iscsi/${TARGET_IQN}/tpgt_0/np/127.0.0.1:3260" \
	|| _fatal
echo 1 > "/sys/kernel/config/target/iscsi/${TARGET_IQN}/tpgt_0/enable" \
	|| _fatal

echo "$(uname -r) LIO Target ready at: iscsi://127.0.0.1:3260/${TARGET_IQN}/0"
