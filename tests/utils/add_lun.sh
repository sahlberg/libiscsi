#!/bin/bash
TargetName="tgt1"
TargetIP="192.168.5.138"

tgtadm --lld iscsi --op show --mode target | grep "Target" | grep $TargetName
if [ $? -ne 0 ]; then
	echo "Targetname $TargetName does not exist.."
	exit 0
fi

python3 add_lun.py
iscsiadm -m session --rescan
sleep 5
lsblk
