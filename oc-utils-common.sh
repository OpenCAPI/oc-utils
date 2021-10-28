# Copyright 2016, 2020 International Business Machines
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

version=1.1
# revision 1.1 adds a check for lock file per card to avoid resetting a card under programmation

log_file=/var/log/capi-utils.log
LockDirPrefix=/var/ocxl/locked_card

# Reset a card
function reset_card() {
  # Set return status
  ret_status=0
  # Timeout for reset
  reset_timeout=30
  reset_count=0
  # get number of cards in system
  n=`ls /dev/ocxl 2>/dev/null | wc -l`

  # if necessary, convert card name into slot name
  modprobe pnv-php	# required to access physical slot
  slot=$1
  if [[ ! -f /sys/bus/pci/slots/$slot/power ]]
  then
    if [[ -c /dev/ocxl/$slot ]]
    then
      slot=`printf "$slot\n" | awk -F"." '{ print $2 }' | sed s/$/.0/`
    fi
    slot=`lspci -m -v -s $slot | awk '/^PhySlot:/ { print $2; exit }'`
    if [[ -z $slot ]]
    then
      printf "$1: No such card or slot. Exiting.\n"
      exit 1
    fi
  fi

  [ -n "$3" ] && printf "$3\n" || printf "Preparing to reset card\n"
  [ -n "$4" ] && reset_timeout=$4
  printf "Resetting card $1: Reset! \n"
  printf 0 > /sys/bus/pci/slots/$slot/power
  printf 1 > /sys/bus/pci/slots/$slot/power
  while true; do
    if [[ `ls /dev/ocxl 2>/dev/null | wc -l` == "$n" ]]; then
      break
    fi 
    printf "."
    sleep 1
    reset_count=$((reset_count + 1))
    if [[ $reset_count -eq $reset_timeout ]]; then
      printf "${bold}ERROR:${normal} Reset timeout has occurred\n"
      ret_status=1
      break 
    fi
  done
  printf "\n"

  if [ $ret_status -ne 0 ]; then
    exit 1
  else
    printf "Reset complete\n"
  fi
}

# Reset a card and also Reload image
function reload_card() {
  # Set return status
  ret_status=0
  # Timeout for reset
  reset_timeout=30
  reset_count=0
  # get number of cards in system
  n=`ls /dev/ocxl 2>/dev/null | wc -l`

  # if necessary, convert card name into slot name
  modprobe pnv-php	# required to access physical slot
  slot=$1
  if [[ ! -f /sys/bus/pci/slots/$slot/power ]]
  then
    if [[ -c /dev/ocxl/$slot ]]
    then
      slot=`printf "$slot\n" | awk -F"." '{ print $2 }' | sed s/$/.0/`
    fi
    slot=`lspci -m -v -s $slot | awk '/^PhySlot:/ { print $2; exit }'`
    if [[ -z $slot ]]
    then
      printf "$1: No such card or slot. Exiting.\n"
      exit 1
    fi
  fi

  [ -n "$3" ] && printf "$3\n" || printf "Preparing to reset card\n"
  [ -n "$4" ] && reset_timeout=$4
# added by collin for image_reload
# tuned for the new slot naming scheme
  setpci -s `cat /sys/bus/pci/slots/$slot/address`.0 638.B=01
  printf "Resetting card $1: Image Reloading ... \n"

# subsys=$(lspci -s `cat /sys/bus/pci/slots/JP91NVB1/address`.0 -vvv |grep Subsystem |awk '{ print $NF }')
  subsys=$(lspci -s `cat /sys/bus/pci/slots/$slot/address`.0 -vvv |grep Subsystem |awk '{ print $NF }')
# adding specific code for 250SOC card (subsystem_id = 0x066A, former id was 0x060d for old cards)
  if [[ $subsys = @("066a"|"060d") ]]
  then
    # Unbinding to prevent driver to access the card before power down
    # TO DO : need to look for all existing /sys/bus/pci/slots/$slot/address`.X entries
    # for the time beiing we consider only 2 entries as implemented in https://github.com/OpenCAPI/OpenCAPI3.0_Client_RefDesign/
    echo  `cat /sys/bus/pci/slots/$slot/address`.0 > /sys/bus/pci/drivers/ocxl/unbind
    echo  `cat /sys/bus/pci/slots/$slot/address`.1 > /sys/bus/pci/drivers/ocxl/unbind
    
    setpci -s `cat /sys/bus/pci/slots/$slot/address`.0 634.B=11
    setpci -s `cat /sys/bus/pci/slots/$slot/address`.0 630.L=00020000
  fi
  printf 0 > /sys/bus/pci/slots/$slot/power
  if ! printf 1 > /sys/bus/pci/slots/$slot/power 2> /dev/null
  then
    echo ">> Card can not power-on. Reboot or power-cycle needed for re-enumeration"
    exit 1
  fi
  while true; do
    if [[ `ls /dev/ocxl 2>/dev/null | wc -l` == "$n" ]]; then
      break
    fi 
    printf "."
    sleep 1
    reset_count=$((reset_count + 1))
    if [[ $reset_count -eq $reset_timeout ]]; then
      printf "${bold}ERROR:${normal} Reset timeout has occurred\n"
      ret_status=1
      break 
    fi
  done
  printf "\n"

  if [ $ret_status -ne 0 ]; then
    exit 1
  else
    printf "Image Reload complete\n"
  fi
}


# stop on non-zero response
set -e

# output formatting
( [[ $- == *i* ]] && bold=$(tput bold) ) || bold=""
( [[ $- == *i* ]] && normal=$(tput sgr0) ) || normal=""

# make sure script runs as root
if [[ $EUID -ne 0 ]]; then
  printf "${bold}ERROR:${normal} This script must run as root (${EUID})\n"
  exit 1
fi

