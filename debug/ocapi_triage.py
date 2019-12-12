#!/usr/bin/python3
import argparse
from argparse import RawTextHelpFormatter
ecmdMode = 0
try: 
    import ecmd
    from xscom.xscom_ecmd import xscom
    ecmdMode = 1
except:
    from xscom.xscom import xscom
from ocapi_triage_targets import assign_targets
from subprocess import run

#parse command line arguments
parser = argparse.ArgumentParser(usage="ocapi_triage.py [-h] [--verbose] [--chip CHIP_ID] [targets]", formatter_class=RawTextHelpFormatter)
parser.add_argument("--verbose", "-v", help="verbose output (prints scoms with fully masked output, -vv to also print raw data and mask)", action="count")
parser.add_argument("--chip", "-c", type=int, default=0, help="specify target chip ID (default = 0)")
parser.add_argument('targets', help="specify targets to print (by default, prints all)\n"
        "fir        = Global FIR scoms\n"
        "odl        = ODL FIR and status scoms\n"
        "otl1, otl2 = Stack 1/2 OTL scoms\n"
        "ctl1, ctl2 = Stack 1/2 CS.CTL.MISC scoms\n"
        "dat1, dat2 = Stack 1/2 DAT.MISC scoms\n"
        "smx1, smx2 = Stack 1/2 CS.SMx scoms",  nargs = argparse.REMAINDER)
args = parser.parse_args()

scom = xscom(0) #currently defaults to chip id 0, will expand later

#scom.getscom(0x5013C00)

target_list=[]
if(len(args.targets) == 0): #default targets: all
    assign_targets(target_list, "all")
else:
    for x in args.targets:
        assign_targets(target_list, x)

for i in range(0, len(target_list)):
    if(target_list[i][0] == 0): #print section headers
        print("############################################")
        print(target_list[i][2])
        print("############################################")
    else:
        data = scom.getscom(target_list[i][0])
        if(target_list[i][1] == 0):
            mask = 0x0000000000000000 #don't mask if no mask is specified
        else:
            mask = scom.getscom(target_list[i][1])
        masked_data = data & (~mask) #apply mask
        
        #format output data
        data = "{0:#0{1}x}".format(data,18)
        mask = "{0:#0{1}x}".format(mask,18)
        masked_output = "{0:#0{1}x}".format(masked_data,18)
    
        #print output
        if(args.verbose == 2): #print raw data and mask if extra verbose
            print((target_list[i][2] + ":").ljust(46) + data)
            print((target_list[i][2] + " Mask:").ljust(46) + mask)
        #print masked data only if nonzero, or if set to verbose
        if(args.verbose or masked_data):
            print((target_list[i][2] + " (Masked):").ljust(46) + masked_output)
if ecmdMode == 0:
	run(["bash", "./log_triage.bash"]) #copy dmesg and msglog into local log files
