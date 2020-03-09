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
  echo "    [-C <card>] card to reload."
  echo "      Example: if you want to reload card"
  echo -e "        \033[33m IBM,oc-snap.0004:00:00.1.0 \033[0m"
  echo "      Command line should be:"
  echo -e "        \033[33m sudo ./oc-reload.sh -C IBM,oc-snap.0004:00:00.1.0 \033[0m"
  echo "      Or:"
  echo -e "        \033[33m sudo ./oc-reload.sh -C 0004:00:00.0 \033[0m"
  echo "    [-V] Print program version (${version})"
  echo "    [-h] Print this help message."
  echo
  echo "Utility to reload FPGA image from the FPGA Flash."
  echo "Pls notify other users who you work with them on the same server."
  echo
}

# Select function for FPGA Cards Selection
function select_cards() {
    # print current date on server for comparison
    printf "\n${bold}Current date:${normal}$(date)\n"
    
    # get number of cards in system
    n=`ls /dev/ocxl 2>/dev/null | wc -l`
    printf "$n cards found.\n"

    # Find all OC cards in the system
    allcards=`ls /dev/ocxl 2>/dev/null | awk -F"." '{ print $2 }' | sed s/$/.0/ | sort`
    allcards_array=($allcards)

    # print card information
    i=0;
    while read d ; do
	card_info=`ls /dev/ocxl/*${allcards_array[$i]:0:4}*`
	printf "Card$i: ${card_info##*/} \n"
	i=$[$i+1]
    done < <( lspci -d "1014":"062b" -s .1 )
    printf "\n"

    # prompt card to flash to
    while true; do
        read -p "Which card do you want to reload image from FLASH? [0-$(($n - 1))] " c
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

# OPTIND Reset done in order to use getopts even if not the first time getopts is called (when sourcing this script by oc-flash-script.sh for example)
OPTIND=1

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
capi_check=`ls /dev/ocxl 2>/dev/null | wc -l`
if [ $capi_check -eq 0 ]; then
  printf "${bold}ERROR:${normal} No CAPI devices found\n"
  exit 1
fi

if [ -n "$card" ]; then
        reload_card $card factory "Image Reloading for OpenCAPI Adapter $card"
else
        select_cards
        # Find all OC cards in the system
        n=`ls /dev/ocxl 2>/dev/null | wc -l`
	if (($c < 0 )) || (( "$c" >= "$n" )); then
            printf "${bold}ERROR:${normal} Wrong card number ${c}\n"
            exit 1
        fi
        reload_card ${allcards_array[$c]} factory "Image Reloading for OpenCAPI Adapter ${allcards_array[$c]}"
fi


