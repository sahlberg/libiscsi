#!/bin/bash
while true
do
	rm /tmp/before /tmp/after
	dd if=/dev/zero of=/dev/sdc count=10 bs=1M oflag=direct
	#bash pattern.sh
	a=$(( ( RANDOM % 5 )  + 1 ))
	size=$(($a * 100))

	echo "Size is : $size"
	umount /mnt
	echo "y" | mkfs.ext3 /dev/sdf 
	mount /dev/sdf /mnt 
	dd if=/dev/urandom of=/mnt/file1 count=$size bs=1M oflag=direct
	umount /mnt 

	mount /dev/sdf /mnt
	cd /mnt 
	find . -exec md5sum {} \; > /tmp/before
	cd -
	umount /mnt 

	python3 flush_start.py
	sleep 300
	python3 flush_status.py

	mount /dev/sdc /mnt 
	cd /mnt 
	find . -exec md5sum {} \; > /tmp/after
	cd -
	umount /mnt

	diff /tmp/before /tmp/after
	if [ $? -ne 0 ]; then	
		echo "Mismatch in checksum...."
		break
	fi

	>/tmp/stord.ERROR
	>/tmp/stord.INFO
	>/tmp/stord.WARNING
	ssh root@192.168.2.24 "bash -x aero_clean.sh"
	sleep 60
done
