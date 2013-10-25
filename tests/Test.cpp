/**************************************************************
 * This file is part of the MPWide communication library
 *
 * Written by Derek Groen with thanks going out to Steven Rieder,
 * Simon Portegies Zwart, Joris Borgdorff, Hans Blom and Tomoaki Ishiyama.
 * for questions, please send an e-mail to: 
 *                                     djgroennl@gmail.com
 * MPWide is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published 
 * by the Free Software Foundation, either version 3 of the License, 
 * or (at your option) any later version.
 *
 * MPWide is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with MPWide.  If not, see <http://www.gnu.org/licenses/>.
 * **************************************************************/

#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <iostream>

using namespace std;

#include "../MPWide.h"

/*
  Test.cpp
  A comprehensive benchmarking tool for testing MPWide between multiple sites.
*/

int main(int argc, char** argv){

  /* Initialize */
  int size = 1;

  if(argc==1) {
    printf("usage: ./MPWTest <act_as_server> <hostname or ip address of other endpoint> <streams (default: 1)> <buffer [kB] (default: 8 kB))> \n All parameters after the second are optional.\n");
    exit(0);
  }

  string host = (string) argv[2];

  if(argc>3) {
    size = atoi(argv[3]);
  }

  int bufsize = 8*1024;
  if(argc>4) {
    bufsize = atoi(argv[4]);
  }

/* Optional functionality which disables autotuning and allows users to specify a software-based packet pacing rate. */
//  if(argc>4) {
//    MPW_setAutoTuning(false);
//    MPW_setPacingRate((atoi(argv[4]))*1024*1024);
//  }

  int is_server = atoi(argv[1]);

  int winsize = 16*1024*1024;

  /* Create a path in MPWide, but do not yet connect it. */
  int path_id = MPW_CreatePathWithoutConnect(host, 16256, size); 

  /* Server process should be started first, it will try to connect, fail and then start listening for an incoming connection. */
  if(is_server == 1) {
    int status  = MPW_ConnectPath(path_id, true);
  }
  /* Client process should be started second. It will try to connect to the server, and exit if it fails. */
  else {
    int status  = MPW_ConnectPath(path_id, false);
    if (status == -1) {
        MPW_DestroyPath(path_id);
        cout << "MPWTest client program cannot connect to the server. Perhaps you entered a wrong hostname of the machine to connect to, or perhaps the firewall on the server node is blocking incoming traffic?" << endl;
        exit(1);
    }
  }
  cerr << "\nSmall test completed, now commencing large test.\n" << endl;

  /* Creating message buffers for the performance tests. */
  long long int len = bufsize*1024; 
  char* msg  = (char*) malloc(len);
  char* msg2 = (char*) malloc(len);

  for(int i=0; i<size; i++) {
    MPW_setWin(i,winsize);
  }

  /* Main loop for the performance tests */
  for(int i=0; i<20; i++) {

    /* Below are two examples of how to facilitate a two-way message exchange. 
     * These commands are identical for the server and client program. */
    // MPW_SendRecv(msg,len,msg2,len,channels,size); ///non-path version.
    // MPW_SendRecv(msg,len,msg2,len,path_id); ///path version

    /* But here the server first receives and then sends a message. */
    if(is_server == 1) {
      MPW_SendRecv(0,0,msg,len,path_id);
      MPW_SendRecv(msg,len,0,0,path_id);
    }
    /* While the client first sends and then receives a message. */
    else {
      MPW_SendRecv(msg,len,0,0,path_id);
      MPW_SendRecv(0,0,msg,len,path_id);
    }
    
    /* We sleep for a second to prevent any interference on the line due to 
     * asynchronicity of the performance tests. */
    sleep(1);
    cout << "End of iteration " << i << "." << endl;
  }

  free(msg);
  free(msg2);

  MPW_Finalize();

  /* If the test doesn't crash or hang and this line is printed on both client and 
     server machine, we can consider it as a success. */
  cout << "MPWTest is done." << endl;

  return 1;
}
