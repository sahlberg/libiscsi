#!/bin/bash

IRQS=$(cat /proc/interrupts | egrep 'ens192' | awk '{print $1}' | sed 's/://')

cores=($(seq 1 $(nproc)))

i=0

for IRQ in $IRQS

do

  core=${cores[$i]}

  let "mask=2**(core-1)"

  echo $(printf "%x" $mask) > /proc/irq/$IRQ/smp_affinity

  let "i+=1"

  if [[ $i == ${#cores[@]} ]]; then i=0

  fi

done
