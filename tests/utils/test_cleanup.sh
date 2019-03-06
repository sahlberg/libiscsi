#!/bin/bash
TargetName="tgt1"
#TargetName="192168111242315bd768368ede80f4ff75bee0b88cubuntuvm"
TargetIP=`ip route get 8.8.8.8 | sed -n '/src/{s/.*src *//p;q}'`
#Cleanup 
iscsiadm -m node --logout
iscsiadm -m node -o delete
tgtadm --lld iscsi --op show --mode target | grep "Target" | grep $TargetName
if [ $? -ne 0 ]; then
	echo "Targetname $TargetName does not exist.."
	exit 0
fi

python3 test_cleanup.py
lsblk
