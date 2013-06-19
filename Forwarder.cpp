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
#include <math.h>
#include <iostream>

using namespace std;

#include "MPWide.h"

int main(int argc, char** argv){

  string source_host[8];
  int source_baseport[8];
  string dest_host[8];
  int dest_baseport[8];
  int streams[8];
  int numcon = 0; //number of topological connections.
  int size = 0;

  for(int i = 0; i<8 ; i++) {
    cout << "Enter source host ip (or 'done' if no other streams need to be defined):" << endl;
    cin >> source_host[i];
    if(source_host[i].compare("done") == 0) {
      cout << "Done parsing." << endl;
      break;
    }
    cout << "Enter source base port:" << endl;
    cin >> source_baseport[i];
    cout << "Enter destination host ip:" << endl;
    cin >> dest_host[i];
    cout << "Enter destination base port:" << endl;
    cin >> dest_baseport[i];
    cout << "Enter the number of parallel streams:" << endl;
    cin >> streams[i];
  
    numcon++;
    size += streams[i];
  }

  string *hosts = new string[size*2];
  int ports[size*2];

  int offset = 0;
  for(int i=0 ; i<numcon; i++) {
    for(int j=0 ; j<streams[i]; j++) {
      hosts[     offset+j] = source_host[i];
      hosts[size+offset+j] = dest_host[i];
      ports[     offset+j] = source_baseport[i]+j;
      ports[size+offset+j] = dest_baseport[i]+j;

      cout << source_host[i] << ":" << source_baseport[i]+j << " " << dest_host[i] << ":" << dest_baseport[i]+j << endl;
    }
    offset += streams[i];
  }

  cout << "Initializing..." << endl;

  MPW_Init(hosts,ports,size*2);
  delete [] hosts;

//  MPW_TinyTest();

  cerr << "\nStarting Relay Service.\n" << endl;

  int channels[size];
  int channels2[size];

  for(int i=0; i<size; i++) {
    channels[i] = i;
    channels2[i] = i+size;
  }
  MPW_Relay(channels, channels2, size);

  MPW_Finalize();


  return 1;
}
