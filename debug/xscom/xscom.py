import struct

class xscom:
        def __init__(self, chip_id, debug = False):
                xscom_path = "/sys/kernel/debug/powerpc/scom/%08x/access" % chip_id
                self.fd = open(xscom_path, "rb+", 0);
                self.debug = debug


        def __mangle(self, addr):
                if addr & (1 << 63):
                        addr |= (1 << 59)
                return addr << 3


        def getscom(self, addr):
                addr = self.__mangle(addr)
                self.fd.seek(addr)
                val = struct.unpack('Q', self.fd.read(8))[0]
                if self.debug:
                        print("getscom(0x%016x) = %016x" % (addr, val))
                return val


        def putscom(self, addr, val):
                if self.debug:
                        print("putscom(0x%016x, 0x%016x)" % (addr, val))

                addr = self.__mangle(addr)
                self.fd.seek(addr)
                self.fd.write(struct.pack('Q', val))
