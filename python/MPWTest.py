import sys
import time
import argparse

# load the shared object
#import MPWide
import ctypes
MPWide = ctypes.cdll.LoadLibrary("../libMPW.so")

"""
 MPWide Test script (direct port from Test.cpp).

- TODO: incorporate argparse mechanism

- TODO: It's possible to convert NumPy array to char * without copying (which
  we do now). Rupert knows more about this, and we should do this if the
  performance is lacking.

"""
size = 1

if len(sys.argv)==1:
    print "usage: ./MPWTest <ip address of other endpoint> <channels> <buffer [kb]> <pacing [MB/s]> <tcpwin [bytes]> \n All parameters after the first are optional."
    sys.exit()

if len(sys.argv) > 1:
    host = sys.argv[1]


size = 1
 
if len(sys.argv)>2:
    size = int(sys.argv[2])

bufsize = 8*1024

# Size of message that you submit.
if len(sys.argv)>3:
    bufsize = int(sys.argv[3])
  
if len(sys.argv)>4:
    MPWide.MPW_setPacingRate(int(sys.argv[4])*1024*1024)

winsize = 16*1024*1024

if len(sys.argv)>5:
    winsize = int(sys.argv[5])
  
path_id = MPWide.MPW_CreatePath_c(host,16256,size);

msglen = bufsize*1024 

msg  = "a" * msglen #string 
msg2 = "b" * msglen #string)

# test loop 
for i in xrange(0,100):
    MPWide.MPW_SendRecv(msg,msglen,msg2,msglen,path_id)
    time.sleep(1);
    print "End of iteration ", i, "."
  
MPWide.MPW_DestroyPath(path_id)

