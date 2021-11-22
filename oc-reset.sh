#!/bin/bash
#
# Copyright 2016, 2017 International Business Machines
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# get capi-utils root
[ -h $0 ] && package_root=`ls -l "$0" |sed -e 's|.*-> ||'` || package_root="$0"
package_root=$(dirname $package_root)
# echo "DEBUG : Package_root is : $package_root"
source $package_root/oc-utils-common.sh

bold=\\033[1m
green=\\033[32m
blue=\\033[34m
red=\\033[31m
normal=\\033[0m

program=`basename "$0"`
mylock=0  # var used to remove lock dir only if we created it

# Print usage message helper function
function usage() {
  echo "Usage:  sudo ${program} [OPTIONS]"
  echo "    [-C <card>] card to reset."
  echo "      Example: if you want to reset card"
  echo -e "        ${green} IBM,oc-snap.0004:00:00.1.0 ${normal}"
  echo "      Command line should be:"
  echo -e "         ${green}sudo ./oc-reset.sh -C IBM,oc-snap.0004:00:00.1.0 ${normal}"
  echo "      Or:"
  echo -e "         ${green}sudo ./oc-reset.sh -C 0004:00:00.0 ${normal}"
  echo "      Or:"
  echo -e "         ${green}sudo ./oc-reset.sh -C 4 ${normal}"
  echo "    [-V] Print program version (${version})"
  echo "    [-h] Print this help message."
  echo
  echo "Utility to reset CAPI FPGA cards."
  echo "Please ensure that you are aiming to reset the target FPGA Card."
  echo "Pls notify other users who you work with them on the same server."
  echo
}

# Select function for FPGA Cards Selection
function select_cards() {
    # print current date on server for comparison
    printf "\n${bold}Current date:${normal}$(date)\n"

    # get number of cards in system
    n=`ls /dev/ocxl 2>/dev/null | wc -l`
    printf "${bold}  $n OpenCAPI cards found.${normal}\n"

    # Find all OC cards in the system
    allcards=`ls /dev/ocxl 2>/dev/null | awk -F"." '{ print $2 }' | sed s/$/.0/ | sort`
    allcards_array=($allcards)

    # print card information
    i=0;
    slot_enum=""
    delimiter="|"
    while read d ; do
      p[$i]=$(cat /sys/bus/pci/devices/${allcards_array[$i]}/subsystem_device)
      # translate the slot number string to a hexa number
      card_slot_hex=$(printf '%x' "0x${allcards_array[$i]::-8}")
      # build a slot_enum of all card numbers and use in the test menu to test user input
      if [ -z "$slot_enum" ]; then
         slot_enum=$card_slot_hex
      else
         slot_enum=$slot_enum$delimiter$card_slot_hex
      fi

      # find in oc-devices files the card corresponding to the slot to display the informations
      while IFS='' read -r line || [[ -n $line ]]; do
        if [[ ${line:0:6} == ${p[$i]:0:6} ]]; then
          parse_info=($line)
          board_vendor[$i]=${parse_info[1]}
          printf "${bold} Card %s:${normal} %s - %s \n" "$card_slot_hex" "${allcards_array[$i]}" "${board_vendor[$i]}"
        fi
      done < "$package_root/oc-devices"
      i=$[$i+1]
    done < <( lspci -d "1014":"062b" -s .1 )
    printf "\n"

    # prompt card until input is in list of available slots
    while ! [[ "$c" =~ ^($slot_enum)$ ]]
    do
        echo -e "  ${green}Which card number do you want to reset? [${bold}$slot_enum${normal}]: \c" | sed 's/|/-/g'
        read -r c
     done
    printf "\n"

}

# Parse any options given on the command line
while getopts ":C:Vh" opt; do
  case ${opt} in
      C)
      card=$OPTARG
      ;;
      V)
      echo "${version}" >&2
      exit 0
      ;;
      h)
      usage;
      exit 0
      ;;
      \?)
      printf "${bold}${red}ERROR:${normal} Invalid option: -${OPTARG}\n" >&2
      exit 1
      ;;
      :)
      printf "${bold}${red}ERROR:${normal} Option -$OPTARG requires an argument.\n" >&2
      exit 1
      ;;
  esac
done

shift $((OPTIND-1))

ulimit -c unlimited

# check if OPENCAPI boards exists
# we get number of cards in system
ocapi_check=`ls /dev/ocxl 2>/dev/null | wc -l`
if [ $ocapi_check -eq 0 ]; then
  printf "${bold}${red}ERROR:${normal} No OpenCAPI devices found\n"
  exit 1
fi

if [ -n "$card" ]; then
        if [[ $card  != *":"* ]]; then
           # if card argument is just the card slot (with no :) then add the necessary stuff around it
           card=$(printf '%.4x:00:00.0' "0x${card}")
        fi
else
        select_cards


	# Convert the slot number into a 000x:00:00.0 slot number
        card=$(printf '%.4x:00:00.0' "0x${c}")
fi



	#echo "card selected is : $card"
        echo -e "${blue}Checking if card $card is locked${normal}"
#	echo "DEBUG : LockDirPrefix is $LockDirPrefix"
        LockDir="$LockDirPrefix$card"
#	echo "DEBUG : LockDir is $LockDir"
        # make LockDir if not present
        # mutual exclusion
        if mkdir $LockDir 2>/dev/null; then
		echo -e "${blue}$LockDir created during oc-reset${normal}"
		trap 'rm -rf "$LockDir";echo -e "${blue}$LockDir removed${normal}"' EXIT # This prepares a cleaning of the newly created dir
					      # when script will output
       else
		echo
		printf "${bold}${red}ERROR:${normal} $LockDir is already existing\n"
		printf " => Card has been locked already (by oc-flash-script or oc-reset)\n"
 		exit 10
	fi




reset_card $card factory " Resetting OpenCAPI card in slot $card"

