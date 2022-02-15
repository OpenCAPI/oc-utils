#!/bin/bash
#
# Copyright 2016, 2021 International Business Machines
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
# Usage: sudo oc-flash-script.sh <path-to-bin-file>

# Changes History:
# V2.0 code cleaning
# V2.1 reduce lines printed to screen (elasped times)
# V2.2 test if binary image is a capi2 image and correct printf error
# V2.3 adding 250SOC specific code
# V2.31 repaired the 4 bytes mode for 9H3
# V3.00 reordering the slot numbering
# V4.00 integrating the Partial reconfiguration
# V4.1  introducing a per card lock mechanism

# Exit codes:
#  echo "Exit codes : 0  : OK"
#  echo "           : 1  : argument issue"
#  echo "           : 2  : card or slot issue"
#  echo "           : 3  : file name doesn't match card name"
#  echo "           : 4  : dynamic code doesn't match static code"
#  echo "           : 5  : Utility capi-flash not found
#  echo "           : 10 : a card was locked by another process"


###################################################################################################################
# get capi-utils root directory & source oc-utils-common.sh

[ -h $0 ] && package_root=`ls -l "$0" |sed -e 's|.*-> ||'` || package_root="$0"
package_root=$(dirname $package_root)
source $package_root/oc-utils-common.sh


###################################################################################################################
# Variables

tool_version=4.1

bold=$(tput bold)
blue=$(tput setaf 4)
red=$(tput setaf 1)
green=$(tput setaf 2)
normal=$(tput sgr0)

force=0
automation=0
program=`basename "$0"`
card=-1

flash_address=""
flash_address2=""
flash_block_size=""
flash_type=""

reset_factory=0


###################################################################################################################
# Print startup infos

printf "\n"
printf "oc-flash_script version is $tool_version\t - "
printf "Tool compiled on: "
ls -l $package_root/oc-flash|cut -d ' ' -f '6-8' # date of last tool compilation
echo "_________________________________________________________________________${bold} ${green}"
echo "            ____                   _________    ____  ____               "
echo "           / __ \____  ___  ____  / ____/   |  / __ \/  _/               "
echo "          / / / / __ \/ _ \/ __ \/ /   / /| | / /_/ // /                 "
echo "         / /_/ / /_/ /  __/ / / / /___/ ___ |/ ____// /                  "
echo "         \____/ .___/\___/_/ /_/\____/_/  |_/_/   /___/                  ${blue}"
echo "   ___       /_/                           _             __            __"
echo "  / _ \_______  ___ ________ ___ _  __ _  (_)__  ___ _  / /____  ___  / /"
echo " / ___/ __/ _ \/ _ '/ __/ _ '/  ' \/  ' \/ / _ \/ _ '/ / __/ _ \/ _ \/ / "
echo "/_/  /_/  \___/\_, /_/  \_,_/_/_/_/_/_/_/_/_//_/\_, /  \__/\___/\___/_/  "
echo "              /___/                            /___/                     ${normal}"
echo "_________________________________________________________________________"


###################################################################################################################
# Functions

# Print usage message helper function
function usage() {
  echo ""
  echo "Example  : sudo ${program}  xxx_primary.bin xxx_secondary.bin"
  echo "or for PR: sudo ${program}  xxx_partial.bin"
  echo ""

  echo "Usage:  sudo ${program} [OPTIONS]"
  echo "    [-C <card>] card to flash."
  echo "    [-f] force execution without asking."
  echo "         warning: use with care e.g. for automation."
  echo "    [-a] automated mode, prevents answering questions."
  echo "         warning: exits if errors detected." 
 # echo "    [-r] Reset adapter to factory before writing to flash."
  echo "    [-V] Print program version (${version})"
  echo "    [-h] Print this help message."
  echo "    <path-to-bin-file>"
  echo "    <path-to-secondary-bin-file> (Only for SPIx8 device)"
  echo
  echo "Utility to flash/write bitstreams to OpenCAPI FPGA cards."
  echo "Please ensure that you are using the right bitstream data."
  echo "Using non-functional bitstream data or aborting the process"
  echo "can leave your card in a state where a hardware debugger is"
  echo "required to make the card usable again."
  echo
}


###################################################################################################################
# Parse any options given on the command line

while getopts ":C:faVhr" opt; do
  case ${opt} in
# we kept C as option name to avoid changing existing scripts, but "C" now represents the slot number
# when provided it will be converted temporarilly to a card relative position to maintain
# compatibility
# The ultimate goal is to switch to slot number everywhere in this script
      C)
      card=$OPTARG
      paramcard=1
      ;;
      f)
      force=1
      ;;
      a)
      automation=1
      ;;      
      r)
      printf "${bold}Warning:${normal} Factory/user reset option is unavailable in OC, ignoring -r option\n" >&2
      reset_factory=0
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

# now working with the next given parameters that should be the image files ($@)
shift $((OPTIND-1))


###################################################################################################################
# Main

ulimit -c unlimited

#------------------------------------------------------------------------------------------------------------------
# make sure an input argument (image file) is provided
if [ $# -eq 0 ]; then
  printf "${bold}${red}ERROR:${normal} Input argument missing\n"
  usage
  exit 1
fi

#------------------------------------------------------------------------------------------------------------------
# make sure the input file exists
if [[ ! -e $1 ]]; then
  printf "${bold}${red}ERROR:${normal} $1 not found\n"
  usage
  exit 1
fi

#------------------------------------------------------------------------------------------------------------------
# check if some OpenCAPI boards exist
capi_check=`ls /dev/ocxl 2>/dev/null | wc -l`
if [ $capi_check -eq 0 ]; then
  printf "${bold}${red}ERROR:${normal} No OpenCAPI devices found\n"
  exit 1
fi

printf "\n"

#------------------------------------------------------------------------------------------------------------------
# get number of cards in system
n=`ls /dev/ocxl 2>/dev/null | wc -l`
printf " In this server:  ${bold}$n${normal} OpenCAPI card(s) found."
# touch history files if not present
for i in `seq 0 $(($n - 1))`; do
  f="/var/ocxl/card$i"
  if [[ ! -f $f ]]; then
    touch $f
  fi
done

#------------------------------------------------------------------------------------------------------------------
# print current date on server for comparison
printf "\n${bold} Current date is ${normal}$(date)\n\n"

#------------------------------------------------------------------------------------------------------------------
# print table header
printf " Following logs show last programming files (except if hardware or capi version has changed):\n"
printf "${bold}%-7s %-35s %-29s %-20s %s${normal}\n" " #" "Card slot and name" "Flashed" "by"

#------------------------------------------------------------------------------------------------------------------
# Find all OC cards in the system
allcards=`ls /dev/ocxl 2>/dev/null | awk -F"." '{ print $2 }' | sed s/$/.0/ | sort`
if [ -z "$allcards" ]; then
	echo "No OpenCAPI cards found.\n"
	exit 2
fi

allcards_array=($allcards)

#------------------------------------------------------------------------------------------------------------------
# Get found cards information and flash history and print it
i=0;
slot_enum=""
delimiter="|"

# list of ".1" slots returned by lspci containing "062b"
# Getting generic informations (vendor, flash, etc) from oc-devices file    
while read d ; do
  #extract the subsystem_device id in order to know the board id
	p[$i]=$(cat /sys/bus/pci/devices/${allcards_array[$i]}/subsystem_device)
	# translate the slot number string to a hexa number
  card_slot_hex=$(printf '%x' "0x${allcards_array[$i]::-8}")
	# build a slot_enum of all card numbers and use it in the test menu to test user input
	if [ -z "$slot_enum" ]; then
		slot_enum=$card_slot_hex
	else
		slot_enum=$slot_enum$delimiter$card_slot_hex
	fi
      
	f=$(cat /var/ocxl/card$i)
  while IFS='' read -r line || [[ -n $line ]]; do
	 	if [[ ${line:0:6} == ${p[$i]:0:6} ]]; then
		  	parse_info=($line)
		  	board_vendor[$i]=${parse_info[1]}
		  	fpga_manuf[$i]=${parse_info[2]}
		  	flash_partition[$i]=${parse_info[3]}
		  	flash_block[$i]=${parse_info[4]}
		  	flash_interface[$i]=${parse_info[5]}
		  	flash_secondary[$i]=${parse_info[6]}
		  	component_list=(${line:6:23})
		  	bin_list=(${f:51})
        #display Card number : slot - Card name - date -name of last programming registered in file
		  	printf "${bold}%-8s${normal} %-22s %-29s %-20s \n" " Card $card_slot_hex: ${allcards_array[$i]}" "${component_list[0]}" "${f:0:29}" "${f:30:20}"
        #display the 2 names of bin files
      	if [ ! -z ${bin_list[1]} ]; then
	    	  printf "\t%s \n\t%s\n" "${bin_list[0]}"  "${bin_list[1]}"
			  else
	  	  printf "\t%s \n" "${bin_list[0]}"
			  fi
	  	echo ""
	  fi
  done < "$package_root/oc-devices"

  i=$[$i+1]
done < <( lspci -d "1014":"062b" -s .1 )

printf "\n"

#------------------------------------------------------------------------------------------------------------------
# card is set via parameter since it is positive (otherwise default to -1)
# $card parameter when provided needs to be the slot number
# we translate it to card position in the old numbering way (eg: 0 to n-1)
if [ ! -z $paramcard ]; then
	# Assign C to 4 digits hexa
  #	card4=`printf "%04x" $card`
	card4=$(printf '%04x' "0x${card}")
	echo "Slot is: $card4"
	echo $allcards
	# search for card4 occurence and get line number in list of slots
	ln=$(grep  -n ${card4} <<<$allcards| cut -f1 -d:)
	
	if [ -z $ln ]; then
		echo "Requested slot $card4 can't be found among :"
		echo $allcards
		exit 2
	else
		ln=$(grep  -n ${card4} <<<$allcards| cut -f1 -d:)
		# echo "Corresponding slot is found at position: $ln"
		# Calculate the position number from line number to use script in the old way
		c=$(($ln - 1))
		#echo Card is: card$c
	fi
else
  # prompt card to flash to
  #  while true; do
  #    read -p "  Which card do you want to flash? [0-$(($n - 1))] " c
  #    if ! [[ $c =~ ^[0-9]+$ ]]; then
  #      printf "${bold}${red}ERROR:${normal} Invalid input\n"
  #    else
  #      c=$((10#$c))
  #      if (( "$c" >= "$n" )); then
  #        printf "${bold}${red}ERROR:${normal} Wrong card number\n"
  #        exit 1
  #      else
  #        break
  #      fi
  #    fio
  #  done

  # prompt card until input is in list of available slots
  while ! [[ "$c" =~ ^($slot_enum)$ ]]
  do
    echo -e "  ${green}Which card number do you want to flash? [${bold}$slot_enum${normal}]: \c" | sed 's/|/-/g'
    read -r c
  done
  printf "\n"

  card4=$(printf '%04x' "0x${c}")
  #echo "Slot is: $card4"
  # search for card4 occurence and get line number in list of slots
  ln=$(grep  -n ${card4} <<<$allcards| cut -f1 -d:)
  c=$(($ln - 1))

fi

#printf "\n"

#------------------------------------------------------------------------------------------------------------------
# check file type (looking at file extension and card manufacturer)
PR_mode=0
FILE_EXT=${1##*.}
if [[ ${fpga_manuf[$c]} == "Altera" ]]; then
  if [[ $FILE_EXT != "rbf" ]]; then
    printf "${bold}${red}ERROR: ${normal}Wrong file extension: .rbf must be used for boards with Altera FPGA\n"
    exit 1
  fi
elif [[ ${fpga_manuf[$c]} == "Xilinx" ]]; then
  if [[ $FILE_EXT != "bin" ]]; then
    printf "${bold}${red}ERROR: ${normal}Wrong file extension: .bin must be used for boards with Xilinx FPGA\n"
    exit 1
  fi
else
  printf "${bold}${red}ERROR: ${normal}Card not listed in oc-devices or previous card failed or is not responding\n"
  exit 1
fi

#------------------------------------------------------------------------------------------------------------------
# Get flash address and block size
# Decide if Partial Reconfiguration or not (looking at file name)
if [ -z "$flash_address" ]; then
  flash_address=${flash_partition[$c]}
  if [[ $1 =~ "_partial" ]]
  then
     #printf "Partial Reconfiguration image has been detected.\n"
     PR_mode=1
  fi
  else if [[ $1 =~ "fw_" ]]
  then
     printf "===================================================================================\n"
     echo "NOTE : You are in the process of programming a CAPI2 image in FACTORY area!"
     echo "       A reboot or power cycle will be needed to re-enumerate the cards."
     echo "       You may need to then switch your card back to USER area (capi-reset <card_nb> user)"
     printf "===================================================================================\n"
  fi
fi
if [ -z "$flash_block_size" ]; then
  flash_block_size=${flash_block[$c]}
fi
if [ -z "$flash_type" ]; then
  flash_type=${flash_interface[$c]}
fi
if [ -z "$flash_type" ]; then
  flash_type="BPIx16" #If it is not listed in oc-device file, use default value
fi
if [ $PR_mode == 1 ]; then
  flash_type="PR_SPIx8" #if PR mode then overide the flash_type setting to consider it as a 
fi

#------------------------------------------------------------------------------------------------------------------
# Deal with the second argument (the secondary bin file)
if [ $flash_type == "SPIx8" ]; then
  if [ $# -eq 1 ]; then
    printf "${bold}${red}ERROR:${normal} Input argument missing for SPIx8 card (or bad card selected)\n"
    printf "       The device you have selected requires both primary and secondary bin files\n"
    bdf=`echo ${allcards_array[$c]}`
    #echo $bdf
    usage
    exit 1
  fi

  #Check the second file existence
  if [[ ! -e $2 ]]; then
    printf "${bold}${red}ERROR:${normal} $2 not found\n"
    usage
    exit 1
  fi

  #Assign secondary address
  flash_address2=${flash_secondary[$c]}
  if [ -z "$flash_address2" ]; then
    printf "${bold}${red}ERROR:${normal} The second address must be assigned in file oc-device\n"
    exit 1
  fi
fi

#------------------------------------------------------------------------------------------------------------------
# card is set via parameter since it is positive
#if (($force != 1)); then
  # prompt to confirm
while true; do
  printf " ${bold}INFO:${normal} It is ${bold}highly recommended ${normal}to CLOSE all JTAG tools (SDK, hardware_manager) before programming\n";
  printf "      This could create a conflict, lose access to the card and force you to reboot the server!\n\n";

  #------------------------------------------------------------------------------------------------------------------
  # We extract the card name (such as "AD9V3") from the provided bin file names $1 and $2 (primary and secondary) and check they are consistent
  file_to_program=`echo $1 |awk -F 'oc_20' '{ print $2 }' | awk -F 'OC-' '{ print $2 }'|awk -F '_'  '{ print $1 }'`
 	if [ $flash_type == "SPIx8" ]; then
  	file_to_program2=`echo $2 |awk -F 'oc_20' '{ print $2 }' | awk -F 'OC-' '{ print $2 }'|awk -F '_'  '{ print $1 }'`
	  if [[ ${file_to_program} !=  ${file_to_program2} ]]; then
		  printf "\n>>>=================================================================================<<<\n"
	  	printf ">>> ${bold}${red}ERROR:${normal} Inconsistency between primary ${bold}${file_to_program}${normal} and secondary ${bold}${file_to_program2}${normal} selected boards!!\n"
		  printf ">>>=================================================================================<<<\n"
		  exit 3    # we exit whenever the primary and secondary file are board inconsistent
	  fi
  fi

  #------------------------------------------------------------------------------------------------------------------
  # We display the card name we want to program (such as "AD9V3")
  card_to_program=`echo  ${board_vendor[$c]} |awk -F 'OC-' '{ print $2 }'|awk -F '('  '{ print $1 }'`
  echo "card_to_program=${card_to_program}"

  #------------------------------------------------------------------------------------------------------------------
  # We display a summary about what we're going to do: Partial Reconfig or not, SPIx8 (primary and secondary files) or not, etc
  if [ $PR_mode == 1 ]; then
    printf " You have asked to ${bold}dynamically${normal} program the ${bold}${card_to_program}${normal} board in slot ${bold}$card4${normal} with :\n     ${bold}$1${normal}\n" 
  else
    printf " You have asked to ${bold}flash${normal} the ${bold}${card_to_program}${normal} board in slot ${bold}$card4${normal} with:\n     ${bold}$1${normal}\n" 
  fi
  if [ $flash_type == "SPIx8" ]; then
    printf " and ${bold}$2${normal}\n" 
  fi

  #------------------------------------------------------------------------------------------------------------------
  # Testing if bin images have been made for card type (AD9V3 bin files for AD9V3 card for example)
  
  # Bin images have NOT been made for card type (for example AD9H7 bin images while card type is AD9V3)-> NOT Good !
  if [[ ${file_to_program} !=  ${card_to_program} ]]; then 
	  printf "\n>>>=================================================================================<<<\n"
	  printf ">>> ${bold}${red}ERROR:${normal} You have chosen to program a ${bold}${card_to_program}${normal} board with a file built for a ${bold}${file_to_program}${normal}!!\n"
	  printf ">>> You may crash and lose your card if you force the programming.\n" 
	  printf ">>> You can force at your own risks using the '-f' option.\n" 
	  printf ">>>=================================================================================<<<\n"
		
    # Not using the force mode
	  if (($force != 1)); then
      # We're in automation mode, so exiting
	  	if (($automation == 1)); then
	  		exit 3
      # We're in interactive mode, so asking the user  
		  else
			  read -p "Do you really want to continue? [y/n] " yn
			  case $yn in
				  [Yy]* ) break;;
				  [Nn]* ) exit;;
				  * ) printf "${bold}ERROR:${normal} Please answer with y or n\n";;
			  esac
		  fi
    
    # Using the force mode, so warning and continue
	  else
		  printf "${bold}${red}WARNING: ${normal}Force mode was required, although file is not matching board !!\n"
		  break
	  fi

  # Bin images have been made for card type (for example AD9V3 bin images while card type is AD9V3)-> let's continue
  else
  	break
  fi

done

#------------------------------------------------------------------------------------------------------------------
# We display a summary telling we're going to flash
printf "Continue to flash ${bold}$1${normal} ";
if [ $flash_type == "SPIx8" ]; then
	printf "and ${bold}$2${normal} "
fi
printf "to ${bold}card position $c${normal}(slot$card4)\n"

printf "\n"

#------------------------------------------------------------------------------------------------------------------
# In case of Partial Reconfig (PR), check that PR number of partial binary file corresponds to the PR number of loaded static image
PR_risk=0

if [ $PR_mode == 1 ]; then

  # Extract the PR Number from the binary file name
  PRC_dynamic=`echo $1  |awk -F 'oc_20' '{ print $2 }' | awk -F '_PR' '{ print $2 }'|awk -F '_'  '{ print $1 }'`

  # We did not find any PR number just above, so flashing is at risk
  if [ -z "$PRC_dynamic" ]; then
    printf ">>> ${bold}WARNING :${normal} NO dynamic PR Code in filename! <<<\n" 
    printf "Impossible to know if static and dynamic code match. You can continue at your own risk !\n" 
    PR_risk=1
  fi

  # Extract the PR number of loaded static image from the name of the bin file logged in /var/ocxl/cardxx
  # TODO get the actual static code from the card itself
  # PRC_static=`../oc-accel/software/tools/snap_peek 0x60 -C5`
  PRC_static=`cat /var/ocxl/card$c | awk -F 'oc_20' '{ print $2 }' | awk -F '_PR' '{ print $2 }'|awk -F '_'  '{ print $1 }'`

  # We did not find any PR number just above, so flashing is at risk
  if [ -z "$PRC_static" ]; then
    printf ">>> ${bold}WARNING :${normal} NO static PR Code found in filename logged in Flash log files! <<<\n" 
    printf "Impossible to know if static and dynamic code match. You can continue at your own risk !\n" 
    PR_risk=1

  # Comparing PR numbers from the bin file name and from the /var/ocxl/cardxx log file (dynamic vs static)
  else

    # Dynamic PR number does NOT correspond to static PR Number, so flashing is at risk
    if [ ${PRC_dynamic} !=  ${PRC_static} ]; then
      printf "\n>>>===================================================================================<<<\n"
      printf ">>> ${bold}${red}ERROR :${normal} Static code ${bold}${PRC_static}${normal} (flash log file) doesn't match with dynamic code ${bold}${PRC_dynamic}${normal} (your filename)!\n"
      printf ">>> You may crash and lose your card if you force the programming.\n" 
      printf ">>> You can force at your own risks using the '-f' option.\n" 
      printf ">>>===================================================================================<<<\n"
      PR_risk=1

    # Dynamic PR number = static PR number, so this is OK
    else
      printf "The PR Codes match ($PRC_static). Programming continues safely.\n" 
      PR_risk=0

    fi   # end of "${PRC_dynamic} !=  ${PRC_static}" test
  fi   # end of "-z "$PRC_static" test
fi   # end of "PRmode == 1" test

# Not using the force mode
if (($force != 1)); then

  # Partial reconfig is at risk
	if [ $PR_risk == 1 ]; then
    # We're in automation mode, so exiting
		if (($automation == 1)); then
			exit 4
    # We're in interactive mode, so asking the user
		else
			read -p "Do you want to continue? [y/n] " yn
			case $yn in
				[Yy]* ) ;;
				[Nn]* ) exit;;
				* ) printf "${bold}ERROR:${normal} Please answer with y or n\n";;
			esac
		fi

  # Partial reconfig is OK
	else 
    # We're in interactive mode, so just asking the user before continuing
		if (($automation != 1)); then
			read -p "Do you want to continue? [y/n] " yn
			case $yn in
				[Yy]* ) ;;
				[Nn]* ) exit;;
				* ) printf "${bold}ERROR:${normal} Please answer with y or n\n";;
			esac
		fi
	fi

 # Using the force mode
 else 
  # Partial reconfig is at risk, but force mode so warning and continue
	if  [ $PR_risk == 1 ]; then
		 printf "${bold}${red}WARNING: ${normal}Force mode was required, we continue although there were errors !!"
	fi
fi

#------------------------------------------------------------------------------------------------------------------
# Check if lowlevel flash utility is existing and executable
if [ ! -x $package_root/oc-flash ]; then
    	printf "${bold}ERROR:${normal} Utility capi-flash not found!\n"
    	exit 5
fi

#------------------------------------------------------------------------------------------------------------------
# Reset to card/flash registers to known state (factory) 
if [ "$reset_factory" -eq 1 ]; then
      	oc-reset $c factory "Preparing card for flashing"
fi

#------------------------------------------------------------------------------------------------------------------
# Entering card locking mechanism
trap 'kill -TERM $PID; perst_factory $c' TERM INT
# flash card with corresponding binary
bdf=`echo ${allcards_array[$c]}`
echo "${blue}Entering card locking mechanism ...${normal}"

#LockDir=/var/ocxl/oc-flash-script.lock
LockDir="$LockDirPrefix$bdf"  # $LockDirPrefix taken from oc-utils-common.sh (typically /var/ocxl/locked_card_)

# First step: create the dirname of $LockDir (typically /var/ocxl) in case it is not yet existing ("mkdir -p" always successful even if dir already exists)
mkdir -p `dirname $LockDir`

# Second step: trying to create $LockDir locking directory (typically into /var/ocxl)
# The $LockDir creation succeeded => implement a trap to be sure the $LockDir will be deleted when script exits
if mkdir $LockDir 2>/dev/null; then
	echo "${blue}$LockDir created${normal}"
	# The following line prepares a cleaning of the newly created dir when script will exit
	trap 'rm -rf "$LockDir";echo "${blue}$LockDir removed at the end of oc-flash-script${normal}"' EXIT	

 # The $LockDir creation failed because a LockDir already exists ("mkdir" fails if dir already exists)
else
	printf "${bold}${red}ERROR:${normal} Existing LOCK for card ${bdf} => Card has been locked already!\n"

  # Comparing lock creation date with last boot date
  DateLastBoot=`who -b | awk '{print $3 " " $4}'`
  EpochLastBoot=`date -d "$DateLastBoot" +%s`

  EpochLockDir=`stat --format=%Y $LockDir`
  DateLockDir=`date --date @$EpochLockDir`

  # Lock creation date is earlier than last boot date, so this lock should not be here anymore
  # Deleting the Lock dir and creating a new one
  if [ $EpochLockDir -lt $EpochLastBoot ]; then
	  echo
	  echo "Last BOOT:              `date --date @$EpochLastBoot` ($EpochLastBoot)"
	  echo "Last LOCK modification: $DateLockDir ($EpochLockDir)"
	  echo "$LockDir modified BEFORE last boot"
	  echo;echo "======================================================="
	  echo "LOCK is not supposed to still be here"
	  echo "  ==> Deleting and recreating $LockDir"
	  rmdir $LockDir
	  mkdir $LockDir
	  echo -e "${blue}$LockDir created during oc-flash-scrip${normal}"
	  # The following line prepares a cleaning of the newly created dir when script will output
	  trap 'rm -rf "$LockDir";echo "${blue}$LockDir removed at the end of oc-flash-script${normal}"' EXIT

  # Lock creation date is later than last boot date, so card lock is accurate => exiting now
  else
	  echo "$LockDir modified AFTER last boot"
	  printf "${bold}${red}ERROR:${normal}  Card has been recently locked!\n"
	  echo "Exiting..."
	  exit 10
  fi

fi

#------------------------------------------------------------------------------------------------------------------
# Flashing !

# SPIx8 needs two file inputs (primary/secondary)
if [ $flash_type == "SPIx8" ]; then
	# $package_root/oc-flash --type $flash_type --file $1 --file2 $2   --card ${allcards_array[$c]} --address $flash_address --address2 $flash_address2 --blocksize $flash_block_size &
	# until multiboot is enabled, force writing to 0x0
	$package_root/oc-flash --image_file1 $1 --image_file2 $2   --devicebdf $bdf --startaddr 0x0

# Not SPIx8, so only one file input (primary)
else
	$package_root/oc-flash --image_file1 $1 --devicebdf $bdf --startaddr 0x0
fi

#------------------------------------------------------------------------------------------------------------------
# Updating /var/ocxl/cardxx flash history file
if [ $flash_type == "SPIx8" ]; then
	printf "%-29s %-20s %s %s\n" "$(date)" "$(logname)" $1 $2 > /var/ocxl/card$c
else
	printf "%-29s %-20s %s\n" "$(date)" "$(logname)" $1 > /var/ocxl/card$c
fi

#------------------------------------------------------------------------------------------------------------------
# Waiting for the flash operation and the reseting the card
PID=$! # process ID of the most recently executed background pipeline
wait $PID
trap - TERM INT
wait $PID
RC=$?
if [ $RC -eq 0 ]; then
	if [ $PR_mode == 0 ]; then
		#  reload code from Flash (oc-reload calls a oc_reset)
		#  As we call routines, not shells, we keep the current card LockDir
      		printf " Auto reloading the image from flash.\n"
      		source $package_root/oc-reload.sh -L -C ${allcards_array[$c]}
	else
		#  In PR mode, reset cleans the logic but could be not mandatory
		reset_card $bdf factory " Resetting OpenCAPI card in slot $bdf"
	fi
fi
