#!/bin/bash
TargetName="tgt1"
#TargetName="192168111242315bd768368ede80f4ff75bee0b88cubuntuvm"
TargetIP="192.168.5.200"
#Cleanup 
iscsiadm -m node --logout
iscsiadm -m node -o delete

tgtadm --lld iscsi --op show --mode target | grep "Target" | grep $TargetName
if [ $? -eq 0 ]; then
	echo "Targetname $TargetName already exists.."
	exit 0
fi

python3 post_cleanup.py

#Discovey on local host
iscsiadm --mode discovery --type sendtargets --portal $TargetIP
iscsiadm -m node -T $TargetName --login

#Turn off Read Ahead
#echo 0 > /sys/devices/platform/host34/session2/target34:0:0/34:0:0:1/block/sdd/queue/read_ahead_kb
#tgtadm --lld iscsi --mode logicalunit --op delete --tid=1 --lun=1
#tgtadm --lld iscsi --mode target --op delete --tid=1 --force
#iscsiadm -m session -P3

lsblk
