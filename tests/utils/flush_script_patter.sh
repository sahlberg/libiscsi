#!/bin/bash
while true
do
	dd if=/dev/zero of=/dev/sdc count=10 bs=1M oflag=direct
	a=$(( ( RANDOM % 5 )  + 1 ))
	size=$(($a * 1024 * 1024))
	bash pattern.sh $size

	>/tmp/stord.ERROR
	>/tmp/stord.INFO
	>/tmp/stord.WARNING
	ssh root@192.168.2.24 "bash -x aero_clean.sh"
	sleep 60
done
