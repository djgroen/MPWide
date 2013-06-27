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

/* TestRestart.cpp
 * MPWide Test Case.
 * Written by Derek Groen, Sept. 2009.
 *
 * Usage for two sites:
 *
 * 1. Make sure that the bool "threesites" in this file is set to 'false'!
 *
 * on one site, run: 
 *   ./TestRestart <other host> 0 <number of streams>
 * then on the other side, run:
 *   ./TestRestart <other host> 1 <number of streams>
 * up to 2000 times to test the restart mechanism. If correct, one side will stay active whereas
 * the other side can start and stop.
 *
 * Usage for three sites:
 *
 * 1. Make sure that the bool "threesites" in this file is set to 'true'!
 *
 * on site 1, run: 
 *   ./TestRestart 0 0 <number of streams>
 * then on site 2, run: 
 *   ./TestRestart <site 1> 1 <number of streams>
 * and on site 3, run:
 *   ./TestRestart <site 1> 2 <number of streams>
 *
 * Note that <number of streams> should be equal for all three instances!
 * In the case of site 1, this program will double the number of streams
 * automatically to allow connections to both site 2 and site 3. *
 */

#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>

using namespace std;

#include "MPWide.h"

int main(int argc, char** argv){

  bool threesites = false; //enable this to perform 3 site tests.

  string a;

  cout << "Maximum size = " << a.max_size() << endl;

  int size = 2;

  /* Initialize */

  string host = (string) argv[1];
  int flag = atoi(argv[2]);
  if(argc>3) {
    int arg_size = atoi(argv[3]);
    if(arg_size % 2 == 0 && arg_size > 0) {
      size = arg_size;
    }
    else {
      cout << "INPUT ERROR: This script tests MPW_Cycle. Therefore, the number of streams specified must be a multiple of 2." << endl;
      exit(0);
    }
  }

  if(threesites && flag == 0) {
    size *= 2;
  }


  int offset = 0;
  if(flag == 2) {
    offset = size;
  }

  string *hosts = new string[size]; 
  int *ports = new int[size];    
  int *cports = new int[size];

  for(int i=0; i<size; i++) {
    hosts[i] = host;
    ports[i] = 6000+i+offset;
    cports[i] = 6000+i+size;
    if(threesites && flag != 0) {
      cports[i] += size;
    }
  }

  MPW_Init(hosts,ports,cports,size);
  // Delete created arrays
  delete [] hosts;
  delete [] ports;
  delete [] cports;

  cerr << "\nCommencing Test Suite.\n" << endl;


  long long int len = 6*1024*1024; //testing_size/numstreams;
  long long int len2 = 5*1024*1024;
  long long int len3 = 1*1024*1024;

  char* msg  = (char*) malloc(len);
  char* msg2 = (char*) malloc(len*2);
  char* msg3 = (char*) malloc(len2);
  char* msg4 = (char*) malloc(len2*2);
  char* msg5 = (char*) malloc(len3);
  char* msg6 = (char*) malloc(len3*2);

  int channels[size];

  msg[1000000] = 'y';

  for(int i=0; i<size; i++) {
    channels[i] = i;
  }

  char* testa;
  char* testb;

  int limit = 2000;
  if(flag == 1) { limit= 1; }

  for(int i=0; i<limit; i++) {

    cout << "Testing SendRecv." << endl;

    if(threesites && flag == 0) {
      MPW_SendRecv(msg3,len2,msg4,len2,channels,size/2);
      MPW_SendRecv(msg3,len2,msg4,len2,&(channels[size/2]),size/2);

      MPW_SendRecv(msg,len,msg2,len,channels,size/2);
      MPW_SendRecv(msg,len,msg2,len,&(channels[size/2]),size/2);
    }
    else {
      MPW_SendRecv(msg3,len2,msg4,len2,channels,size);

      MPW_SendRecv(msg,len,msg2,len,channels,size);
    }  

    cout << "Testing Cycle" << endl;

    if(threesites && flag == 0) {
      MPW_Cycle(msg6, len3*2, msg5,len3,&(channels[size/4]),size/4,channels,size/4);
      MPW_Cycle(msg6, len3*2, msg5,len3,&(channels[3*size/4]),size/4,&(channels[size/2]),size/4);
    }
    else {
      if(flag > 0) {
        MPW_Cycle(msg5, len3, msg6,len3*2,channels,size/2,&(channels[size/2]),size/2);
      } else {
        MPW_Cycle(msg6, len3*2, msg5,len3,&(channels[size/2]),size/2,channels,size/2);
      }
    }

    if(flag == 0) {
      cout << "Closing and reopening channels." << endl;

      if(threesites) {
	MPW_CloseChannels(channels, size/2);
	MPW_ReOpenChannels(channels, size/2);
      }
      else {
        MPW_CloseChannels(channels, size);
        MPW_ReOpenChannels(channels, size);
      }
    }
    if(flag == 1) {
      MPW_Finalize();
      return 1;
    }

    cout << "End of iteration " << i << "." << endl;
  }
  free(msg);
  free(msg2);
  free(msg3);
  free(msg4);
  free(msg5);
  free(msg6);

  MPW_Finalize();

  return 1;
}
