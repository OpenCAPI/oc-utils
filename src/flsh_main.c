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


//#include "svdpi.h"

#ifdef USE_SIM_TO_TEST
//  #include "svdpi.h"
//  extern void CFG_NOP( const char*);              
//  extern void CFG_NOP2(const char*, int, int, int*);              
#endif


extern void my_test();   
int update_image(u32 devsel,char binfile[1024], char cfgbdf[1024], int start_addr, int verbose_flag);

int main(int argc, char *argv[])
{
  static int verbose_flag = 0;
  static int dualspi_mode_flag = 1; //default to assume x8 spi programming/loading
  static struct option long_options[] =
  {
    /* These options set a flag. */
    {"verbose", no_argument,       &verbose_flag, 1},
    {"brief",   no_argument,       &verbose_flag, 0},
    {"singlespy",    no_argument,  &dualspi_mode_flag, 0},
    {"dualspi",      no_argument,  &dualspi_mode_flag, 1},
    {"image_file1",  required_argument, 0, 'a'},
    {"image_file2",  required_argument, 0, 'b'},
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
  //if (argc < 3) {
  //  printf("Usage: capi_flash <primary_bin_file> <secondary_bin_file> <card#>\n\n");
  //}
  //strcpy (binfile, argv[1]);
  //strcpy (binfile2, argv[2]);
  //strcpy(cfgbdf, argv[3]);
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
    printf("Verbose in use\n");
  
  if(dualspi_mode_flag) {
    if(verbose_flag)
      printf("Using spi x8 mode\n");

    if(binfile[0] == '\0') {
      printf("ERROR: Must supply primary bitstream\n");
      exit(-1);
    } else {
      if(verbose_flag)
        printf("Primary bitstream: %s !\n", binfile);
    }
    if(binfile2[0] == '\0') {
      printf("ERROR: Must supply secondary bitstream\n");
      exit(-1);
    }
    if(cfgbdf[0] == '\0') {
      printf("ERROR: Must supply target device\n");
      exit(-1);
    } 
  } else {
    printf ("Using spi x4 mode\n");
    if(binfile[0] == '\0') {
      printf("ERROR: Must supply primary bitstream\n");
      exit(-1);
    }  
    if(cfgbdf[0] == '\0') {
      printf("ERROR: Must supply target device\n");
      exit(-1);
    } 
  }

  if(verbose_flag)
    printf("Hello world - TRC_CONFIG = %d, TRC_AXI = %d, TRC_FLASH = %d, TRC_FLASH_CMD = %d\n", TRC_CONFIG, TRC_AXI, TRC_FLASH, TRC_FLASH_CMD);

  u32 temp;
  int vendor,device, subsys;
  strcpy(cfg_file,"/sys/bus/pci/devices/");
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

  temp = config_read(CFG_DEVID,"Read device id of card");
  vendor = temp & 0xFFFF;
  device = (temp >> 16) & 0xFFFF;
  if ( (vendor != 0x1014) || ( device != 0x062B)) {
    printf("DEVICE: %x VENDOR: %x\n",device,vendor);
    printf("This card shouldn't be flashed with this script\n");
    exit(-1);
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

  TRC_FLASH_CMD = TRC_OFF;
  TRC_AXI = TRC_OFF;
  TRC_CONFIG = TRC_OFF;

  printf("-------------------------------\n");
  printf("QSPI master core setup: started\r");
  QSPI_setup();          // Reset and set up Quad SPI core
  if(verbose_flag) 
    read_QSPI_regs();

  TRC_AXI = TRC_OFF;
  TRC_CONFIG = TRC_OFF;

//ICAP_setup();          // TODO: Create this after can load and read back FLASH image
  if(verbose_flag) 
    read_ICAP_regs();

  printf("QSPI master core setup: completed\n");

  printf("-------------------------------\n");
  printf("Programming Primary SPI with primary bitstream:\n    %s\n",binfile);
  update_image(SPISSR_SEL_DEV1,binfile,cfgbdf,start_addr, verbose_flag);

  if(dualspi_mode_flag) {
    printf("-------------------------------\n");
    printf("Programming Secondary SPI with secondary bitstream:\n    %s\n",binfile2);
    update_image(SPISSR_SEL_DEV2,binfile2,cfgbdf,start_addr, verbose_flag);
  }

  printf("Finished Programming Sequence\n");
  printf("-------------------------------");
  
  Check_Accumulated_Errors();

  return 0;  // Incisive simulator doesn't like anything other than 0 as return value from main() 
}

int update_image(u32 devsel,char binfile[1024], char cfgbdf[1024], int start_addr, int verbose_flag)
{
  int priv1,priv2;
  int dat, dif;
  int cp;
  int CFG;
  int BIN;
  time_t st, et, eet, set, ept, spt, svt, evt;
  int address_primary, raddress_primary, eaddress_primary, paddress_primary , address_secondary, raddress_secondary, eaddress_secondary, paddress_secondary;

  char bin_file[256];
  char cfg_file[256];

  int  print_cnt = 0;

  //if (argc < 2) {
  //  printf("Usage: capi_flash <rbf_file> <card#>\n\n");
  //}
  strcpy (bin_file, binfile);

  if ((BIN = open(bin_file, O_RDONLY)) < 0) {
    printf("ERROR: Can not open %s\n",bin_file);
    exit(-1);
  }

  strcpy(cfg_file, "/sys/bus/pci/devices/");
  strcat(cfg_file, cfgbdf);
  strcat(cfg_file, "/config");

  off_t fsize;
  struct stat tempstat;
  int num_64KB_sectors, num_256B_pages;
  address_primary = start_addr;  //TODO/FIXME: decide starting address within primary spi.
  address_secondary = start_addr;  //TODO/FIXME: decide starting address within secondary spi.
  raddress_primary = paddress_primary = eaddress_primary = address_primary;
  raddress_secondary = paddress_secondary = eaddress_secondary = address_secondary;
  if (stat(bin_file, &tempstat) != 0) {
    fprintf(stderr, "Cannot determine size of %s: %s\n", bin_file, strerror(errno));
    exit(-1);
  } else {
    fsize = tempstat.st_size;
  }
  printf("\nFlashing file of size %ld bytes\n",fsize);
  num_64KB_sectors = fsize/65536 + 1;
  num_256B_pages = fsize/256 + 1;
  if(verbose_flag) {
    printf("Performing %d 64KiB sector erases\n",num_64KB_sectors);
    printf("Performing %d 256B Programs/Reads\n",num_256B_pages);
  }

 // Set stdout to autoflush
 setvbuf(stdout, NULL, _IONBF, 0);

 int i,j;
 byte wdata[256], rdata[256], edat[256];

 //Initial Flash memory setup
 flash_setup(devsel);
 if(verbose_flag)
   read_flash_regs(devsel);

 //printf("Entering Erase Segment\n");
 st = set = time(NULL);
 cp = 1;
 lseek(BIN, 0, SEEK_SET);   // Reset to beginning of file
 for(i=0;i<num_64KB_sectors;i++) {
   //printf("Erasing Sector: %d      \r",i);
   printf("Erasing Sectors    : %d %% of %d sectors   \r",(int)(i*100/num_64KB_sectors), num_64KB_sectors);
   fw_Write_Enable(devsel);
   fw_64KB_Sector_Erase(devsel, eaddress_secondary);
   fr_wait_for_WRITE_IN_PROGRESS_to_clear(devsel);
   eaddress_secondary = eaddress_secondary + 65536;
 }
 eet = spt = time(NULL);
 eet = eet - set;
 printf("Erasing Sectors    : completed in   %d seconds           \n", (int)eet);
 
 //printf("Entering Program Segment\n");

 lseek(BIN, 0, SEEK_SET);   // Reset to beginning of file
 for(i=0;i<num_256B_pages;i++) {
   //printf("Writing Page: %d        \r",i);
   printf("Writing image code : %d %% of %d pages      \r",(int)(i*100/num_256B_pages), num_256B_pages);
   //printf("Reading piece of file\n");
   dif = read(BIN,&wdata,256);
   if (!(dif)) {
     //edat = 0xFFFFFFFF;
   }
   //printf("Setting Write Enable\n");
   fw_Write_Enable(devsel);
   //printf("Setting Page Program\n");
   fw_Page_Program(devsel, paddress_secondary, 256, wdata);
   //printf("program checkpoint 1\n");
   //printf("Polling for write complete\n");
   fr_wait_for_WRITE_IN_PROGRESS_to_clear(devsel);
   //printf("program checkpoint 2\n");
   paddress_secondary = paddress_secondary + 256;
 }
 ept = svt = time(NULL); 
 ept = ept - spt;
 printf("Writing Image code : completed in   %d seconds           \n", (int)ept);

 //printf("Entering Read Segment\n");
	
  int misc_pntcnt = 0;
 lseek(BIN, 0, SEEK_SET);   // Reset to beginning of file
 for(i=0;i<num_256B_pages;i++) {
   //printf("Reading Page: %d        \r",i);
   printf("Checking image code: %d %% of %d pages      \r",(int)(i*100/num_256B_pages), num_256B_pages);
   fr_Read(devsel, raddress_secondary, 256, rdata);
   raddress_secondary = raddress_secondary + 256;
   dif = read(BIN,&edat,256);
   if (!(dif)) {
     //edat = 0xFFFFFFFF;
   }
   for(j=0;j<256;j++) {
       if(edat[j] != rdata[j]) {
         printf("ERROR: EDAT byte %d: %x   RDAT byte %d: %x\n",j ,edat[j], j, rdata[j]);
       }
   }
 }
 et = evt = time(NULL); 
 evt = evt - svt;
 printf("Checking Image code: completed in   %d seconds           \n", (int)evt);
 
 et = et - st;
 printf("Total Time to write the new Image: %d seconds           \n", (int)et);
 printf("\n");

 close(BIN);
/*
 close(CFG);
 close(CFG_FD);
*/
 return 0;
}
