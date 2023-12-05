
import os
import sys
import struct

length = len(sys.argv)
if (length < 2):
    sys.exit()
descriptor = open(os.path.join(os.getcwd(), "_cl_"), "wb")
descriptor.write(length.to_bytes(4, byteorder=sys.byteorder, signed=False))
for i in range(length-1):
    arg = sys.argv[i+1]
    length = len(arg)
    descriptor.write(length.to_bytes(4, byteorder=sys.byteorder, signed=False))
    descriptor.write(arg.encode())
descriptor.close()

