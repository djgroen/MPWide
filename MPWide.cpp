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
#include <string>
#include "MPWide.h"
#include <errno.h>
#include "Socket.h"
#include <sys/time.h>
#include <pthread.h>
#include <cstdlib>
#include <vector>
#include <unistd.h>

#define SendRecvInputReport 0
int MPWideAutoTune = 1;

void MPW_setAutoTuning(bool b) {
  if(b) {
    MPWideAutoTune = 1;
  }
  else {
    MPWideAutoTune = 0;
  }
}

/* Enable(1)/Disable(0) Performance Timing Measurements */
#define PERF_TIMING 1
/* Performance report verbosity: 1 reports speeds on send/recv. 2 reports on initialization details.
   3 also reports number of steps taken to recv packages. 4 becomes ridiculously verbose, with e.g. 
   reports for accumulated bytes after every chunk is received. */
#define PERF_REPORT 1
#define MONITORING 1

#define min(X,Y)   (X < Y ? X : Y)

using namespace std;

/* STREAM-specific definitions */
static vector<int> port;
static vector<int> cport;
static vector<int> isclient;
static vector<Socket> client;
static vector<string> remote_url;
// length of all the above vectors:
static int num_streams = 0;

// This is set to true on the first invocation of MPW_Init. MPW_EMPTY is given a 1-byte buffer.
static bool MPW_INITIALISED = false;
char *MPW_EMPTY;

/* PATH-specific definitions */
class MPWPath {
  public: 
    string remote_url; // end-point of the path
    int *streams; // id numbers of the streams used
    int num_streams; // number of streams

    MPWPath(string remote_url, int* str, int numstr)
      :remote_url(remote_url), num_streams(numstr)
    {
      streams = new int[numstr];
      for(int i=0; i<numstr; i++) {
        streams[i] = str[i];
      }
    }
};
/* List of paths. */
static vector<MPWPath> paths;

/* Send and Recv occurs in chunks of size tcpbuf_ssize/rsize.
 * Setting this to 1MB or higher gave problems with Amsterdam-Drexel test. */
static int tcpbuf_ssize = 8*1024;
static int tcpbuf_rsize = 8*1024;
static int relay_ssize = 8*1024;
static int relay_rsize = 8*1024;

#if PacingMode == 1
  static double pacing_rate = 100*1024*1024; //Pacing rate per stream.
  static long long int pacing_sleeptime = 1000000/(pacing_rate/(1.0*tcpbuf_ssize)); //Sleep time for SendRecvs in microseconds.

  double MPW_getPacingRate() {
    return pacing_rate;
  }
  void MPW_setPacingRate(double rate) {
    if(rate == -1) {
      pacing_rate = -1;
      pacing_sleeptime = 0;
    }
    else {
      pacing_rate = rate;
      pacing_sleeptime = 1000000/(pacing_rate/(1.0*tcpbuf_ssize));
      cout << "Pacing enabled, rate = " << pacing_rate << " => delay = " << pacing_sleeptime << " us." << endl;
    }
  }
#endif

typedef struct thread_tmp{
  long long int sendsize, recvsize;
  long long int* dyn_recvsize; //For DynEx.
  int thread_id;
  int channel;
  int numchannels;
  int numrchannels; //Cycle only.
  char* sendbuf;
  char* recvbuf;
}thread_tmp;

/* global thread memory */
static thread_tmp* ta;

#ifdef PERF_TIMING
  double GetTime(){
    struct timeval tv;
    gettimeofday( &tv, NULL);
    double time;
    time = (tv.tv_sec + (double)tv.tv_usec*1e-6);
    return time;
  }

  void MPW_setChunkSize(int sending, int receiving) {
    tcpbuf_ssize = sending;
    tcpbuf_rsize = receiving;
    cout << "Chunk Size  modified to: " << sending << "/" << receiving << "." << endl;
  }

  long long int swapLLI( const long long int ll){
    union ud{
      char cval[8];
      long long int llval;
    }u;

    u.llval = ll;

    char tmp; //swaps...
    tmp = u.cval[0]; u.cval[0] = u.cval[7]; u.cval[7] = tmp;
    tmp = u.cval[1]; u.cval[1] = u.cval[6]; u.cval[6] = tmp;
    tmp = u.cval[2]; u.cval[2] = u.cval[5]; u.cval[5] = tmp;
    tmp = u.cval[3]; u.cval[3] = u.cval[4]; u.cval[4] = tmp;
    return u.llval;
  }

  double BarrierTime   = 0.0; //4 newly introduced globals for monitoring purposes
  double SendRecvTime  = 0.0;
  double PackingTime   = 0.0;
  double UnpackingTime = 0.0;
#endif

/* malloc function, duplicated from the TreePM code. */
void *MPWmalloc( const size_t size){
  void *p;
  p = malloc( size);
  if( p == NULL){
    fprintf( stderr, "malloc error size %ld\n", sizeof(size));
    exit(1);
  }
  return p;
}

/* Convert a host name to an ip address. */
char *MPW_DNSResolve(char *host){
  if(isdigit(host[0])) {
    return host;
  }
  const hostent* host_info = 0 ;

  for( int i=0; (host_info==0) && (i<4); i++) {
    host_info = gethostbyname(host);
  }
  if(host_info) {
    const in_addr* address = (in_addr*)host_info->h_addr_list[0] ;
    cout << " address found: " << inet_ntoa( *address ) << endl;
    host = (char*) (inet_ntoa(*address));
    return host;
  }
  cout << "Error: Unable to resolve host name" << endl;
}

char *MPW_DNSResolve(string host) {
  char * l_host = new char[host.size() + 1];
  std::copy(host.begin(), host.end(), l_host);
  l_host[host.size()] = '\0';
  return MPW_DNSResolve(l_host);
}

int selectSockets(int wchannel, int rchannel, int mask)
/* Returns:
 0 if no access.
 1 if read on read channel.
 2 if write on write channel.
 3 if both. */
{
  int rsockp = client[rchannel].getSock();
  int wsockp = client[wchannel].getSock();

  /* args: FD_SETSIZE,writeset,readset,out-of-band sent, timeout*/
  int ok = 0;
  int access = 0;

  fd_set rsock, wsock;
  FD_ZERO(&rsock);
  FD_ZERO(&wsock);
  FD_SET(rsockp,&rsock); 
  FD_SET(wsockp,&wsock);

  struct timeval timeout;
  timeout.tv_sec  = 10;
  timeout.tv_usec = 0;

  ok = select(max(rsockp,wsockp)+1, &rsock, &wsock, (fd_set *) 0, &timeout);
  if(ok) {
    if(mask%2 == 0) {
      if(FD_ISSET(rsockp,&rsock)) { access++;    }
    }
    if(mask/2 == 0) {
      if(FD_ISSET(wsockp,&wsock)) { access += 2; }
    }
  }
  else if (ok<0){
    cout << "socketSelect error: " << errno << endl; //" Msg: " << strerror(errno) << endl;
  }
  return access;
}

int MPW_NumChannels(){
  return num_streams;
}

long long int bytes_sent;
bool stop_monitor = false;

/* Performs per-second bandwidth monitoring in real-time */
void *MPW_TBandwidth_Monitor(void *args)
{
  ofstream myfile;
  myfile.open("bandwidth_monitor.txt");
  long long int old_bytes_sent = 0;
  long long int cur_bytes_sent = 0;
  long long int old_time = 0;
  
  while(!stop_monitor) {
    if(old_time != int(GetTime())) {
      cur_bytes_sent = bytes_sent;
      myfile << "time: " << int(GetTime()) << " bandwidth: " << cur_bytes_sent - old_bytes_sent << endl;
      old_bytes_sent = cur_bytes_sent;
      old_time = int(GetTime());
    }
    usleep(1000);
  }
  myfile.close();
  return NULL;
}

typedef struct init_tmp {
  int i;
  int port;
  int cport;
}init_tmp;

void* MPW_InitStream(void* args) 
{
  init_tmp t = *((init_tmp *) args);

  int i = t.i;
  int port = t.port;
  int cport = t.cport;
  bool connected = false;
  client[i].set_non_blocking(false);

  while(!connected) {

    if(isclient[i]) {
      client[i].create();
      /* Patch to bypass firewall problems. */
      if(cport>0) {
        #if PERF_REPORT > 1
          cout << "[" << i << "] Trying to bind as client at " << (cport) << endl;
        #endif
        int bound = client[i].bind(cport);
      }

      /* End of patch*/
      connected = client[i].connect(remote_url[i],port);
      #if PERF_REPORT > 1
        cout << "[" << i << "] Attempt to connect as client: " << connected << endl;
      #endif
    }

    if(!connected) {
      client[i].close();
      client[i].create();
      int bound = client[i].bind(port);
      #if PERF_REPORT > 1
        cout << "[" << i << "] Trying to bind as server at " << (port) << ". Result = " << bound << endl;
      #endif

      if(bound > 0) {
        client[i].listen();
        connected = client[i].accept(client[i]);
        #if PERF_REPORT > 1 
          cout <<  "[" << i << "] Attempt to act as server: " << connected << endl;
        #endif
        if(connected) { isclient[i] = 0; } 
      }
      else {
        cout << "Bind on ch #"<< i <<" failed: waiting for 1 s." << endl;
    	sleep(1);
      }
    }
  }
  return NULL;
}

/* Close down individual streams. */
void MPW_CloseChannels(int* channel, int numchannels) 
{
  for(int i=0; i<numchannels; i++) {
    cout << "Closing channel #" << channel[i] << " with port = " << port[i] << " and cport = " << cport[i] << endl;
    client[channel[i]].close();
    if(!isclient[channel[i]]) {
      client[channel[i]].closeServer();
    }
  }
}

/* Reopen individual streams. 
 * This is not required at startup. */
void MPW_ReOpenChannels(int* channel, int numchannels) 
{
  pthread_t streams[numchannels];
  init_tmp t[numchannels];

  for(int i = 0; i < numchannels; i++) {
    t[i].i    = channel[i];
    t[i].port = port[channel[i]];
    t[i].cport = cport[channel[i]];
    cout << "ReOpening client channel #" << channel[i] << " with port = " << port[channel[i]] << " and cport = " << cport[channel[i]] << endl;
    int code = pthread_create(&streams[i], NULL, MPW_InitStream, &t[i]);
  }

  for(int i = 0; i < numchannels; i++) {
    pthread_join(streams[i], NULL);
  }
}

//internal
void MPW_AddStreams(string* url, int* ports, int* cports, int numstreams) {
  num_streams += numstreams;

  for(int i = 0; i<numstreams; i++) {
    cout << "MPW_DNSResolve resolves " << url[i] << " to address " << MPW_DNSResolve(url[i]) << "." << endl;
    remote_url.push_back(MPW_DNSResolve(url[i]));
    client.push_back(Socket());
    isclient.push_back(1);
    port.push_back(ports[i]);
    cport.push_back(cports[i]);

    #if PERF_REPORT > 1
      cout << url[i] << " " << ports[i] << " " << cports[i] << endl;
    #endif

    if(url[i].compare("0") == 0) {
      isclient[i] = 0;
      cport[i]    = -2;
      cout << "Empty IP address given: Switching to Server-only mode." << endl;
    }
  }
}

void MPW_InitStreams(int *stream_indices, int numstreams) {
  pthread_t streams[numstreams];
  init_tmp t[numstreams];

  for(int i = 0; i < numstreams; i++) {
    t[i].i    = stream_indices[i];
    t[i].port = port[i];
    t[i].cport = cport[i];
    if(i>0) {
      int code = pthread_create(&streams[i], NULL, MPW_InitStream, &t[i]);
    }
  }
  if(numstreams > 0) {
    MPW_InitStream(&t[0]);
  }
  for(int i = 1; i < numstreams; i++) {
    pthread_join(streams[i], NULL);
  }

  if(MPWideAutoTune == 1) {
    for(unsigned int i=0; i<paths.size(); i++) {
      if(paths[i].num_streams < 3) {
        MPW_setPacingRate(1200*1024*1024);
      }
      else {
        MPW_setPacingRate((1200*1024*1024)/paths[i].num_streams);
      }
      for(int j=0; j<paths[i].num_streams; j++) {
        MPW_setWin(paths[i].streams[j] , 32*1024*1024/paths[i].num_streams);
      }
    }
  }

  cout << "-----------------------------------------------------------" << endl;
  cout << "MPWide Settings:" << endl;
  cout << "Chunk Size   (send/recv): " << tcpbuf_ssize << "/" << tcpbuf_rsize << endl;
  cout << "Relay pace   (send/recv): " << relay_ssize << "/" << relay_rsize << endl;
  cout << "Number of streams       : " << num_streams << endl;
  cout << "tcp buffer parameter    : " << WINSIZE << endl;
  cout << "pacing rate             : " << pacing_rate << " bytes/s." << endl;
  cout << "bandwidth monitoring    : " << MONITORING << endl;
  cout << "-----------------------------------------------------------" << endl;
  cout << "END OF SETUP PHASE." << endl;

  if(MPW_INITIALISED == false) {
    MPW_INITIALISED = true;
    #if MONITORING == 1
    pthread_t monitor;
    int code = pthread_create(&monitor, NULL, MPW_TBandwidth_Monitor, NULL);
    #endif
    /* Allocate global thread memory */
    ta = (thread_tmp *) MPWmalloc( sizeof(thread_tmp) * num_streams);
  }
  else {
    ta = (thread_tmp *) realloc(ta, sizeof(thread_tmp) * num_streams);
  }
}

/* Initialize the MPWide. set client to 1 for one machine, and to 0 for the other. */
void MPW_Init(string* url, int* ports, int* cports, int numstreams)
{
  #if PERF_REPORT > 0
    cout << "Initialising..." << endl;
  #endif

  MPW_EMPTY = (char*) MPWmalloc(1); //construct 'empty' buffer.

  int stream_indices[numstreams];
  for(int i=0; i<numstreams; i++) {
    stream_indices[i] = num_streams + i; //if this is the first MPW_Init, then num_streams still equals 0 here.
  }

  MPW_AddStreams(url, ports, cports, numstreams);
  MPW_InitStreams(stream_indices, numstreams);
}

/* Constructs a path. Return path id or negative error value. */
int MPW_CreatePathWithoutConnect(string host, int server_side_base_port, int streams_in_path) {
  int path_ports[streams_in_path];
  int path_cports[streams_in_path];
  string *hosts = new string[streams_in_path];
  int stream_indices[streams_in_path];
  for(int i=0; i<streams_in_path; i++) {
    path_ports[i] = server_side_base_port + i;
    path_cports[i] = -2;
    hosts[i] = host;
    stream_indices[i] = i + num_streams;
  }

  /* Add Path to paths Vector. */
  paths.push_back(MPWPath(host, stream_indices, streams_in_path));
  int path_id = paths.size()-1;
  MPW_AddStreams(hosts, path_ports, path_cports, streams_in_path);

  #if PERF_REPORT > 0
  cout << "Creating New Path:" << endl;
  cout << host << " " <<  server_side_base_port << " " << streams_in_path << " streams."  << endl;
    #if PERF_REPORT > 1
    for(int i=0; i<streams_in_path; i++) {
      cout << "Stream[" << i << "]: " << paths[paths.size()-1].streams[i] << endl;
    } 
    #endif
  #endif

  /* Return the identifier for the MPWPath we just created. */
  return path_id;
}

/* Connects a path. */
int MPW_ConnectPath(int path_id) {
  MPW_InitStreams(paths[path_id].streams, paths[path_id].num_streams);
  return 0;
}

/* Creates and connects a path */
int MPW_CreatePath(string host, int server_side_base_port, int streams_in_path) {

  int path_id = MPW_CreatePathWithoutConnect(host, server_side_base_port, streams_in_path);
  MPW_ConnectPath(path_id);
  return path_id;
}

void DecrementStreamIndices(int q) {
  for(unsigned int i=0; i<paths.size(); i++) {
    for(int j=0; j<paths[i].num_streams; j++) {
      if(paths[i].streams[j] > q) { 
        paths[i].streams[j]--; 
      }
    }
  }
}

void EraseStream(int i) {
  port.erase(port.begin()+i);
  cport.erase(port.begin()+i);
  isclient.erase(isclient.begin()+i);
  client.erase(client.begin()+i);
  remote_url.erase(remote_url.begin()+i);
  num_streams--;
}

void MPW_setWin(int channel, int size) {
  client[channel].setWin(size);
}

void MPW_setPathWin(int path, int size) {
  for(int i=0; i < paths[path].num_streams; i++) {
    client[paths[path].streams[i]].setWin(size);
  }
}

// Return 0 on success (negative on failure).
int MPW_DestroyPath(int path) {
  MPW_CloseChannels(paths[path].streams, paths[path].num_streams);
  for(int i=0; i < paths[path].num_streams; i++) {
    EraseStream(paths[path].streams[i]);
    DecrementStreamIndices(i);
  }
  paths.erase(paths.begin()+path);
}

/* Path-based Send and Recv operations*/
int MPW_DSendRecv(char* sendbuf, long long int sendsize, char* recvbuf, long long int maxrecvsize, int path) {
  return MPW_DSendRecv(sendbuf, sendsize, recvbuf, maxrecvsize, paths[path].streams, paths[path].num_streams);
}

void MPW_SendRecv(char* sendbuf, long long int sendsize, char* recvbuf, long long int recvsize, int path) {
  MPW_SendRecv(sendbuf, sendsize, recvbuf, recvsize,  paths[path].streams, paths[path].num_streams);
}

void MPW_Send(char* sendbuf, long long int sendsize, int path) {
  MPW_SendRecv(sendbuf, sendsize, MPW_EMPTY, 1, paths[path].streams, paths[path].num_streams);
}

void MPW_Recv(char* recvbuf, long long int recvsize, int path) {
  MPW_SendRecv(MPW_EMPTY, 1, recvbuf, recvsize,  paths[path].streams, paths[path].num_streams);
}


/* Variant that does not require client port binding. */
void MPW_Init(string* url, int* ports, int numstreams) 
{
  int cports[numstreams];

  for(int i=0; i<numstreams; i++) {
    cports[i] = -1;
  }
  MPW_Init(url, ports, cports, numstreams);
}

/* Shorthand initialization call for local processes that use a single stream. */
void MPW_Init(string url, int port) {
  string u1[1] = {url};
  int    p1[1] = {port};
  MPW_Init(u1,p1,1);
}

extern "C" {
  void MPW_Init_c (char** url, int* ports, int numstreams) 
  {
    string urls[numstreams];
    for(int i=0;i<numstreams;i++) {
      urls[i].assign(url[i]);
    }
    MPW_Init(urls,ports,numstreams);
  }

  void MPW_Init1_c (char* url, int port)
  {
    MPW_Init(url, port);
  }
}

/* Shorthand initialization call for inter-cluster single-stream usage. */
void MPW_Init(string url) {
  string u1[1] = {url};
  int    p1[1] = {6000};
  MPW_Init(u1,p1,1);
}

/* Close all sockets and free data structures related to the library. */
int MPW_Finalize()
{
  stop_monitor = true;
  for(int i=0; i<num_streams; i++) {
    client[i].close();
    if(!isclient[i]) {
      client[i].closeServer();
    }
  }
  #if PERFREPORT > 0
  cout << "MPWide sockets are closed." << endl;
  #endif
  free(ta); //clean global thread memory
  free(MPW_EMPTY);
  sleep(1);
  return 1;
}



/* Wrapping function for SendRecv in case no receiving is required. */
void MPW_Send(char* sendbuf, long long int size, int* channels, int num_channels)
{
  MPW_SendRecv(sendbuf,size,MPW_EMPTY,1,channels,num_channels);
}

void MPW_Recv(char* buf, long long int size, int* channels, int num_channels)
{
  MPW_SendRecv(MPW_EMPTY,1,buf,size,channels,num_channels);
}

/* Send/Recv between two processes. */
void InThreadSendRecv(char* sendbuf, long long int sendsize, char* recvbuf, long long int recvsize, int base_channel)
{

#ifdef PERF_TIMING
  double t = GetTime();
#endif

  long long int a = 0;
  long long int b = 0;

  int channel = base_channel % 65536;
  int channel2 = channel;
  if(base_channel > 65535) {
    channel2 = (base_channel/65536) - 1;
    client[channel2].set_non_blocking(true);
  }
  client[channel].set_non_blocking(true); 

  bool rdone = false;
  bool wdone = false;

  while((a < sendsize) || (b < recvsize)) {

    int mode = 0;
    int mask = 0;
    mode = selectSockets(channel,channel2,mask);

    if(!rdone && (mode%2==1)) {
      if((recvsize-b)) {
        int n = client[channel2].irecv(recvbuf+b,min(tcpbuf_rsize,recvsize-b)); 
        b += n;
	    #if MONITORING == 1
	    bytes_sent += n;
	    #endif
      }
      if(!(recvsize-b)) {
        mask++; //don't check for read anymore
        rdone = true;
      }
    }

    if(!wdone && (mode/2==1)) {
      if(sendsize-a) {
        int n = client[channel].isend(sendbuf+a,min(tcpbuf_ssize,sendsize-a)); 

        a += n;
        #if MONITORING == 1
        bytes_sent += n;
        #endif
      }
      if(!(sendsize-a)) {
        mask += 2; //don't check for write anymore
        wdone = true;
      }
    }

    #if PacingMode == 1
    usleep(pacing_sleeptime);
    #endif
  }

  #ifdef PERF_TIMING
  t = GetTime() - t;
    #if PERF_REPORT > 2
    cout << "This Send/Recv took: " << t << "s. Rate: " << (sendsize+recvsize)/(t*1024*1024) << "MB/s. ch=" << channel << "/" << channel2 << endl;
    #endif
  SendRecvTime += t;
  #endif
}

typedef struct relay_struct{
  int channel;
  int channel2;
  int bufsize;
}relay_struct;

/* MPWide Relay function: Provides two-way relay over one channel. */
void* MPW_Relay(void* args) 
{
  relay_struct *r = (relay_struct *)args;

  int mode  = 0;
  int mode2 = 0;
  long long int n     = 0;
  long long int ns    = 0;
  long long int n2    = 0;
  long long int ns2   = 0;

  int   channel  = r->channel;
  int   channel2 = r->channel2;
  int   bufsize  = r->bufsize;
  
  char* buf      = (char *) malloc(bufsize);
  char* buf2     = (char *) malloc(bufsize);
  
  client[channel].set_non_blocking(true);   
  client[channel2].set_non_blocking(true); 

  #if PERF_REPORT > 1
  cout << "Starting Relay Channel #" << channel << endl;
  #endif
  int tmp = 0;

  while(1) {

    mode  = client[channel].select_me(0);
    mode2 = client[channel2].select_me(0);
    //cout << "mode/mode2 = " << mode << "/" << mode2 << "--" << n << "/" << n2 << "/" << ns << "/" << ns2 <<endl;

    /* Recv from channel 1 */
    if(mode%2 == 1) {
      if(n == ns) {
        n = 0; ns = 0;
      }  
      tmp = client[channel].irecv(buf+n,min(bufsize-n,relay_rsize));
      n += tmp; 
      #if PERF_REPORT == 4
      cout << "Retrieved from 1: " << n << endl;
      #endif
    }
    /* ...forward to channel 2. */
    if(mode2/2 == 1 && n > 0) {
      tmp = client[channel2].isend(buf+ns,min(n-ns,relay_ssize));
      ns += tmp;
      #if PERF_REPORT == 4
      cout << "Sent to 2: " << ns << endl;
      #endif
    }
    
    // Recv from channel 2 
    if(mode2%2 == 1) {  
      if(n2 == ns2) {
        n2 = 0; ns2 = 0;
      }
      tmp = client[channel2].irecv(buf2+n2,min(bufsize-n2,relay_rsize));
      n2 += tmp;
      #if PERF_REPORT == 4
      cout << "Retrieved from 2: " << n2 << endl;
      #endif
    }
    // ...forward to channel 1. 
    if(mode/2 == 1 && n2 > 0) {
      tmp = client[channel].isend(buf2+ns2,min(n2-ns2,relay_ssize));
      ns2 += tmp;
      #if PERF_REPORT == 4
      cout << "Sent to 1: " << ns2 << endl;
      #endif
    }
    
    #if PacingMode == 1
    usleep(pacing_sleeptime);
    #endif
  }
  
  free(buf);
  free(buf2);
}

void* CheckStop(void* args) {
  while(true) {
    if(!access("stop",F_OK)) {
      MPW_Finalize();
      remove("stop");
      exit(0);
    }
    sleep(2);
  }
}

/* MPW_Relay: 
 * redirects [num_channels] streams in [channels] to the respective 
 * streams in [channels2] and vice versa. */
void MPW_Relay(int* channels, int* channels2, int num_channels) {
  int bufsize = max(relay_ssize,relay_rsize);

  pthread_t streams[num_channels*2];
  relay_struct rstruct[num_channels*2];
  
  for(int i = 0; i < num_channels; i++) {
    rstruct[i].channel  = channels[i];
    rstruct[i].channel2 = channels2[i];
    rstruct[i].bufsize  = bufsize;
    int code = pthread_create(&streams[i], NULL, MPW_Relay, &rstruct[i]); 
  }
  
  for(int i = 0; i < num_channels*2; i++) {
    pthread_join(streams[i], NULL);
  }  
  return;
}

/* Dynamically sized Send/Recv between two processes. */
void *MPW_TDynEx(void *args)
{
//  cout << "TDynEx." << endl;
  double t = GetTime();
  bool cycling = false;
  thread_tmp *ta = (thread_tmp *)args;

  if (ta->channel > 65535) { cycling = true; }

  char * sendbuf = ta->sendbuf;
  long long int totalsendsize = ta->sendsize;
  long long int recvsize = ta->recvsize;
  // Maximum size permitted for message.
  long long int maxrecvsize = recvsize;
  char* recvbuf = ta->recvbuf;
  int channel = ta->channel % 65536; //send channel

  int channel2 = channel; //recv channel
  client[channel].set_non_blocking(true);
  if(cycling) { 
    channel2 = (ta->channel / 65536) - 1; 
    client[channel2].set_non_blocking(true);
  }

  int id = ta->thread_id;
  long long int numschannels = ta->numchannels;
  long long int numrchannels = ta->numrchannels;

  long long int sendsize = totalsendsize / numschannels;
  if(id < (totalsendsize % numschannels)) {
    sendsize++;
  }

  #if SendRecvInputReport == 2
  cout << "TDynEx(sendsize="<<sendsize<<",maxrecvsize="<<maxrecvsize<<",ch_send="<<channel<<",ch_recv="<<channel2<<",nc="<<numschannels<<"/"<<numrchannels<<endl;
  #endif

  long long int size_found = -1;
  long long int recvsizeall = -1;

  long long int a,b,c,d;
  a = b = c = d = 0;

  // recvsize is initially set to the size of the long long int which holds the message size.
  bool recv_settings_known = false;   //this thread knows how much data may be received.
  int mask = 0;
  int mode = 0;
  long long int offset_r = 0; //stores correct recv buffer offset for this thread.

  /* Second: await the recvsize */
//  if(id < numrchannels) {

//  cout << "Receiving size from channel " << channel2 << ", id: " << id << ", numrchannels: " << numrchannels << endl;

  while(recvsize > d || sendsize > c) {
    mode = selectSockets(channel,channel2,mask);

    /* (1.) Receiving is possible, but only done by thread 0 until we know more. */
    if(mode%2 == 1) {
      if(!recv_settings_known) {
        int n = client[channel2].irecv(((char*) (&size_found))+b,8-b); 
        b += n;

        if(b == 8) { //recvsize data is now available.
          #ifdef EndianConvert
          size_found = swapLLI(size_found);
          #endif

          if (size_found > maxrecvsize) { //Would we want to do reallocs here???
            cerr << "ERROR: DynEx recv size is greater than given constraint." << endl;
            cerr << "(Size found = " << size_found << " bytes. Maxrecvsize = " << maxrecvsize << " bytes.)" << endl;
            cerr << "(Channel: " << channel <<", Totalsendsize = "<< totalsendsize <<")" << endl;
            cerr << "Going to sleep, so trace can be done. Press Ctrl-c to exit." << endl;
            while(1) { sleep(1); }
          }
          recvsizeall = size_found;
          if(id == 0) {
            *(ta->dyn_recvsize) = size_found;
          }

          recvsize = recvsizeall / numrchannels;
          if(id < (recvsizeall % numrchannels)) {
            recvsize++;
          }

          if(id < numrchannels) {
            offset_r = ((size_found / numrchannels) * id) + min(id,size_found % numrchannels);
            recvbuf = &(ta->recvbuf[offset_r]);
          } 
 
          recv_settings_known = true;
        }
      } 
      else {
        int n = client[channel2].irecv(recvbuf+d,min(tcpbuf_rsize,recvsize-d));
        d += n;
        #if MONITORING == 1
        bytes_sent += n;
        #endif
        if(recvsize == d) { mask++; }
      }
    }

    /* SENDING POSSIBLE */
    if(mode/2==1) {
      if(a<8) { //send size first.
        long long int tmpsize = totalsendsize;

        #ifdef EndianConvert
        tmpsize = swapLLI(tmpsize);
        #endif
        int n = client[channel].isend(((char*) (&tmpsize))+a,8-a);
        a += n;
      }
      else { //send data after that, leave 16byte margin to prevent SendRecv from crashing.
        int n = client[channel].isend(sendbuf+c,min(tcpbuf_ssize,sendsize-c)); 
        c += n;
        #if MONITORING == 1
        bytes_sent += n;
        #endif

        if(sendsize == c) {
          mask += 2; //don't check for write anymore
        }
      }
    }
    #if PacingMode == 1
    usleep(pacing_sleeptime);
    #endif
  }

  return NULL;
}

/* Better version of TSendRecv */
void *MPW_TSendRecv(void *args)
{
  thread_tmp *t = (thread_tmp *)args;
  InThreadSendRecv(t->sendbuf, t->sendsize, t->recvbuf, t->recvsize, t->channel);

  return NULL;
}

/* DSendRecv: MPWide Low-level dynamic exchange. 
 * In this exchange, the message size is automatically appended to the data. 
 * The size is first read by the receiving process, which then reads in the
 * appopriate amount of memory.
 * The actual size of the received data in each stream is stored in recvsize, 
 * whereas the total size is returned as a long long int.
 *
 * Note: DSendRecv assumes that sendbuf has been split into equal-sized chunks by
 * using MPW_splitBuf in this file. If the splitting is non-equal for some reason, 
 * this function will hang.
 * */

long long int DSendRecv(char** sendbuf, long long int totalsendsize, char* recvbuf, long long int maxrecvsize, int* channel, int num_channels) {
#ifdef PERF_TIMING
  double t = GetTime();
#endif
  //cout << sendbuf[0] << " / " << recvbuf[0] << " / " << num_channels << " / " << sendsize[0] << " / " << recvsize[0] << " / " << channel[0] << endl;

  pthread_t streams[num_channels];
  long long int dyn_recvsize = 0;

  for(int i=0; i<num_channels; i++){
      ta[channel[i]].sendsize = totalsendsize;
      ta[channel[i]].recvsize = maxrecvsize;
      ta[channel[i]].dyn_recvsize = &dyn_recvsize; //one recvsize stored centrally. Read in by thread 0.
      ta[channel[i]].channel = channel[i];
      ta[channel[i]].sendbuf = sendbuf[i];
      ta[channel[i]].recvbuf = recvbuf;
      ta[channel[i]].thread_id = i;
      ta[channel[i]].numchannels = num_channels;
      ta[channel[i]].numrchannels = num_channels;
      if(i>0) {
        int code = pthread_create(&streams[i], NULL, MPW_TDynEx, &ta[channel[i]]);
      }
  }

  MPW_TDynEx(&ta[channel[0]]);

  for(int i=1; i<num_channels; i++) {
    pthread_join(streams[i], NULL);
  }

#ifdef PERF_TIMING
  t = GetTime() - t;
#endif

  long long int total_size = totalsendsize + dyn_recvsize;

#ifdef PERF_TIMING
  #if PERF_REPORT>0
  cout << "DSendRecv: " << t << "s. Size: " << (total_size/(1024*1024)) << "MB. Rate: " << total_size/(t*1024*1024) << "MB/s." << endl;
  #endif
  SendRecvTime += t;
#endif

  return ta[channel[0]].dyn_recvsize[0];

}

/* buf,bsize and num_chunks contain parameters.
 * split_buf and chunksizes are placeholders for the splitted buffer and its properties. */
//TODO: Introduce a split in 4096-byte chunks using an ifdef mechanism?
void MPW_splitBuf(char* buf, long long int bsize, int num_chunks, char** split_buf, long long int* chunk_sizes) {

  if(num_chunks < 1) { 
    cout << "ERROR: MPW_splitBuf is about to split into 0 chunks." << endl;
    exit(0);
  }
  long long int bsize_each = bsize / num_chunks;
  int bsize_each_odd = bsize % num_chunks;
  long long int offset = 0;
  int ii = 0;

  for(int i=0; i<num_chunks; i++) {
    ii = bsize_each;
    if( i < bsize_each_odd)  ii ++;
    split_buf[i] = &buf[offset];
    chunk_sizes[i] = ii;
    offset += ii;
  }
}

/* Split streams and exchange the message using dynamic sizing. */
long long int MPW_DSendRecv( char *sendbuf, long long int sendsize,
                char *recvbuf, long long int maxrecvsize,
                int *channel, int nc){

  char **sendbuf2 = new char*[nc];
  long long int *sendsize2 = new long long int[nc];

  MPW_splitBuf(sendbuf,sendsize,nc,sendbuf2,sendsize2);
  long long int total_recv_size = DSendRecv( sendbuf2, sendsize, recvbuf, maxrecvsize, channel, nc);

  delete [] sendbuf2;
  delete [] sendsize2;
  return total_recv_size;
}

/* Low-level command */
long long int Cycle(char**  sendbuf2, long long int sendsize2, char* recvbuf2, long long int maxrecvsize2, int* ch_send, int nc_send, int* ch_recv, int nc_recv, bool dynamic) {
  #ifdef PERF_TIMING
  double t = GetTime();
  #endif
  pthread_t streams[nc_send];
  char dummy_recv[nc_recv];
  char dummy_send[nc_send][1];

  long long int totalsendsize = sendsize2;

  long long int dyn_recvsize_sendchannel = 0; 
  long long int dyn_recvsize_recvchannel = 0; 

  long long int recv_offset = 0; //only if !dynamic

  //TODO: Add support for different number of send/recv streams.
  for(int i=0; i<max(nc_send,nc_recv); i++){

    if(totalsendsize>0 && i<nc_send) {
      if(dynamic) { //overall sendsize given to all threads.
        ta[i].sendsize = totalsendsize;
      } else { //1 sendsize separately for each thread.
        ta[i].sendsize = totalsendsize / nc_send;
        if(i < (totalsendsize % nc_send)) {
          ta[i].sendsize++;
        }
      }
      ta[i].sendbuf = sendbuf2[i];
    }
    else {
      ta[i].sendsize = 1*nc_send;
      ta[i].sendbuf = dummy_send[i];
    }

    if(maxrecvsize2>0 && i<nc_recv) {
      if(dynamic) { //one recvbuf and size limit for all threads.
        ta[i].recvsize = maxrecvsize2;
        ta[i].recvbuf = recvbuf2;
      } else { //assign separate and fixed-size recv bufs for each thread.
        ta[i].recvbuf = &(recvbuf2[recv_offset]);
        ta[i].recvsize = maxrecvsize2 / nc_recv;
        if(i<(maxrecvsize2 % nc_recv)) { ta[i].recvsize++; }
        recv_offset += ta[i].recvsize;
      }
    }
    else {
      ta[i].recvsize = 1*nc_recv;
      ta[i].recvbuf = dummy_recv;
    }

    ta[i].dyn_recvsize = &dyn_recvsize_sendchannel; //one recvsize stored centrally. Read in by thread 0.
    ta[i].channel = 0;
    if(i<nc_send) {
      ta[i].channel = ch_send[i]; 
    }
    if(i<nc_recv) {
      if(i<nc_send) {
        ta[i].channel += ((ch_recv[i]+1)*65536);
      }
      else {
        ta[i].channel = ch_recv[i];
      }
    }

    ta[i].thread_id = i;
    ta[i].numchannels  = nc_send;
    ta[i].numrchannels = nc_recv;
    //printThreadTmp(ta[i]);
    if(i>0) {
      if(dynamic) {
        int code = pthread_create(&streams[i], NULL, MPW_TDynEx, &ta[i]);
      } else {
        int code = pthread_create(&streams[i], NULL, MPW_TSendRecv, &ta[i]);
      }
    }
  }

  if(dynamic) {
    MPW_TDynEx(&ta[0]);
  } else {
    MPW_TSendRecv(&ta[0]);
  }

  if(max(nc_send,nc_recv)>1) {
    for(int i=1; i<max(nc_send,nc_recv); i++) {
      pthread_join(streams[i], NULL);
    }
  }

  #ifdef PERF_TIMING
  t = GetTime() - t;
  #endif

  long long int total_size = 0;
  total_size     = sendsize2 + dyn_recvsize_recvchannel;

  #ifdef PERF_TIMING
    #if PERF_REPORT>0
      cout << "Cycle: " << t << "s. Size: " << (total_size/(1024*1024)) << "MB. Rate: " << total_size/(t*1024*1024) << "MB/s." << endl;
    #endif
    SendRecvTime += t;
  #endif

//  return dyn_recvsize_recvchannel;
  return (ta[0].dyn_recvsize)[0];
}

/* Recv from one set of channels. Send through to another set of channels. */
long long int MPW_Cycle(char* sendbuf, long long int sendsize, char* recvbuf, long long int maxrecvsize,
             int* ch_send, int num_ch_send, int* ch_recv, int num_ch_recv, bool dynamic) 
{
  #if SendRecvInputReport == 1
  if(dynamic) {
    cout << "MPW_DCycle(sendsize="<<sendsize<<",maxrecvsize="<<maxrecvsize<<",ncsend="<<num_ch_send<<",ncrecv="<<num_ch_recv<<");"<<endl;
  } else {
    cout << "MPW_Cycle(sendsize="<<sendsize<<",recvsize="<<maxrecvsize<<",ncsend="<<num_ch_send<<",ncrecv="<<num_ch_recv<<");"<<endl;
  }
  for(int i=0; i<num_ch_send; i++) {
    cout << "send channel " << i << ": " << ch_send[i] << endl;
  }
  for(int i=0; i<num_ch_recv; i++) {
    cout << "recv channel " << i << ": " << ch_recv[i] << endl;
  }
  #endif

  if(sendsize<1 && maxrecvsize<1) {
    if(sendsize == 0 && maxrecvsize == 0) {
//      cout << "MPW_Cycle: called with empty send/recv buffers. Skipping transfer.\n" << endl;
    }
    else {
      cout << "MPW_Cycle error: sendsize = " << sendsize << ", maxrecvsize = " << maxrecvsize << endl;
      exit(-1);
    }
  }
//  cout << "MPW_Cycle: " << sendsize << "/" << maxrecvsize << "/" << ch_send[0] << "/" << num_ch_send << "/" << ch_recv[0] << "/" << num_ch_recv << endl;
  char **sendbuf2 = new char*[num_ch_send];
  long long int *sendsize2    = new long long int[num_ch_send]; //unused by Cycle.

  MPW_splitBuf( sendbuf, sendsize, num_ch_send, sendbuf2, sendsize2);

  long long int total_recv_size = Cycle( sendbuf2, sendsize, recvbuf, maxrecvsize, ch_send, num_ch_send, ch_recv, num_ch_recv, dynamic);

  delete [] sendbuf2;
  delete [] sendsize2;
  return total_recv_size;
}

long long int MPW_DCycle(char* sendbuf, long long int sendsize, char* recvbuf, long long int maxrecvsize,
             int* ch_send, int num_ch_send, int* ch_recv, int num_ch_recv)
{
  return MPW_Cycle(sendbuf, sendsize, recvbuf, maxrecvsize, ch_send, num_ch_send, ch_recv, num_ch_recv, true);
}

void MPW_Cycle(char* sendbuf, long long int sendsize, char* recvbuf, long long int maxrecvsize,
             int* ch_send, int num_ch_send, int* ch_recv, int num_ch_recv)
{
  MPW_Cycle(sendbuf, sendsize, recvbuf, maxrecvsize, ch_send, num_ch_send, ch_recv, num_ch_recv, false);
}

void MPW_PSendRecv(char** sendbuf, long long int* sendsize, char** recvbuf, long long int* recvsize, int* channel, int num_channels)
{
#ifdef PERF_TIMING
  double t = GetTime();
#endif

  //cout << sendbuf[0] << " / " << recvbuf[0] << " / " << num_channels << " / " << sendsize[0] << " / " << recvsize[0] << " / " << channel[0] << endl;
  pthread_t streams[num_channels];

  for(int i=0; i<num_channels; i++){
    ta[i].sendsize = sendsize[i];
    ta[i].recvsize = recvsize[i];
    ta[i].channel = channel[i];
    ta[i].sendbuf = sendbuf[i];
    ta[i].recvbuf = recvbuf[i];

    if(i>0) {
      int code = pthread_create(&streams[i], NULL, MPW_TSendRecv, &ta[i]);
    }
  }

  MPW_TSendRecv(&ta[0]);

  if(num_channels>1) {
    for(int i=1; i<num_channels; i++) {
      pthread_join(streams[i], NULL);
    }
  }

  #ifdef PERF_TIMING
    t = GetTime() - t;
    long long int total_size = 0;
    for(int i=0;i<num_channels;i++) {
      total_size += sendsize[i]+recvsize[i];
    }

    #if PERF_REPORT>0
      cout << "PSendRecv: " << t << "s. Size: " << (total_size/(1024*1024)) << "MB. Rate: " << total_size/(t*1024*1024) << "MB/s." << endl;
    #endif
    SendRecvTime += t;
  #endif
}

void MPW_SendRecv( char* sendbuf, long long int sendsize, char* recvbuf, long long int recvsize, int* channel, int nc){

#if SendRecvInputReport == 1
  cout << "MPW_SendRecv(sendsize=" << sendsize << ",recvsize=" << recvsize << ",nc=" << nc << ");" << endl;
  for(int i=0; i<nc; i++) {
    cout << "channel " << i << ": " << channel[i] << endl;
  }
#endif

  long long int sendsize_each = sendsize / nc;
  long long int recvsize_each = recvsize / nc;

  int sendsize_each_odd = sendsize % nc;
  int recvsize_each_odd = recvsize % nc;

  char **sendbuf2 = new char*[nc];
  char **recvbuf2 = new char*[nc];
  long long int *sendsize2 = new long long int[nc];
  long long int *recvsize2 = new long long int[nc];
  
  int offset = 0;
  int offset2 = 0;
  int ii=0;
  int iii=0;
  
  for( int i=0; i<nc; i++){
    ii = sendsize_each;
    if( i < sendsize_each_odd)  ii ++;
    sendbuf2[i] = &sendbuf[offset];
    sendsize2[i] = ii;
    offset += ii;

    iii = recvsize_each;
    if( i < recvsize_each_odd)  iii ++;
    recvbuf2[i] = &recvbuf[offset2];
    recvsize2[i] = iii;
    offset2 += iii;
  }

  MPW_PSendRecv( sendbuf2, sendsize2, recvbuf2, recvsize2, channel, nc);

  delete [] sendbuf2;
  delete [] recvbuf2;
  delete [] sendsize2;
  delete [] recvsize2;
}

/* Synchronization functions: try to minimize the use of this function. */
void MPW_Barrier(int channel)
{
  #ifdef PERF_TIMING
    double t = GetTime();
  #endif

  int i = channel;
  char s[8];
  
  if(isclient[i]) {
    client[i].send("Test 1!",8);  
    client[i].recv(s,8);
  }
  else {
    client[i].recv(s,8);
    client[i].send(s,8);
  }
  #ifdef PERF_TIMING
    t = GetTime() - t;
    BarrierTime += t;
  #endif
}

void *MPW_Barrier(void* args) {
  int ch_index = ((int*) args)[0];
  MPW_Barrier(ch_index);

  return NULL;
}

/* MPW_Barrier on all active connections. */
void MPW_Barrier() {
  pthread_t streams[num_streams];
  int ch_index[num_streams]; 

  for(int i=0; i<num_streams; i++){
    ch_index[i] = i;
    int code = pthread_create(&streams[i], NULL, MPW_Barrier, &(ch_index[i]));
  }
  for(int i=0; i<num_streams; i++) {
    pthread_join(streams[i], NULL);
  }
}

/* C interface */
extern "C" { 
  void MPW_SendRecv1_c (char* sendbuf, long long int sendsize, char* recvbuf, long long int recvsize, int base_channel) {
    //string urls(url)
    MPW_SendRecv(sendbuf, sendsize, recvbuf, recvsize, base_channel);
  }
  void MPW_SendRecv_c (char* sendbuf, long long int sendsize, char* recvbuf, long long int recvsize, int* base_channel, int num_channels) {
    //string urls(url)
    MPW_SendRecv(sendbuf, sendsize, recvbuf, recvsize, base_channel, num_channels);
  }
  void MPW_PSendRecv_c(char** sendbuf, long long int* sendsize, char** recvbuf, long long int* recvsize, int* channel, int num_channels) {
    MPW_PSendRecv(sendbuf, sendsize, recvbuf, recvsize, channel, num_channels);
  }
}

/* Diagnostics */
void printThreadTmp (thread_tmp t) { //print thread specific information.
  cout << "Thread #" << t.thread_id << " of " << t.numchannels << " send channels and " << t.numrchannels << " recv channels." << endl;
  cout << "Sendsize: " << t.sendsize << ", Recvsize: " << t.recvsize << endl;
  cout << "DynRecvsize: " << t.dyn_recvsize[0] << ", channel: " << t.channel%65536 << "," << (t.channel/65536)-1 << endl;
}
void MPW_Print() { //print MPWide stream specific information.
  for(int i=0;i<num_streams;i++) {
    fprintf( stderr, "MPWide connection stream #: %d\n", i);
    fprintf( stderr, "MPWide base connection port:%d\n", port[i]);
    fprintf( stderr, "MPWide remote_url:%s\n", remote_url[i].c_str());
  }
  fflush(stderr);
}


/* Non-blocking extension */

typedef struct MPW_NBE {
  pthread_t pthr_id;
  int id;
  thread_tmp NBE_args;
}MPW_NBE;

static vector<MPW_NBE> MPW_nonBlockingExchanges;

/* A TSendRecv that encapsulates a full MPW_SendRecv. */
void *MPW_TSendRecv_Full(void *args)
{
  thread_tmp *t = (thread_tmp *)args;
  
  MPW_SendRecv(t->sendbuf, t->sendsize, t->recvbuf, t->recvsize, t->channel); 
  //NOTE: the channel variable in the thread_tmp structure is repurposed as a path variable here (both are of 'int' datatype). 
  //It also gets changed to a negative number whenever the SendRecv is completed.
  
  t->channel = (t->channel+1)*-1; //turn channel into a negative number to indicate completion.
  return NULL;
}


int MPW_ISendRecv( char* sendbuf, long long int sendsize, char* recvbuf, long long int recvsize, int path) {
  MPW_NBE new_nonblocking_exchange = MPW_NBE();
  new_nonblocking_exchange.NBE_args = thread_tmp();
  new_nonblocking_exchange.NBE_args.sendbuf = sendbuf;
  new_nonblocking_exchange.NBE_args.sendsize = sendsize;
  new_nonblocking_exchange.NBE_args.recvbuf = recvbuf;
  new_nonblocking_exchange.NBE_args.recvsize = recvsize;
  new_nonblocking_exchange.NBE_args.channel = path;
   
  if(MPW_nonBlockingExchanges.size() == 0) { //no other non-blocking comms? Use id 0.
    new_nonblocking_exchange.id = 0;
  }
  else { //generate a new unique incremented id.
    new_nonblocking_exchange.id = MPW_nonBlockingExchanges[MPW_nonBlockingExchanges.size()-1].id + 1;
  }
  
  MPW_nonBlockingExchanges.push_back(new_nonblocking_exchange);
  MPW_nonBlockingExchanges[MPW_nonBlockingExchanges.size()-1].NBE_args = new_nonblocking_exchange.NBE_args;
  
  int code = pthread_create(&(MPW_nonBlockingExchanges[MPW_nonBlockingExchanges.size()-1].pthr_id), 
                            NULL, MPW_TSendRecv_Full, &(MPW_nonBlockingExchanges[MPW_nonBlockingExchanges.size()-1].NBE_args));
                            
  return new_nonblocking_exchange.id;
}

int Find_NBE_By_ID(int NBE_id) {
  int element_number = -1;
  for(int i=0; i<MPW_nonBlockingExchanges.size(); i++) {
    if(MPW_nonBlockingExchanges[i].id == NBE_id) {
      element_number = i;
    }
  }
  if(element_number < 0) {
    cout << "WARNING: you used a non existent NonBlockingExchange ID number in MPW_Wait." << endl;
  }
  return element_number;
}

bool MPW_Has_NBE_Finished(int NBE_id) {
  int element_number = Find_NBE_By_ID(NBE_id);
  return(MPW_nonBlockingExchanges[element_number].NBE_args.channel >= 0);
}

void MPW_Wait(int NBE_id) {
  int element_number = Find_NBE_By_ID(NBE_id);  
  pthread_join(MPW_nonBlockingExchanges[element_number].pthr_id, NULL);
  MPW_nonBlockingExchanges.erase(MPW_nonBlockingExchanges.begin()+element_number);
}
