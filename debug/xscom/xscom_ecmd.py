import struct
import ecmd
import os

class xscom:
        def __init__(self, chip_id, debug = False):
            rc = ecmd.ecmdLoadDll(os.environ['ECMD_DLL_FILE'], "ver13,ver14")
            if (rc):
                print("ERROR: problem on dll load")
            #setup chip target
            self.tgt = ecmd.ecmdChipTarget()
            self.tgt.chipType = "pu"
            self.tgt.cage = 0
            self.tgt.node = 0
            self.tgt.slot = 0
            self.tgt.pos = chip_id
            self.tgt.core = 0

            #self.looper = ecmd.ecmdLooperData();

            #rc = ecmd.ecmdConfigLooperInit(self.tgt, ecmd.ECMD_ALL_TARGETS_LOOP, self.looper)
            #if (rc):
            #    print("ERROR: problem calling ecmdConfigLooperInit")

            self.debug = debug


        def __mangle(self, addr):
                if addr & (1 << 63):
                        addr |= (1 << 59)
                return addr << 3

        def getscom(self, addr):
                scomData = ecmd.ecmdDataBuffer(64)

                if self.debug:
                        print("getscom(0x%016x, 0x%016x)" % (addr, val))

                #addr = self.__mangle(addr)
                scomData.flushTo0() #reset mask to default 0 value
                #ecmd.ecmdConfigLooperNext(self.tgt, self.looper)
                rc = ecmd.getScom(self.tgt,addr,scomData)
                if(rc):
                    print("ERROR: problem calling putScom")
                return scomData.getDoubleWord(0)


        def putscom(self, addr, val,start=0,numBits=64):
                spyData = ecmd.ecmdDataBuffer(64)
                scomData = ecmd.ecmdDataBuffer(64)
                scomMask = ecmd.ecmdDataBuffer(64)

                if self.debug:
                        print("putscom(0x%016x, 0x%016x)" % (addr, val))

                addr = self.__mangle(addr)
                scomMask.flushTo0() #reset mask to default 0 value
                for x in range(start,start+numBits):
                    scomMask.setBit(x) #generates mask based on start and numBits values
                scomData.insertFromHexLeft(data,start,numBits)
                ecmd.ecmdConfigLooperNext(self.tgt, self.looper)
                rc = ecmd.putScomUnderMask(self.tgt,addr,scomData,scomMask)
                if(rc):
                    print("ERROR: problem calling putScom")
