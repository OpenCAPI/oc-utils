/*
 * Copyright 2019 International Business Machines
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <getopt.h>
#include "flsh_common_defs.h"
#include "flsh_common_funcs.h"
#include "flsh_global_vars.h"


//#include "svdpi.h"

#ifdef USE_SIM_TO_TEST
//  #include "svdpi.h"
//  extern void CFG_NOP( const char*);              
//  extern void CFG_NOP2(const char*, int, int, int*);              
#endif

int main(int argc, char *argv[])
{
  static int verbose_flag = 0;
  static int dualspi_mode_flag = 1; //default to assume x8 spi programming/loading
  static struct option long_options[] =
  {
    /* These options set a flag. */
    {"verbose", no_argument,       &verbose_flag, 1},
    {"brief",   no_argument,       &verbose_flag, 0},
    {"image_location",  required_argument, 0, 'a'},
    {"devicebdf",    required_argument, 0, 'c'},
          {0, 0, 0, 0}
  };

  char image_location[1024];
  char cfgbdf[1024];
  char cfg_file[1024];
  int CFG;  

  while(1) {
      int option_index = 0;
      int c;
      c = getopt_long (argc, argv, "a:b:c:",
                       long_options, &option_index);

      /* Detect the end of the options. */
      if (c == -1)
        break;

      switch (c)
        {
        case 0:
          /* If this option set a flag, do nothing else now. */
          if (long_options[option_index].flag != 0)
            break;
          printf ("option %s", long_options[option_index].name);
          if (optarg)
            printf (" with arg %s", optarg);
          printf ("\n");
          break;

        case 'a':
          printf("Primary Bitstream: %s\n", optarg);
          strcpy(image_location,optarg);
          break;

        case 'c':
          printf("Target Device: %s\n", optarg);
          strcpy(cfgbdf,optarg);
          break;

        case '?':
          /* getopt_long already printed an error message. */
          break;

        default:
          abort ();
        }
  }
  if(verbose_flag) {
    printf("Verbose in use\n");
  } else {
   printf("Verbose not in use\n");
  }
  if(strcmp(image_location,"user") == 0) {
    printf("Loading 'user' image\n");
  }
  else if(strcmp(image_location,"factory") == 0) {
     printf("Loading 'factory' image\n");
  }
  else {
    printf("ERROR: Must supply factory or user as image to load, provided %s!\n",image_location);
    exit(-1);
  }
  if(cfgbdf[0] == '\0') {
    printf("ERROR: Must supply target device\n");
    exit(-1);
  } 

  printf("Hello world - TRC_CONFIG = %d, TRC_AXI = %d, TRC_FLASH = %d, TRC_FLASH_CMD = %d\n", TRC_CONFIG, TRC_AXI, TRC_FLASH, TRC_FLASH_CMD);

  u32 temp;
  int vendor,device, subsys;
  strcpy(cfg_file,"/sys/bus/pci/devices/");
  strcat(cfg_file,"0006:00:00.0");
  strcat(cfg_file,"/config");
  if ((CFG = open(cfg_file, O_RDWR)) < 0) {
    printf("Can not open %s\n",cfg_file);
    exit(-1);
  }

  //TODO/FIXME: passing this on to global cfg descriptor
  if ((CFG_FD = open(cfg_file, O_RDWR)) < 0) {
    printf("Can not open %s\n",cfg_file);
    exit(-1);
  }
  temp = config_read(CFG_DEVID,"Read device id of card");
  vendor = temp & 0xFFFF;
  device = (temp >> 16) & 0xFFFF;
  printf("DEVICE: %x VENDOR: %x\n",device,vendor);
  if ( (vendor != 0x1014) || ( device != 0x062B)) {
    printf("This card shouldn't be flashed with this script\n");
    //exit(-1);
  }
  else {
    printf("This card has the flash controller!\n");
  }
/*
  if ((CFG = open(cfg_file, O_RDWR)) < 0) {
    printf("Can not open %s\n",cfg_file);
    exit(-1);
  }

  //TODO/FIXME: passing this on to global cfg descriptor
  if ((CFG_FD = open(cfg_file, O_RDWR)) < 0) {
    printf("Can not open %s\n",cfg_file);
    exit(-1);
  }

  lseek(CFG, 0, SEEK_SET);
  read(CFG, &temp, 4);
  printf("Device ID: %04X\n", device);
  printf("Vendor ID: %04X\n", vendor);

  lseek(CFG, 44, SEEK_SET);
  read(CFG, &temp, 4);
  subsys = (temp >> 16) & 0xFFFF;
*/

  TRC_FLASH_CMD = TRC_ON;
  TRC_AXI = TRC_ON;
  TRC_CONFIG = TRC_ON;

  printf("Beginning qspi master core setup\n");

  TRC_AXI = TRC_OFF;
  TRC_CONFIG = TRC_OFF;

  printf("Entering Image reload segment\n");

  reload_image(image_location,cfgbdf);

  printf("Finished Image reload segment\n");
  
  Check_Accumulated_Errors();

  return 0;  // Incisive simulator doesn't like anything other than 0 as return value from main() 
}

int reload_image(char image_location[1024], char cfgbdf[1024])
{
  int priv1,priv2;
  int dat, dif;
  int cp;
  int CFG;

  int fifo_room;
  int address;

  char cfg_file[256];

  int  print_cnt = 0;
  u32 bitstream_word;

  strcpy(cfg_file, "/sys/bus/pci/devices/");
  strcat(cfg_file, cfgbdf);
  strcat(cfg_file, "/config");

  
  reset_ICAP();
  read_ICAP_regs();
  fifo_room = read_ICAP_wfifo_size();
  if(fifo_room >= 8) {
    printf("Sufficient room for entire reload bitstream: %d\n",fifo_room);
  }
  else {
    printf("ERROR: Too few entries (%d) to hold reload bitstream\n",fifo_room);
    exit(-1);
  }
  if(strcmp(image_location,"factory") == 0) {
    address = 0x00000000;
  } else {
    address = 0x00100000;//FIXME/TODO: user address will change per card. 
  }

  bitstream_word = 0xFFFFFFFF;
  write_ICAP_bitstream_word(bitstream_word);
  wait_ICAP_write_done();
  //bitstream_word = 0x5599AA66;
  bitstream_word = 0xAA995566;
  write_ICAP_bitstream_word(bitstream_word);
  wait_ICAP_write_done();
  //bitstream_word = 0x04000000;
  bitstream_word = 0x20000000;
  write_ICAP_bitstream_word(bitstream_word);
  wait_ICAP_write_done();
  //bitstream_word = 0x0C400080;
  bitstream_word = 0x30020001;
  write_ICAP_bitstream_word(bitstream_word);
  wait_ICAP_write_done();
  bitstream_word = address;
  write_ICAP_bitstream_word(bitstream_word);
  wait_ICAP_write_done();
  //bitstream_word = 0x0C000180;
  bitstream_word = 0x30008001;
  write_ICAP_bitstream_word(bitstream_word);
  wait_ICAP_write_done();
  //bitstream_word = 0x000000F0;
  bitstream_word = 0x0000000F;
  write_ICAP_bitstream_word(bitstream_word);
  wait_ICAP_write_done();
  //bitstream_word = 0x04000000;
  bitstream_word = 0x20000000;
  write_ICAP_bitstream_word(bitstream_word);
  wait_ICAP_write_done();

  read_ICAP_regs(); 

/*
 close(CFG);
 close(CFG_FD);
*/
 return 0;
}

