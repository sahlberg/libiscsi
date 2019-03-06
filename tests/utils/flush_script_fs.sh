#!/bin/bash
while true
do 
	rm /tmp/before /tmp/after
	dd if=/dev/zero of=/dev/sdc count=10 bs=1M oflag=direct

	umount /mnt
	echo y | mkfs.ext4 /dev/sdf
	mount /dev/sdf /mnt 
	
	cd /mnt 
	cp -rf /lib/* .
	mkdir a; cp -rf /etc/* a/
	cd -
	
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
	
done 
