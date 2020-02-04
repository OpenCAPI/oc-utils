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
source $package_root/oc-utils-common.sh

program=`basename "$0"`

# Print usage message helper function
function usage() {
  echo "Usage:  sudo ${program} [OPTIONS]"
  echo "    [-C <card>] card to reset."
  echo "      Example: if you want to reset card" 
  echo -e "        \033[33m IBM,oc-snap.0004:00:00.1.0 \033[0m"
  echo "      Command line should be:"
  echo -e "        \033[33m sudo ./oc-reset.sh -C IBM,oc-snap.0004:00:00.1.0 \033[0m"
  echo "      Or:"
  echo -e "        \033[33m sudo ./oc-reset.sh -C 0004:00:00.0 \033[0m"
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
    n=`ls -d /sys/class/ocxl/IBM* | awk -F"/sys/class/ocxl/" '{ print $2 }' | wc -w`
    printf "$n cards found.\n"

    # Find all OC cards in the system
    allcards=`ls -d -1 /sys/class/ocxl/IBM* |grep "/sys/class/ocxl/IBM," | awk -F"." '{ print $2 }' | sed s/$/.0/ | sort`
    allcards_array=($allcards)

    # print card information
    i=0;
    while read d ; do
	card_info=`ls -d -1 /sys/class/ocxl/IBM*${allcards_array[$i]:0:4}*`
	printf "Card$i: ${card_info:16} \n"
	i=$[$i+1]
    done < <( lspci -d "1014":"062b" -s .1 )
    printf "\n"

    # prompt card to flash to
    while true; do
        read -p "Which card do you want to reset? [0-$(($n - 1))] " c
        if ! [[ $c =~ ^[0-9]+$ ]]; then
            printf "${bold}ERROR:${normal} Invalid input\n"
        else
            c=$((10#$c))
            if (( "$c" >= "$n" )); then
                printf "${bold}ERROR:${normal} Wrong card number\n"
                exit 1
            else
                break
            fi
        fi
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
      printf "${bold}ERROR:${normal} Invalid option: -${OPTARG}\n" >&2
      exit 1
      ;;
      :)
      printf "${bold}ERROR:${normal} Option -$OPTARG requires an argument.\n" >&2
      exit 1
      ;;
  esac
done

shift $((OPTIND-1))

ulimit -c unlimited

# check if CAPI boards exists
capi_check=`ls -d /sys/class/ocxl/IBM* | awk -F"/sys/class/ocxl/" '{ print $2 }' | wc -w`
if [ $capi_check -eq 0 ]; then
  printf "${bold}ERROR:${normal} No CAPI devices found\n"
  exit 1
fi

if [ -n "$card" ]; then
        reset_card $card factory "Resetting OpenCAPI Adapter $card"
else
        select_cards
        # Find all OC cards in the system
        n=`ls -d /sys/class/ocxl/IBM* | awk -F"/sys/class/ocxl/" '{ print $2 }' | wc -w`
	if (($c < 0 )) || (( "$c" >= "$n" )); then
            printf "${bold}ERROR:${normal} Wrong card number ${c}\n"
            exit 1
        fi
        reset_card ${allcards_array[$c]} factory "Resetting OpenCAPI Adapter ${allcards_array[$c]}"
fi


