# MPWide
=======

MPWide is a communication library for message passing across wide area networks.

For the main documentation, please refer to doc/MPWide Manual.pdf or doc/MPWide Manual.odt.

This software is open-source (LGPL), although proper crediting would be appreciated. If you have
any questions, or would like to use MPWide for your project and would like to get some advice,
feel free to contact me at:
djgroennl@gmail.com

More information on MPWide can be found at:
  https://github.com/djgroen/MPWide 

The MPWide paper can be found at:
  http://dx.doi.org/10.5334/jors.ah

Main extensions of this version over the one described in the paper are:
- Support for simplified 'path' data structure (where a path is a connection
  consisting of one or more streams).
- Python interfaces, possible with SWIG and Cython (prototype version).
- Recognition of host names (basic DNS resolution).
- Addition of packet pacing.
- Introduction of mpw-cp file copying client.
- Support for non-blocking communications.
- Improved thread-safety, reliability and performance.
- Several new function calls, which allow MPWide to be integrated into server/client 
  based environments.

Open issues:
- See the github page for a list of open issues.

## Major Updates

* October 15th 2013,
-- Fixed mpw-cp.
-- Added more comments to the code.
-- Wrote a full user manual.

* August 9th 2013,
-- Improved thread-safety of MPWide.
-- Improved performance.
-- Improved error reporting and logging infrastructure.
-- Added several functions to allow MPWide to be used in a non-blocking manner.
-- Added several functions to make it easier for MPWide to be used in a larger client/server environment.

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
