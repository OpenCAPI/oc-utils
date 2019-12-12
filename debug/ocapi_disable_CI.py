#!/usr/bin/python3
import argparse
from argparse import RawTextHelpFormatter
from xscom.xscom import xscom
from subprocess import run
from os import listdir

#parse command line arguments
parser = argparse.ArgumentParser(usage="ocapi_triage.py [-h] [--verbose] [--chip CHIP_ID]", formatter_class=RawTextHelpFormatter)
parser.add_argument("--verbose", "-v", help="verbose output", action="count")
parser.add_argument("--chip", "-c", type=int, default=0, help="specify target chip ID (default = all)")
args = parser.parse_args()
verbose = 0
if args.verbose :
    verbose = args.verbose
scomlist = [0x5011000,0x5011030,0x5011060,0x5011090,0x5011200,0x5011230,0x5011260,0x5011290,0x5011400,0x5011430,0x5011460,0x5011490]

for xscom_chip in listdir('/sys/kernel/debug/powerpc/scom/'):
    scom = xscom(int(xscom_chip),verbose>1) 
    
    for addr in scomlist:
        data = scom.getscom(addr)
        if (verbose):
            print("getscom(0x%016x) = %016x" % (addr, data))
        #check bit 57 to see if this SM is configured in OCapi mode
        if (data & 0x0000000000000040):
            #set bit 46 to disable cache inject
            data |= 0x0000000000020000
            if verbose :
                print("putscom(0x%016x, 0x%016x)" % (addr, data))
            scom.putscom(addr, data)
