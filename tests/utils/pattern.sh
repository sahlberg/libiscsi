#/bin/bash 

size=$1
blk_sz=$((16*1024))
start_seek_offset=$((3 * 65536))
blocks=$((2 * $size/$blk_sz)) 
for ((i=0;i<$blocks;i=i+2))
do
        #echo "Write:$i"
	seek=$(($start_seek_offset + $i))
        dd if=/dev/urandom of=/dev/sdf count=1 bs=$blk_sz seek=$seek oflag=direct 2>/dev/null 1>/dev/null
        #dd if=/dev/urandom of=/dev/sdf count=1 bs=$blk_sz seek=$i oflag=direct
done
