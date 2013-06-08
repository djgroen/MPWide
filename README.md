MPWide
=======

MPWide is a communication library for message passing across wide area networks.

------------------
MPWide README
version 1.51, last modified on June 8th 2013
------------------

This is a README for the MPWide communication library, which has been written by Derek Groen.
This software is open-source (LGPL), although proper crediting would be appreciated. If you have
any questions, or would like to use MPWide for your project and would like to get some advice,
feel free to contact me at:
djgroennl@gmail.com

More information on MPWide can be found at:
  http://castle.strw.leidenuniv.nl/software/mpwide.html

The MPWide paper can be found at:
  http://iopscience.iop.org/1749-4699/3/1/015002

Main extensions of this version over the one described in the paper are:
- Support for simplified 'path' data structure (where a path is a connection
  consisting of one or more streams).
- Preliminary Python interface.
- recognition of host names (basic DNS resolution).
- addition of packet pacing.
- introduction of mpw-cp file copying client.
- 1 second fixed socket timeout when establishing connections to improve
  reliability.

Open issues:
- Using paths gives a very slight performance disadvantage at the moment.

------------------
REQUIRED SOFTWARE:
------------------
A C++ compiler.
Access to the C sockets API.

------------------
INSTALLATION:
------------------
1. compile by typing 'make'

------------------
RUNNING A SIMPLE TEST:
------------------
On machine 1:
./Test 0.0.0.0 1

On machine 2:
./Test <ip address or hostname of machine 1> 1

------------------
COPYING A FILE using SSH and MPWide:
------------------
1. install MPWide in the $HOME/.mpwide directory on each endpoint machine.
2. run mpw-cp to copy the file. Here is an example syntax for local cluster use with Gigabit Ethernet. Here MPWide is configured with 4 streams, pacing at 500 MB/s each and a tcp buffer of 256 kilobytes.:
  ./mpw-cp machine-a:/home/you/yourfile him@machine-b:/home/him/yourfileinhishome 4 500 256

The full syntax of the command is:
./mpw-cp <source host>:<source file or dir> <destination host>:<destination file or dir> <number of streams> <pacing rate in MB> <tcp buffer in kB>

New in MPWide version 1.2 - REVERSE mode:

By default MPWide will place the server on the destination host. However, version 1.2 now supports a REVERSE mode, where the server is placed on the source host. This can help in bypassing tricky firewalls.
The full syntax for REVERSE mode is:
./mpw-cp s:<source host>:<source file or dir> <destination host>:<destination file or dir> <number of streams> <pacing rate in MB> <tcp buffer in kB>

-+-+-+-+-
Troubleshooting / Known bugs:

SYMPTOM: mpw-cp connects properly, but seems to hang, as no receiving file is created.
POSSIBLE CAUSE: This may not be an actual problem, as MPWide retains all data in memory until the transfer of the file is
completed. If the file is large, the network slow or MPWide poorly configured, it might take a while.
POSSIBLE SOLUTION 1: Wait for a few minutes, or try transferring a smaller file first to get an accurate picture of the achieved bandwidth.

SYMPTOM: mpw-cp hangs, the output mentions something like:
"[X] Trying to bind as server at <Y>. Result = 1"
POSSIBLE CAUSE: The firewall on the server side is blocking the incoming connections that MPWide tries to establish.
POSSIBLE SOLUTION 1: Run mpw-cp in REVERSE mode (or in normal mode if you are currently using REVERSE mode).
POSSIBLE SOLUTION 2: Ask your local administrator to open a range of ports in your firewall. Make sure the range is at least twice the number of streams you wish to use for your transfers.
POSSIBLE SOLUTION 3: Change the value of baseport on line 130 in mpw-cp.cpp and recompile on both endpoints. This will change the port range used by mpw-cp.cpp.

SYMPTON: mpw-cp connects properly, but then hangs for a long time.
POSSIBLE CAUSE: Unknown, I've only encountered this once. Could be a platform-specific issue or a MPWide bug.
POSSIBLE SOLUTION 1: Run mpw-cp on the other end-point node.
-+-+-+-+-

------------------
API REFERENCE
------------------
A reference of the programming interface can be found in MPWide.h. Different
versions of MPWide 1.x will have compatible APIs (and MPWide2 will almost
certainly be backwards compatible by the time that comes out).

------------------
TEST SYNTAX:
------------------
./Test <ip address of other end-node> <number of parallel streams>

------------------
TESTRESTART
------------------
The TestRestart program performs test using disconnection and reconnection of
sockets. Documentation on how to use it is put at the top of TestRestart.cpp.

------------------
FORWARDER
------------------
Forwarder is a program that provides tcp message forwarding using MPWide. It
connects two port ranges from two different remote ip addresses, and does not
proceed with forwarding until both ends of a given forwarding channel are
connected.

Example execution:
./Forwarder < forward.cfg

Config file layout:
<address1 ip address>
<address1 baseport>
<address2 ip address>
<address2 baseport>
<number of streams>
(...repeat this for any other forwarding route...)
done

Example config file:
1.2.3.4
6000
5.6.7.8
7000
16
done

------------------
DATAGATHER
------------------

The DataGather program is intended to continuously synchronize a two directories residing on
two different machines. It has not been thoroughly tested, and for normal file transfer
purposes we recommend using mpw-cp instead.

------------------
Recent Updates
------------------

* June 8th 2013,
-- Added MPW_SetPathWin().
-- Added MPW_ISendRecv and related functions to allow for non-blocking SendRecvs. 
   This functionality is still in beta.

* May 31st 2013,
-- Removed an important bug in CreatPath.

* Apr 2nd 2013,
-- Various code cleanups. Minor changes to the API to improve consistency.

* Feb 12th 2013,
-- Added support for 'paths' to simplify access.
-- Cleaned up and shortened the code base.

* May 16th 2012, Release of version 1.2.1
-- Added a Python interface + test script. The Python interface requires SWIG.

* February 27th 2012, Release of version 1.2
-- Added basic support for IPv4 domain name resolution (although using ip addresses
   may still be preferable in sophisticated setups).
-- DNS resolution can explicitly be called using MPW_DNSResolve().

* February 16th 2012, Release of version 1.2b
-- Fixed a pile of bugs in mpw-cp.
-- Added support for reverse server-client ordering in mpw-cp ("REVERSE mode").
-- Implemented a socket connection timeout of 1 second to improve reliability.

* July 11th 2011, Release of version 1.1b
-- Added mpw-cp file transfer client.
-- Slightly improved documentation.

* August 19th 2010: Release of version 1.0
-- Fixed and optimized MPW_DSendRecv. It now supports packet-pacing as it should.
-- Modified test.cpp to provide a sound test for MPW_DSendRecv.

* June 21st 2010: Release of version 1.0a
-- First public release.
