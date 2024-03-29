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
int update_image_zynqmp(char binfile[1024], char cfgbdf[1024], int start_addr, int verbose_flag);

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

  while(1) {
      int option_index = 0;
      int c;
      c = getopt_long (argc, argv, "a:b:c:d:",
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
    printf("This card shouldn't be flashed with this script.\n");
    exit(-1);
  }

  TRC_FLASH_CMD = TRC_OFF;
  TRC_AXI = TRC_OFF;
  TRC_CONFIG = TRC_OFF;

  temp = config_read(CFG_SUBSYS,"Read subsys id of card");
  subsys = (temp >> 16) & 0xFFFF;
  //printf("SUBSYS: %x \n",subsys);
//adding specific code for 250SOC card (subsystem_id = 0x066A)
  if (subsys == 0x066A){
      dualspi_mode_flag = 0;
  }
//adding specific code for Partial reconfiguration (partial bit file provided)
  char *bit_file_extension = "_partial.bin";
  int PR_mode = 0;
  if (strstr(binfile, bit_file_extension)){
      dualspi_mode_flag = 0;
      PR_mode = 1;
  }

  if(verbose_flag) 
    printf("Verbose in use\n");

  if(dualspi_mode_flag) {
    printf(" Using spi x8 mode\n");
    if(binfile[0] == '\0') {
      printf("ERROR: Must supply primary bitstream\n");
      exit(-1);
    }
    //else 
      //{printf("Primary bitstream: %s !\n", binfile);}

    if(binfile2[0] == '\0') {
      printf("ERROR: Must supply secondary bitstream\n");
      exit(-1);
    }
    if(cfgbdf[0] == '\0') {
      printf("ERROR: Must supply target device\n");
      exit(-1);
    } 
  } else {
    if(PR_mode) 
	printf ("Using Partial reconfiguration mode\n");
    else 
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
  
//===============================================
//====== SPECIFIC 250SOC CARD PROGRAMMING =======
//===============================================
  if (subsys == 0x066A) {
//adding specific code for 250SOC card (subsystem_id = 0x066A)
     printf("----------------------------------\n");
     printf("Card with ZynqMP Detected\n");
     printf("Programming Flash with bitstream:\n    %s\n\n",binfile);
     update_image_zynqmp(binfile,cfgbdf,start_addr, verbose_flag);
     printf("\033[1mFinished Programming Sequence\033[0m\n");
     printf("----------------------------------\n");


//===============================================
//===== PARTIAL RECONFIGURATION PROGRAMMING =====
//=====   SUPPORTED CARDS :  AD9H3 - AD9H7 ======
//===============================================

  } else { // if(subsys != 0x066A)
    if(PR_mode) {
    //adding specific code for Partial reconfiguration
    // IMPORTANT //
    // The first access to FA_ICAP will enable the decoupling  mode in the FPGA to isolate the dynamic code
    // After the last PR programming instruction, a read to FA_QSPI will disable the decouple mode
  off_t fsize;
  struct stat tempstat;
  int num_package_icap, icap_burst_size, num_burst, num_package_lastburst, dif;
  u32 wdata, wdatatmp, rdata, burst_size;
  u32 CR_Write_clear = 0, CR_Write_cmd = 1, SR_ICAPEn_EOS=5;
  u32 SZ_Read_One_Word = 1, CR_Read_cmd = 2, RFO_wait_rd_done=1;
  int percentage = 0;
  int prev_percentage = 1;
  time_t spt, ept;

  // Working on the partial bin file
  printf("Opening PR bin file: %s\n", binfile);
  if ((BIN = open(binfile, O_RDONLY)) < 0) {
    printf("ERROR: Can not open %s\n",binfile);
    exit(-1);
  }
  if (stat(binfile, &tempstat) != 0) {
    fprintf(stderr, "Cannot determine size of %s: %s\n", binfile, strerror(errno));
    exit(-1);
  } else {
    fsize = tempstat.st_size;
  }

  num_package_icap = fsize/4 + (fsize % 4 != 0); //reading 32b words
  rdata = 0;
  while (rdata != SR_ICAPEn_EOS) {
    rdata = axi_read(FA_ICAP, FA_ICAP_SR  , FA_EXP_OFF, FA_EXP_0123, "ICAP: read SR (monitor ICAPEn)");
    //printf("Waiting for ICAP EOS set \e[1A\n");
  }
  if(verbose_flag) {
      printf("ICAP EOS done.            \n");
      read_QSPI_regs();
      read_ICAP_regs();
      read_FPGA_IDCODE();
  }

  icap_burst_size = axi_read(FA_ICAP, FA_ICAP_WFV , FA_EXP_OFF, FA_EXP_0123, "read_ICAP_regs");
  num_burst = num_package_icap / icap_burst_size;
  num_package_lastburst = num_package_icap - num_burst * icap_burst_size;

  if(verbose_flag) {
      printf(" Flashing PR bit file of size %ld bytes. Total package: %d. \n",fsize, num_package_icap);
      printf(" Total burst to transfer: %d with burst size of %d. Number of package is last burst: %d.\n", 
            num_burst, icap_burst_size, num_package_lastburst);
  }

  spt = time(NULL); 
  int print_done = 0;

  printf("___________________________________________________________________________\n");
  for(i=0;i<num_burst;i++) {
    percentage = (int)(i*100/num_burst);
    if( ((percentage %5) == 0) && (prev_percentage != percentage)) {
       printf("\e[1m  Writing\e[0m partial image code : \e[1m%d\e[0m %% of %d pages                        \r", percentage, num_burst);
       fflush(stdout);
    }
    for (j=0;j<icap_burst_size;j++) {
      dif = read(BIN,&wdatatmp,4);
      wdata = ((wdatatmp>>24)&0xff) | ((wdatatmp<<8)&0xff0000) | ((wdatatmp>>8)&0xff00) | ((wdatatmp<<24)&0xff000000);
      axi_write(FA_ICAP, FA_ICAP_WF, FA_EXP_OFF, FA_EXP_0123, wdata, "ICAP: write WF (4B to Keyhole Reg)");
      rdata = 1;
      while (rdata != CR_Write_clear) {
        rdata = axi_read(FA_ICAP, FA_ICAP_CR  , FA_EXP_OFF, FA_EXP_0123, "ICAP: read CR (monitor ICAPEn)");
      }
      rdata = 0;
      while (rdata != SR_ICAPEn_EOS) {
        rdata = axi_read(FA_ICAP, FA_ICAP_SR  , FA_EXP_OFF, FA_EXP_0123, "ICAP: read SR (monitor ICAPEn)");
      }
    }
    // Flush the WR FIFO
    axi_write(FA_ICAP, FA_ICAP_CR, FA_EXP_OFF, FA_EXP_0123, CR_Write_cmd, "ICAP: write CR (initiate bitstream writing)");
    rdata = 1;
    while (rdata != CR_Write_clear) {
      rdata = axi_read(FA_ICAP, FA_ICAP_CR  , FA_EXP_OFF, FA_EXP_0123, "ICAP: read CR (monitor ICAPEn)");
    }
    rdata = 0;
    while (rdata != SR_ICAPEn_EOS) {
      rdata = axi_read(FA_ICAP, FA_ICAP_SR  , FA_EXP_OFF, FA_EXP_0123, "ICAP: read SR (monitor ICAPEn)");
    }
    prev_percentage = percentage;
  }

  //printf("Working on the last burst.\n");
  for (i=0;i<num_package_lastburst;i++) {
    dif = read(BIN,&wdatatmp,4);
    wdata = ((wdatatmp>>24)&0xff) | ((wdatatmp<<8)&0xff0000) | ((wdatatmp>>8)&0xff00) | ((wdatatmp<<24)&0xff000000);
    axi_write(FA_ICAP, FA_ICAP_WF, FA_EXP_OFF, FA_EXP_0123, wdata, "ICAP: write WF (4B to Keyhole Reg)");
    rdata = 1;
    while (rdata != CR_Write_clear) {
      rdata = axi_read(FA_ICAP, FA_ICAP_CR  , FA_EXP_OFF, FA_EXP_0123, "ICAP: read CR (monitor ICAPEn)");
    }
    rdata = 0;
    while (rdata != SR_ICAPEn_EOS) {
      rdata = axi_read(FA_ICAP, FA_ICAP_SR  , FA_EXP_OFF, FA_EXP_0123, "ICAP: read SR (monitor ICAPEn)");
    }
  }
  axi_write(FA_ICAP, FA_ICAP_CR, FA_EXP_OFF, FA_EXP_0123, CR_Write_cmd, "ICAP: write CR (initiate bitstream writing)");
  rdata = 1;
  while (rdata != CR_Write_clear) {
    rdata = axi_read(FA_ICAP, FA_ICAP_CR  , FA_EXP_OFF, FA_EXP_0123, "ICAP: read CR (monitor ICAPEn)");
  }
  rdata = 0;
  while (rdata != SR_ICAPEn_EOS) {
    rdata = axi_read(FA_ICAP, FA_ICAP_SR  , FA_EXP_OFF, FA_EXP_0123, "ICAP: read SR (monitor ICAPEn)");
  }
  close(BIN);
  // The following read is just to remove the decoupling done in FPGA
  rdata = axi_read(FA_QSPI, FA_QSPI_SPICR, FA_EXP_OFF, FA_EXP_0123, "Test axi_read");
 
  ept = time(NULL); 
  ept = ept - spt;
  printf("\e[1m Partial reprogramming  \033[1mcompleted\033[0m ok in   %d seconds\e[0m           \n", (int)ept);
  printf("___________________________________________________________________________\n");

//-----------------------



//===============================================
//== NORMAL PROGRAMMING (No Partial Reconfig) ===
//===== SUPPORTED CARDS :  all except 250SOC ====
//===============================================
    } else { // if(subsys != 0x066A) and if (!PR_mode)
// default code : not a 250SOC and not a PartialReconfiguration
    printf(" QSPI master core setup: started\r");
    QSPI_setup();          // Reset and set up Quad SPI core
    if(verbose_flag) 
      read_QSPI_regs();

    TRC_AXI = TRC_OFF;
    TRC_CONFIG = TRC_OFF;

  //ICAP_setup();          // TODO: Create this after can load and read back FLASH image
    if(verbose_flag) 
      read_ICAP_regs();

    printf(" QSPI master core setup: completed\n");
    printf("\n----------------------------------\n");

    printf("\033[1m Programming Primary SPI with primary bitstream:\033[0m\n    %s\n",binfile);
    update_image(SPISSR_SEL_DEV1,binfile,cfgbdf,start_addr, verbose_flag);

    if(dualspi_mode_flag) {
      printf("----------------------------------\n");
      printf("\033[1m Programming Secondary SPI with secondary bitstream:\033[0m\n    %s\n",binfile2);
      update_image(SPISSR_SEL_DEV2,binfile2,cfgbdf,start_addr, verbose_flag);
    }

    printf("\033[1m Finished Programming Sequence\033[0m\n");
    printf("----------------------------------\n");
  
    Check_Accumulated_Errors();

   }
}
  return 0;  // Incisive simulator doesn't like anything other than 0 as return value from main() 
}


//========================================

// Programming Primary/Secondary SPI with primary/secondary bitstream
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
  if (verbose_flag)
    printf("\n Flashing file of size %ld bytes\n",fsize);
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
 int percentage = 0;
 int prev_percentage = 1;

 //Initial Flash memory setup
 flash_setup(devsel);
 if(verbose_flag)
   read_flash_regs(devsel);

 //printf("Entering Erase Segment\n");
 st = set = time(NULL);
 cp = 1;
 lseek(BIN, 0, SEEK_SET);   // Reset to beginning of file
 for(i=0;i<num_64KB_sectors;i++) {
   percentage = (int)(i*100/num_64KB_sectors);
   if( ((percentage %5) == 0) && (prev_percentage != percentage))
      printf(" Erasing Sectors    : \033[1m%d %%\033[0m of %d sectors   \r", percentage, num_64KB_sectors);
   fw_Write_Enable(devsel);
   fw_64KB_Sector_Erase(devsel, eaddress_secondary);
   fr_wait_for_WRITE_IN_PROGRESS_to_clear(devsel);
   eaddress_secondary = eaddress_secondary + 65536;
   prev_percentage = percentage;
 }

 eet = spt = time(NULL);
 eet = eet - set;
 printf(" Erasing Sectors    : \033[1mcompleted\033[0m in   %d seconds           \n", (int)eet);
 
 //printf("Entering Program Segment\n");

 lseek(BIN, 0, SEEK_SET);   // Reset to beginning of file
 for(i=0;i<num_256B_pages;i++) {
   percentage = (int)(i*100/num_256B_pages);
   if( ((percentage %5) == 0) && (prev_percentage != percentage))
       printf("\033[1m Writing\033[0m image code : \033[1m%d %%\033[0m of %d pages                        \r", percentage, num_256B_pages);
   dif = read(BIN,&wdata,256);
   if (!(dif)) {
     //edat = 0xFFFFFFFF;
   }
   fw_Write_Enable(devsel);
   fw_Page_Program(devsel, paddress_secondary, 256, wdata);
   //printf("program checkpoint 1\n");
   fr_wait_for_WRITE_IN_PROGRESS_to_clear(devsel);
   //printf("program checkpoint 2\n");
   paddress_secondary = paddress_secondary + 256;
   prev_percentage = percentage;
 }
 ept = svt = time(NULL); 
 ept = ept - spt;
 printf(" Writing Image code : \033[1mcompleted\033[0m in   %d seconds           \n", (int)ept);

 //printf("Entering Read Segment\n");
	
  int misc_pntcnt = 0;
 lseek(BIN, 0, SEEK_SET);   // Reset to beginning of file
 for(i=0;i<num_256B_pages;i++) {
   percentage = (int)(i*100/num_256B_pages);
   if( ((percentage %5) == 0) && (prev_percentage != percentage))
       printf(" Checking image code: %d %% of %d pages      \r", percentage, num_256B_pages);
   fr_Read(devsel, raddress_secondary, 256, rdata);
   raddress_secondary = raddress_secondary + 256;
   prev_percentage = percentage;
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
 printf(" Checking Image code: \033[1mcompleted\033[0m in   %d seconds           \n", (int)evt);
 
 et = et - st;
 printf("\033[1m Total Time to write the new Image:  %d seconds.\033[0m           \n", (int)et);
 printf("\n");

 close(BIN);
/*
 close(CFG);
 close(CFG_FD);
*/
 return 0;
}


//int update_image_zynqmp(u32 devsel,char binfile[1024], char cfgbdf[1024], int start_addr)
int update_image_zynqmp(char binfile[1024], char cfgbdf[1024], int start_addr, int verbose_flag)
{
  int priv1,priv2;
  int dat, dif;
  int cp;
  int CFG;
  int BIN;
  time_t et, set, evt;
  int address_primary, raddress_primary, eaddress_primary, paddress_primary , address_secondary, raddress_secondary, eaddress_secondary, paddress_secondary;

  char bin_file[256];
  char cfg_file[256];

  int  print_cnt = 0;

  config_write(0x638, 0x00000002, 4, ""); //take microblaze out of rst
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
  if (verbose_flag)
     printf(" Flashing file of size %ld bytes\n",fsize);
  num_64KB_sectors = fsize/65536 + 1;
  num_256B_pages = fsize/256 + 1;
  if (verbose_flag)
     printf("Performing %d 256B Programs/Reads\n",num_256B_pages);

 // Set stdout to autoflush
 setvbuf(stdout, NULL, _IONBF, 0);

 int i,j;
 byte wdata[256], rdata[256], edat[256];

 u32 wdata_word;
 int write_count;
 int y;
 u32 done_status;

 u32 write_addr;
 u32 ack_status; 

 u32 ack_addr;
 int percentage = 0;
 int prev_percentage = 1;

 //printf("reseting file pointer....\n");
 set = time(NULL);
 cp = 1;
 lseek(BIN, 0, SEEK_SET);   // Reset to beginning of file
 write_count = 0;
 
 write_addr = 0x00000000;
 ack_addr =   0x00001000;
 ack_status = 0x00000001;
//printf("Beginning writing through Zynq ...\n");
 lseek(BIN, 0, SEEK_SET);   // Reset to beginning of file
 for(i=0;i<num_256B_pages;i++) {
   if (i > 1){
     percentage = (int)(i*100/num_256B_pages);
     if( ((percentage %5) == 0) && (prev_percentage != percentage)) {
       printf("\033[1m Writing\033[0m image code : \033[1m%d\033[0m %% of %d pages                                                \r", percentage , num_256B_pages);}
     prev_percentage = percentage;
   } else {
     printf("Waiting for card acknowledgement (\033[1mPlease be patient. It can take up to a minute!)\033[0m \r");
   }
   ack_status = axi_read_zynq(FA_QSPI, ack_addr, FA_EXP_OFF, FA_EXP_0123, "");

   while(ack_status == 0x00000001){
     ack_status = axi_read_zynq(FA_QSPI, ack_addr, FA_EXP_OFF, FA_EXP_0123, "");
   }

   for(y=0;y<=64;y++){
     if(y != 64){
       dif = read(BIN,&wdata_word,4);
       axi_write_zynq(FA_QSPI, write_addr , FA_EXP_OFF, FA_EXP_0123, wdata_word, "");
       write_addr = write_addr + 4;
       write_count = write_count + 1;
       paddress_secondary = paddress_secondary + 256;
     }
     else{
       axi_write_zynq(FA_QSPI, ack_addr , FA_EXP_OFF, FA_EXP_0123, 0x00000001, "");
       write_addr = 0x00000000;
     }
   }
 }

 printf(" Writing image code \033[1mcompleted\033[0m                        \n");
 axi_write_zynq(FA_QSPI, ack_addr , FA_EXP_OFF, FA_EXP_0123, 0x000000FF, "");
 //printf("Number of writes in decimal:  %d\n", write_count);
 write_count = 0;
 
 config_write(0x638, 0x00000000, 4, "");
 printf("Copying image from DDR to flash \033[1mcompleted\033[0m\n");


 evt = time(NULL);
 et = evt - set;

 printf("\033[1m Total Time:   %d seconds\033[0m\n\n", (int)et);

 close(BIN);
 return 0;
}


void my_test(void)
{
//  u32 rdata;
//  int rdata_i;

 byte wdata[1024];
 byte rdata[1024];
 int i;
 char ds[1024], ds_elt[10];

 printf("\n Entered my_test \n\n");

 for (i=0; i < 1024; i++) { wdata[i] = i+1; rdata[i] = 0x00; }   // assign something into arrays

//printf("Call CFG_NOP\n");
//CFG_NOP( "from my_main");
//
//printf("Call CFG_NOP\n");
//CFG_NOP( "from my_main again");
//
//printf("Call CFG_NOP\n");
//CFG_NOP( "from my_main yet again");
//
//printf("Call CFG_NOP2\n");
//rdata_i = 0x01010101;
//CFG_NOP2("from my_main", 0x34, 0x87654321, &rdata_i);
//rdata = (u32) rdata_i;
//printf("     CFG_NOP2 returned rdata_i = h%8x, rdata = h%8x\n",rdata_i, rdata);

//printf("Call config_write");
//config_write(CFG_FLASH_DATA, 0x11223344, 4, "TEST config_write");
//
//rdata = 0xDEADBEEF;
//printf("0) rdata = h%8x\n", rdata);
//
//rdata = config_read( CFG_FLASH_DATA, "TEST");
//printf("1) rdata = h%8x\n", rdata);

//config_write(CFG_FLASH_DATA, 0x11223344, 8, "TEST-FAIL");

//config_write(CFG_FLASH_DATA, 0x11223344, 1, "TEST");
//rdata = config_read( CFG_FLASH_DATA, "TEST");
//printf("2) rdata = h%8x\n", rdata);
//
//config_write(CFG_FLASH_DATA, 0x55667788, 2, "TEST");
//rdata = config_read( CFG_FLASH_DATA, "TEST");
//printf("3) rdata = h%8x\n", rdata);
//
//config_write(CFG_FLASH_DATA, 0x99AABBCC, 4, "TEST");
//rdata = config_read( CFG_FLASH_DATA, "TEST");
//printf("4) rdata = h%8x\n", rdata);
//
//printf("\n\n");

//TRC_AXI   = TRC_ON;
//TRC_FLASH = TRC_ON;

//axi_write(FA_QSPI, FA_QSPI_SPICR, FA_EXP_OFF, FA_EXP_0123, 0x12345678, "Test axi_write");

//rdata = axi_read(FA_QSPI, FA_QSPI_SPICR, FA_EXP_OFF, FA_EXP_0123, "Test axi_read");
//printf("axi_read SPICR rdata = x%8x\n", rdata);

 // Test calls to FLASH_OP;
   FLASH_OP_CHECK = FO_CHK_ON;
 //         devsel           cmd   addr        num_addr, num_dummy, num_bytes, wdata[], rdata[], dir, s
// flash_op(SPISSR_SEL_DEV1, 0xC1, 0x89ABCDEF, 3       , 0        , 0        , wdata  , rdata  , FO_DIR_XCHG, "print test 1");
// flash_op(SPISSR_SEL_DEV2, 0xC2, 0x89ABCDEF, 0       , 3        , 1        , wdata  , rdata  , FO_DIR_RD  , "print test 2");
// flash_op(SPISSR_SEL_DEV2, 0xC3, 0x89ABCDEF, 3       , 10       , 1        , wdata  , rdata  , FO_DIR_RD  , "print test 3");
// flash_op(SPISSR_SEL_DEV2, 0xC3, 0x89ABCDEF, 3       , 10       , 2        , wdata  , rdata  , FO_DIR_RD  , "print test 4");
// flash_op(SPISSR_SEL_DEV2, 0xC3, 0x89ABCDEF, 3       , 10       , 3        , wdata  , rdata  , FO_DIR_RD  , "print test 5");
// flash_op(SPISSR_SEL_DEV2, 0xC3, 0x89ABCDEF, 3       , 10       , 4        , wdata  , rdata  , FO_DIR_RD  , "print test 6");
// flash_op(SPISSR_SEL_NONE, 0xC3, 0x89ABCDEF, 1       , 10       , 15       , wdata  , rdata  , FO_DIR_WR  , "print test 7");
// flash_op(SPISSR_SEL_DEV1, 0xC4, 0x89ABCDEF, 2       , 8        , 16       , wdata  , rdata  , FO_DIR_XCHG, "print test 8");
// flash_op(SPISSR_SEL_DEV2, 0xC5, 0x89ABCDEF, 3       , 2        , 100      , wdata  , rdata  , FO_DIR_RD  , "print test 9");
// flash_op(SPISSR_SEL_DEV1, 0xC6, 0x89ABCDEF, 0       , 0        , 0        , wdata  , rdata  , FO_DIR_XCHG, "print test 10");
// flash_op(SPISSR_SEL_DEV1, 0xC7, 0x89ABCDEF, 4       , 9        , 1        , wdata  , rdata  , FO_DIR_XCHG, "print test 11");
// flash_op(SPISSR_SEL_DEV1, 0xC8, 0x89ABCDEF, 4       , 10       , 1        , wdata  , rdata  , FO_DIR_XCHG, "print test 12");
// flash_op(SPISSR_SEL_DEV1, 0xC9, 0x89ABCDEF, 4       , 10       , 2        , wdata  , rdata  , FO_DIR_XCHG, "print test 13");

// flash_op(SPISSR_SEL_DEV1, 0x6B, 0x00000100, 3       , 8        , 16       , wdata  , rdata  , FO_DIR_XCHG, "read mem 1");

// TRC_FLASH = TRC_ON;
// TRC_AXI   = TRC_ON;

   rdata[0] = fr_Extended_Address_Register(SPISSR_SEL_DEV1);
   printf("\nExtended Address Register = %2.2X before writing\n", rdata[0]);

   fw_Write_Enable             (SPISSR_SEL_DEV1);
   fw_Extended_Address_Register(SPISSR_SEL_DEV1, 0x00);   // bit [0] is upper address bit
   rdata[0] = fr_Extended_Address_Register(SPISSR_SEL_DEV1);
   printf("\nExtended Address Register = %2.2X after writing to 0\n", rdata[0]);

   fw_Write_Enable             (SPISSR_SEL_DEV1);
   fw_Extended_Address_Register(SPISSR_SEL_DEV1, 0x01);   // bit [0] is upper address bit
   rdata[0] = fr_Extended_Address_Register(SPISSR_SEL_DEV1);
   printf("\nExtended Address Register = %2.2X after writing to 1\n", rdata[0]);

   fr_wait_for_WRITE_IN_PROGRESS_to_clear(SPISSR_SEL_DEV1);


   TRC_FLASH = TRC_ON;

   fr_Read(SPISSR_SEL_DEV1, 0x00000100, 1024, rdata);  // WARNING: calling fr_Read somehow changes wdata[0,1,2] to FF. Can't figure out why
   //sprintf(ds, "rdata (hex) "); for (i = 0; i < 20; i++) sprintf(ds, "%s %2X ",ds, *(rdata+i) );
   // Create printable string of read data
   strcpy(ds, "rdata (hex)");
   for (i = 0; i < 20; i++) {
     snprintf(ds_elt, sizeof(ds_elt), " %2.2X", *(rdata+i));
     strcat(ds, ds_elt);
   }
   printf("\nBefore 1st PAGE PROGRAM and ERASE: Read of 0x100 = %s\n", ds);

   fw_Write_Enable(SPISSR_SEL_DEV1);
   fw_Page_Program(SPISSR_SEL_DEV1, 0x00000100, 1024, wdata);

   fr_wait_for_WRITE_IN_PROGRESS_to_clear(SPISSR_SEL_DEV1);

   fr_Read(SPISSR_SEL_DEV1, 0x00000100, 1024, rdata);
   //sprintf(ds, "rdata (hex) "); for (i = 0; i < 20; i++) sprintf(ds, "%s %2X ",ds, *(rdata+i) );
   // Create printable string of read data
   strcpy(ds, "rdata (hex)");
   for (i = 0; i < 20; i++) {
     snprintf(ds_elt, sizeof(ds_elt), " %2.2X", *(rdata+i));
     strcat(ds, ds_elt);
   }
   printf("\nAfter 1st PAGE PROGRAM, but before ERASE: Read of 0x100 = %s\n", ds);

   fw_Write_Enable(SPISSR_SEL_DEV1);
   fw_4KB_Subsector_Erase(SPISSR_SEL_DEV1, 0x00000100);

   fr_wait_for_WRITE_IN_PROGRESS_to_clear(SPISSR_SEL_DEV1);

   fr_Read(SPISSR_SEL_DEV1, 0x00000100, 1024, rdata);
   //sprintf(ds, "rdata (hex) "); for (i = 0; i < 20; i++) sprintf(ds, "%s %2X ",ds, *(rdata+i) );
   // Create printable string of read data
   strcpy(ds, "rdata (hex)");
   for (i = 0; i < 20; i++) {
     snprintf(ds_elt, sizeof(ds_elt), " %2.2X", *(rdata+i));
     strcat(ds, ds_elt);
   }
   printf("\nAfter ERASE, before 2nd PAGE PROGRAM: Read of 0x100 = %s\n", ds);

   for (i=0; i < 1024; i++) { wdata[i] = ~(i+1); };      // Change write data
   fw_Write_Enable(SPISSR_SEL_DEV1);
   fw_Page_Program(SPISSR_SEL_DEV1, 0x00000100, 1024, wdata);

   fr_wait_for_WRITE_IN_PROGRESS_to_clear(SPISSR_SEL_DEV1);

   fr_Read(SPISSR_SEL_DEV1, 0x00000100, 1024, rdata);
   //sprintf(ds, "rdata (hex) "); for (i = 0; i < 20; i++) sprintf(ds, "%s %2X ",ds, *(rdata+i) );
   // Create printable string of read data
   strcpy(ds, "rdata (hex)");
   for (i = 0; i < 20; i++) {
     snprintf(ds_elt, sizeof(ds_elt), " %2.2X", *(rdata+i));
     strcat(ds, ds_elt);
   }
   printf("\nAfter 2nd PAGE PROGRAM: Read of 0x100 = %s\n", ds);


   TRC_FLASH = TRC_OFF;

   read_flash_regs(SPISSR_SEL_DEV1);
   read_flash_regs(SPISSR_SEL_DEV2);

  return;
}
