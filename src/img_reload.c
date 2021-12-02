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
// -------------------------------------------------------------------------------)
// This sequence is using the reload writing to the HWICAP (and not the iprog_icap)
// The procedure used is described in UG570 Table 11.3 for IPROG command using ICAP
// -------------------------------------------------------------------------------)

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "flsh_common_defs.h"
#include "flsh_common_funcs.h"
#include "flsh_global_vars.h"


int main(int argc, char *argv[])
{
  static int verbose_flag = 0;
  static int dualspi_mode_flag = 1; //default to assume x8 spi programming/loading
  static struct option long_options[] =
  {
    /* These options set a flag. */
    {"verbose", no_argument,       &verbose_flag, 1},
    {"brief",   no_argument,       &verbose_flag, 0},
    {"singlespi",    no_argument,  &dualspi_mode_flag, 0},
    {"dualspi",      no_argument,  &dualspi_mode_flag, 1},
    //{"image_file1",  required_argument, 0, 'a'},
    //{"image_file2",  required_argument, 0, 'b'},
    {"devicebdf",    required_argument, 0, 'c'},
    {"startaddr",    required_argument, 0, 'd'},
          {0, 0, 0, 0}
  };

  char binfile[1024];
  char binfile2[1024];
  char cfgbdf[1024];
  char cfg_file[1024];
  int CFG;
  int start_addr=0;
  char temp_addr[256];

  while(1) {
      int option_index = 0;
      int c;
      c = getopt_long (argc, argv, "c:d:",
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
          if(verbose_flag)
            printf(" Primary Bitstream: %s\n", optarg);
          strcpy(binfile,optarg);
          break;

        case 'b':
          if(verbose_flag)
            printf(" Secondary Bitstream: %s\n", optarg);
          strcpy(binfile2,optarg);
          break;

        case 'c':
          strcpy(cfgbdf,optarg);
          if(verbose_flag)
	    printf(" Target Device: %s\n", cfgbdf);
          break;

	case 'd':
	  memcpy(temp_addr,&optarg[2],8);
	  start_addr = (int)strtol(temp_addr,NULL,16);
          if(verbose_flag)
	    printf(" Start Address (same address for SPIx8 on both parts): %d\n", start_addr);
        case '?':
          /* getopt_long already printed an error message. */
          break;

        default:
          abort ();
        }
    }

  if(verbose_flag)
    printf("Registers value: TRC_CONFIG = %d, TRC_AXI = %d, TRC_FLASH = %d, TRC_FLASH_CMD = %d\n", TRC_CONFIG, TRC_AXI, TRC_FLASH, TRC_FLASH_CMD);

  u32 temp;
  int vendor,device, subsys;
  int BIN,i, j;
  //strcpy(cfg_file,"/sys/bus/pci/devices/");
  strcpy(cfg_file,"/OCXLSys/bus/pci/devices/");
  strcat(cfg_file,cfgbdf);
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

  TRC_FLASH_CMD = TRC_OFF;
  TRC_AXI = TRC_OFF;
  TRC_CONFIG = TRC_OFF;

  temp = config_read(CFG_SUBSYS,"Read subsys id of card");
  subsys = (temp >> 16) & 0xFFFF;
  //printf("SUBSYS: %x \n",subsys);

  if(verbose_flag) 
    printf("Verbose in use\n");

//-----------------------
//-----------------------
    //adding specific code for Partial reconfiguration

  u32 wdata, wdatatmp, rdata, burst_size;
  u32 CR_Write_clear = 0, CR_Write_cmd = 1, SR_ICAPEn_EOS=5;
  u32 SZ_Read_One_Word = 1, CR_Read_cmd = 2, RFO_wait_rd_done=1;
  time_t start_time, current_time;
  int timeout=0;

  time(&start_time);
  rdata = 0;
  if(verbose_flag) 
     printf("Waiting for ICAP EOS set \e[1A\n");

  while ((rdata != SR_ICAPEn_EOS) && (timeout < 1)) {
    rdata = axi_read(FA_ICAP, FA_ICAP_SR  , FA_EXP_OFF, FA_EXP_0123, "ICAP: read SR (monitor ICAPEn)");
    time(&current_time);
    timeout = (int)difftime(current_time, start_time);
  }
  // timeout can occur for old images, then use the old reload from oc-utils-common.sh
  if(timeout >= 1) {
     //printf("Timeout! EOS cannot be set \n");
     return 0;
  }
     
  if(verbose_flag) 
     printf("ICAP EOS done.\n");


  if(verbose_flag)  {
     read_QSPI_regs();
     read_ICAP_regs();
  }
//==============================================
// This sequence is using the reload writing to the HWICAP (and not the iprog_icap)
  printf("\n----------------------------------\n");
  printf(" Reloading code from Flash for the card in slot %s\n", cfgbdf);

  rdata = 0;
  while ((u32)rdata != (u32)SR_ICAPEn_EOS)  {
     rdata = axi_read(FA_ICAP, FA_ICAP_SR  , FA_EXP_OFF, FA_EXP_0123, "ICAP: read SR (monitor ICAPEn)");
     //printf("Waiting for ICAP SR = h%4x (read:%8x) \e[1A\n", SR_ICAPEn_EOS, rdata);
  }
  wdata = 0xFFFFFFFF;
  axi_write(FA_ICAP, FA_ICAP_WF, FA_EXP_OFF, FA_EXP_0123, wdata, "ICAP: write WF (4B to Keyhole Reg)");
  wdata = 0xAA995566;
  axi_write(FA_ICAP, FA_ICAP_WF, FA_EXP_OFF, FA_EXP_0123, wdata, "ICAP: write WF (4B to Keyhole Reg)");
  wdata = 0x20000000;
  axi_write(FA_ICAP, FA_ICAP_WF, FA_EXP_OFF, FA_EXP_0123, wdata, "ICAP: write WF (4B to Keyhole Reg)");
  wdata = 0x30020001;
  axi_write(FA_ICAP, FA_ICAP_WF, FA_EXP_OFF, FA_EXP_0123, wdata, "ICAP: write WF (4B to Keyhole Reg)");
  wdata = 0x00000000;
  axi_write(FA_ICAP, FA_ICAP_WF, FA_EXP_OFF, FA_EXP_0123, wdata, "ICAP: write WF (4B to Keyhole Reg)");
  wdata = 0x20000000;
  axi_write(FA_ICAP, FA_ICAP_WF, FA_EXP_OFF, FA_EXP_0123, wdata, "ICAP: write WF (4B to Keyhole Reg)");
  wdata = 0x30008001;
  axi_write(FA_ICAP, FA_ICAP_WF, FA_EXP_OFF, FA_EXP_0123, wdata, "ICAP: write WF (4B to Keyhole Reg)");
  wdata = 0x0000000F;
  axi_write(FA_ICAP, FA_ICAP_WF, FA_EXP_OFF, FA_EXP_0123, wdata, "ICAP: write WF (4B to Keyhole Reg)");
  // flush
  //printf("FLUSH START \n");
  //we need to use a specific axi_write since once the write done, we cannot read anymore in ICAP registers
  axi_write_no_check(FA_ICAP, FA_ICAP_CR, FA_EXP_OFF, FA_EXP_0123, CR_Write_cmd, "ICAP: write CR (initiate bitstream writing)");
  //printf("FLUSH DONE \n");
  //printf("\noc-reload from AXI_HWICAP DONE \n");
 // End of oc-reload
//==============================================

  
  return 0;  // Incisive simulator doesn't like anything other than 0 as return value from main() 
}

