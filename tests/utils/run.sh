#!/bin/bash
python3 nep.py
bash -x test_tgt.sh
bash -x test_cleanup.sh
python3 nep_post_cleanup.py
python3 test_tgt_post_cleanup.py 

TargetName="tgt1"
TargetIP="192.168.5.138"
#Discovey on local host
iscsiadm --mode discovery --type sendtargets --portal $TargetIP
iscsiadm -m node -T $TargetName --login
