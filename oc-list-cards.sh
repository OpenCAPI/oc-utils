#!/bin/bash

#check for lspci
lspci >/dev/null 2>&1
[ $? -ne 0 ] && printf "lspci not found. Please make sure it is installed and can be found in the PATH environment variable.\n" >&2 && exit 1

[ -h $0 ] && package_root=`ls -l "$0" |sed -e 's|.*-> ||'` || package_root="$0"
package_root=$(dirname $package_root)

# print table header
printf "${bold}%-21s %-29s %-29s %-20s %s${normal}\n" "#" "Card" "Flashed" "by" "Last Image"
# Find all OC cards in the system
allcards=`ls /dev/ocxl 2>/dev/null | awk -F"." '{ print $2 }' | sed s/$/.0/ | sort`
allcards_array=($allcards)

# print card information and flash history
i=0;
while read d ; do
  p[$i]=$(cat /sys/bus/pci/devices/${allcards_array[$i]}/subsystem_device)
  f=$(cat /var/ocxl/card$i)
  while IFS='' read -r line || [[ -n $line ]]; do
    if [[ ${line:0:6} == ${p[$i]:0:6} ]]; then
      parse_info=($line)
      board_vendor[$i]=${parse_info[1]}
      fpga_type[$i]=${parse_info[2]}
      flash_partition[$i]=${parse_info[3]}
      flash_block[$i]=${parse_info[4]}
      flash_interface[$i]=${parse_info[5]}
      flash_secondary[$i]=${parse_info[6]}
      printf "%-20s %-30s %-29s %-20s %s\n" "card$i:${allcards_array[$i]}" "${line:6:21}" "${f:0:29}" "${f:30:20}" "${f:51}"
    fi
  done < "$package_root/oc-devices"
  i=$[$i+1]
done < <( lspci -d "1014":"062b" -s .1 )
