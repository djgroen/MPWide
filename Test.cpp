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

#include "MPWide.h"

int main(int argc, char** argv){

  /* Initialize */
  int size = 1;

  if(argc==1) {
    printf("usage: ./MPWTest <ip address of other endpoint> <channels> <buffer [kb]> <pacing [MB/s]> <tcpwin [bytes]>. \n All parameters after the first are optional.\n");
    exit(0);
  }

  string host = (string) argv[1];

  if(argc>2) {
    size = atoi(argv[2]);
  }

  int bufsize = 8*1024;
  if(argc>3) {
    bufsize = atoi(argv[3]);
  }

  if(argc>4) {
    MPW_setAutoTuning(false);
    MPW_setPacingRate((atoi(argv[4]))*1024*1024);
  }

  int winsize = 16*1024*1024;
  if(argc>5) {
    winsize = atoi(argv[5]);
  }

//  string *hosts = new string[size];
//  int sports[size];   

//  for(int i=0; i<size; i++) {
//    hosts[i] = host;
//    sports[i] = 16256+i;
//  }

  int path_id = MPW_CreatePath(host, 16256, size); ///path version
//  MPW_Init(hosts, sports, size); ///non-path version.
//  delete [] hosts;
  cerr << "\nSmall test completed, now commencing large test.\n" << endl;


  long long int len = bufsize*1024; 

  char* msg  = (char*) malloc(len);
  char* msg2 = (char*) malloc(len);

  int channels[size];

  for(int i=0; i<size; i++) {
    channels[i] = i;
    MPW_setWin(i,winsize);
  }

  int comm_mode = 1; //0 for regular sendrecv tests, 1 for non-blocking sendrecv tests.

  /* test loop */
  for(int i=0; i<20; i++) {

//    MPW_SendRecv(msg,len,msg2,len,channels,size); ///non-path version.
    if(comm_mode==0) {
      MPW_SendRecv(msg,len,msg2,len,path_id); ///path version
    }
    else if(comm_mode == 1) {
      int id = MPW_ISendRecv(msg,len,msg2,len,path_id);

      sleep(1);
    
      cout << "Has the non-blocking comm finished?" << endl;
      MPW_Has_NBE_Finished(id);
      cout << "Doing something else..." << endl;
    
      MPW_Wait(id);
    }
    
    sleep(1);
    cout << "End of iteration " << i << "." << endl;
  }

  free(msg);
  free(msg2);

  MPW_Finalize();

  return 1;
}
