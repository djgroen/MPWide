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


#if MPW_PacingMode == 1

int MPW_Test_PacingRate() {
  cout << "Test_PacingRate()" << endl;
  MPW_setPacingRate(10*1024*1024);
  int i = MPW_getPacingRate();

  if(i != 10*1024*1024) {
    return -1;
  } 
  return 0;
}

#endif

int Test_DNSResolve(){
  cout << "Test_DNSResolve()" << endl;
  string localhost("localhost");
  string localhostIP("127.0.0.1");
  string host(MPW_DNSResolve(localhost));
  if(host.compare(localhostIP) == 0) {
    return 0;
  }
  else {
    cout << "Unit test Test_DNSResolve failed: host returns " << localhost << endl;
    return -1;
  }
}  

int Test_AutoTuning(){
  cout << "Test_AutoTuning()" << endl;
  MPW_setAutoTuning(false);
  if(MPW_AutoTuning()) {
    return -1;
  }
  MPW_setAutoTuning(true);
  if(!MPW_AutoTuning()) {
    return -1;
  }
  return 0;
}

int Test_Paths(){
  cout << "Test_Paths()" << endl;  
  int i = MPW_CreatePathWithoutConnect("localhost", 16256, 4);
  if(i<0) {
    return -1;
  }
  MPW_setWin(0, 262144);
  MPW_setPathWin(i, 262144);
  int j = MPW_DestroyPath(i);
  if(j<0) {
    return -1;
  }
  return 0;
}

int Test_MPW_splitBuf() {
  cout << "Test_MPW_splitBuf()" << endl;
  char* buf = "aaabbbcccdddeeefff";
  char** split_buf = new char*[6];
  long long int* chunk_sizes = new long long int[6];
  MPW_splitBuf(buf, 18, 6, split_buf, chunk_sizes);
  if(split_buf[5][2] == 'f' && chunk_sizes[5] == 3) {
    return 0;
  }
  return -1;
}

int Test_MPW_setChunkSize() {
  cout << "Test_setChunkSize()" << endl;
  MPW_setChunkSize(65536,65536);
  return 0;
}

int checkOutput(int i, int fails) {
  if(i<0) {
    cout << "This Unit Test Failed." << endl;
    return fails+1;
  }
  return fails;
}

int main(int argc, char** argv){

  int fails = 0;
  int i = 0;
  #if MPW_PacingMode == 1
  i = MPW_Test_PacingRate();
  checkOutput(i, fails);
  #endif
  i = Test_DNSResolve();
  checkOutput(i, fails);

  i = Test_AutoTuning();
  checkOutput(i, fails);

  i = Test_Paths();
  checkOutput(i, fails);

  i = Test_MPW_splitBuf();
  checkOutput(i, fails);

  i = Test_MPW_setChunkSize();
  checkOutput(i, fails);

  cout << "Unit tests completed. Number of failed tests: " << fails << endl;
  cout << "Please also run MPW_Functionaltests to more completely test MPWide." << endl;
}
