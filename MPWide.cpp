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
#include "MPWide.h"
#include "Socket.h"

#include <iostream>
#include <fstream>
#include <string>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>
#include <cstdlib>
#include <vector>
#include <unistd.h>

#include "serialization.h"
#include "mpwide-macros.h"

// forward declarations
class MPWPath;
struct thread_tmp;

bool MPWideAutoTune = true;

/* STREAM-specific definitions */
static int *port = NULL;
static int *cport = NULL;
static int *isclient = NULL;
static Socket **client = NULL;
static std::string *remote_url = NULL;

/* global thread memory */
static thread_tmp** ta = NULL;

// length of all the above vectors:
static int num_streams = 0;

/* List of paths. */
static MPWPath **paths = NULL;
static int num_paths = 0;

/* Send and Recv occurs in chunks of size tcpbuf_ssize/rsize.
 * Setting this to 1MB or higher gave problems with Amsterdam-Drexel test. */
static int tcpbuf_ssize = 8*1024;
static int tcpbuf_rsize = 8*1024;
static int relay_ssize = 8*1024;
static int relay_rsize = 8*1024;

/* PATH-specific definitions */
class MPWPath {
public:
  std::string remote_url; // end-point of the path
  int *streams; // id numbers of the streams used
  int num_streams; // number of streams
  
  MPWPath(std::string remote_url, int* str, int numstr)
  : remote_url(remote_url), num_streams(numstr)
  {
    streams = new int[numstr];
    memcpy(streams, str, num_streams*sizeof(int));
  }
  ~MPWPath() { delete [] streams; }
};

/* thread information */
struct thread_tmp {
  long long int sendsize, recvsize;
  long long int* dyn_recvsize; //For DynEx.
  int thread_id;
  int channel;
  int numchannels;
  int numrchannels; //Cycle only.
  char* sendbuf;
  char* recvbuf;
};

/* socket startup information */
struct init_tmp {
  int stream;
  Socket *sock;
  int port;
  int cport;
  bool server_wait;
  bool connected;
};

/** MPW_PacingMode
  * MPWide is able to have PacingMode either enabled or disabled. By default, the PacingMode is enabled, and MPWide will insert very 
  * short usleep messages between communication calls. These usleep statements often help reduce the chance of overflowing the transfer 
  * buffers of local network interfaces, which in turn result in worse and less stable performance.
  */
#if MPW_PacingMode == 1
  static double pacing_rate = 100*1024*1024; //Pacing rate per stream. This is the maximum throughput in bytes/sec possible for each stream.
  static useconds_t pacing_sleeptime = useconds_t(1000000/(pacing_rate/(1.0*tcpbuf_ssize))); //Sleep time for SendRecvs in microseconds.

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
      pacing_sleeptime = useconds_t(1000000/(pacing_rate/(1.0*tcpbuf_ssize)));
      LOG_INFO("Pacing enabled, rate = " << pacing_rate << " => delay = " << pacing_sleeptime << " us.");
    }
  }

  /* autotunePacingRate selects an appropriate pacing rate depending on the number of streams selected. */
  static void autotunePacingRate()
  {
    int max_streams = 0;
    for(int i = 0; i < num_paths; i++)
    {
      if (paths[i] && paths[i]->num_streams > max_streams)
        max_streams = paths[i]->num_streams;
    }
    
    if (max_streams < 3)
      MPW_setPacingRate(1200*1024*1024);
    else
      MPW_setPacingRate((1200*1024*1024)/max_streams);
  }
#endif // MPW_PacingMode == 1

void MPW_setAutoTuning(bool b) {
  MPWideAutoTune = b;
}

bool MPW_AutoTuning() {
  return MPWideAutoTune;
}

static void showSettings()
{
  LOG_INFO("-----------------------------------------------------------");
  LOG_INFO("MPWide Settings:");
  LOG_INFO("Chunk Size   (send/recv): " << tcpbuf_ssize << "/" << tcpbuf_rsize);
  LOG_INFO("Relay pace   (send/recv): " << relay_ssize << "/" << relay_rsize);
  LOG_INFO("Number of streams       : " << num_streams);
  LOG_INFO("tcp buffer parameter    : " << WINSIZE);
  LOG_INFO("pacing rate             : " << pacing_rate << " bytes/s.");
#ifdef MONITORING
  LOG_INFO("bandwidth monitoring    : " << MONITORING);
#else
  LOG_INFO("bandwidth monitoring    : 0")
#endif
  LOG_INFO("-----------------------------------------------------------");
  LOG_INFO("END OF SETUP PHASE.");
}

#ifdef PERF_TIMING
  double GetTime(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + 1.0e-6*tv.tv_usec;
  }

  double BarrierTime   = 0.0; //4 newly introduced globals for monitoring purposes
  double SendRecvTime  = 0.0;
  double PackingTime   = 0.0;
  double UnpackingTime = 0.0;
#endif

void MPW_setChunkSize(int sending, int receiving) {
  tcpbuf_ssize = sending;
  tcpbuf_rsize = receiving;
  LOG_DEBUG("Chunk Size  modified to: " << sending << "/" << receiving << ".");
}

/* MPW_DNSResolve converts a host name to an ip address. */
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
    host = (char*) (inet_ntoa(*address));
    LOG_DEBUG(" address found: " << host);
    return host;
  }
  LOG_ERR("Error: Unable to resolve host name");
  return NULL;
}

char *MPW_DNSResolve(std::string host) {
  return MPW_DNSResolve((char *)host.c_str());
}

inline int selectSockets(int wchannel, int rchannel, int mask)
/* Returns:
 0 if no access.
 1 if read on read channel.
 2 if write on write channel.
 3 if both. */
{
    return Socket_select(client[rchannel]->getSock(), client[wchannel]->getSock(), mask, 10, 0);
}

int MPW_NumChannels(){
  return num_streams;
}

#if MONITORING == 1
long long int bytes_sent;
bool stop_monitor = false;

#ifdef PERF_TIMING
/* Performs per-second throughput monitoring in real-time */
void *MPW_TBandwidth_Monitor(void *args)
{
  std::ofstream myfile;
  myfile.open("bandwidth_monitor.txt");
  long long int old_bytes_sent = 0;
  long long int cur_bytes_sent = 0;
  long long int old_time = 0;
  
  while(!stop_monitor) {
    if(old_time != int(GetTime())) {
      cur_bytes_sent = bytes_sent;
      myfile << "time: " << int(GetTime()) << " bandwidth: " << cur_bytes_sent - old_bytes_sent << std::endl;
      old_bytes_sent = cur_bytes_sent;
      old_time = int(GetTime());
    }
    usleep(1000);
  }
  myfile.close();
  return NULL;
}
#endif // ifdef PERF_TIMING
#endif // MONITORING == 1

/* Initialize a single MPWide TCP stream (used within a pthread). */
void* MPW_InitStream(void* args) 
{
  init_tmp &t = *((init_tmp *) args);

  Socket *sock = t.sock;
  const int stream = t.stream;
  const int port = t.port;
  const int cport = t.cport;
  const bool server_wait = t.server_wait;

  if(isclient[stream]) {
    sock->create();
    /* Patch to bypass firewall problems. */
    if(cport>0) {
      LOG_DEBUG("[" << stream << "] Trying to bind as client at " << (cport));
      int bound = sock->bind(cport);
    }

    /* End of patch*/
    t.connected = sock->connect(remote_url[stream],port);
    LOG_WARN("Server wait & connected " << server_wait << "," << t.connected);

    #if InitStreamTimeOut == 0
    if (!server_wait) {
      while(!t.connected) {
        usleep(50000);
        t.connected = client[stream].connect(remote_url[stream],port);
      }
    }
    #endif
    
    LOG_DEBUG("[" << stream << "] Attempt to connect as client to " << remote_url[stream]
              <<" at port " << port <<  ": " << t.connected);
  }
 
  if(!t.connected) {
    sock->close();
    if (server_wait) {
      sock->create();

      bool bound = sock->bind(port);
      LOG_DEBUG("[" << stream << "] Trying to bind as server at " << port << ". Result = " << bound);
      
      if (!bound) {
        LOG_WARN("Bind on ch #"<< stream <<" failed.");
        sock->close();
        return NULL;
      }

      if (sock->listen()) {
          t.connected = sock->accept();
          LOG_DEBUG("[" << stream << "] Attempt to act as server: " << t.connected);
          if (t.connected) { isclient[stream] = 0; }
      }
      else {
        LOG_WARN("Listen on ch #"<< stream <<" failed.");
        sock->close();
        return NULL;
      }
    }
  }
  return NULL;
}

/* Close down individual streams. */
void MPW_CloseChannels(int* channel, int numchannels) 
{
  for(int i = 0; i < numchannels; i++) {
    LOG_INFO("Closing channel #" << channel[i] << " with port = " << port[i] << " and cport = " << cport[i]);
    client[channel[i]]->close();
  }
}

/* Add new MPWide streams that are associated to a single MPWide Path. */
void MPW_AddStreams(std::string* url, int* ports, int* cports, const int *stream_indices, const int numstreams) {
  if (client == NULL) {
    client     = new Socket*[MAX_NUM_STREAMS];
    port       = new int[MAX_NUM_STREAMS];
    cport      = new int[MAX_NUM_STREAMS];
    isclient   = new int[MAX_NUM_STREAMS];
    remote_url = new std::string[MAX_NUM_STREAMS];
    ta         = new thread_tmp*[MAX_NUM_STREAMS];
    paths      = new MPWPath*[MAX_NUM_PATHS];
#ifdef PERF_TIMING
#if MONITORING == 1
    pthread_t monitor;
    int code = pthread_create(&monitor, NULL, MPW_TBandwidth_Monitor, NULL);
#endif
#endif
  }
  
  for(int i = 0; i < numstreams; i++) {
    const int stream = stream_indices[i];

    client[stream]     = new Socket();
    port[stream]       = ports[i];
    ta[stream]         = new thread_tmp;
    ta[stream]->channel = stream;
	  LOG_INFO("Stream number " << stream);
    remote_url[stream] = MPW_DNSResolve(url[i]);
    LOG_DEBUG("MPW_DNSResolve resolves " << url[i] << " to address " << remote_url[stream] << ".");
    
    if(url[i].compare("0") == 0 || url[i].compare("0.0.0.0") == 0) {
      isclient[stream] = 0;
      cport[stream]    = -1;
      LOG_INFO("Empty IP address given: Switching to Server-only mode.");
    } else {
      isclient[stream] = 1;
      cport[stream]    = cports[i];
    }

    LOG_DEBUG(url[i] << " " << ports[i] << " " << cports[i]);
  }
}

int MPW_InitStreams(int *stream_indices, int numstreams, bool server_wait) {
  pthread_t streams[numstreams];
  init_tmp t[numstreams];

  for(int i = 0; i < numstreams; i++) {
    int stream = stream_indices[i];
    t[i].stream      = stream;
    t[i].sock        = client[stream];
    t[i].port        = port[stream];
    t[i].cport       = cport[stream];
    t[i].server_wait = server_wait;
    t[i].connected   = false;
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
  
  /* Error handling code (in case MPW_InitStream times out for one or more */
  // closing itself is done by caller
  bool all_connected = true;
  for(int i = 0; i < numstreams; i++) {
    if(t[i].connected == false) {
      LOG_WARN("One connection has failed: #" << stream_indices[i]);
      all_connected = false;
    }
  }
  if (all_connected)
    return 0;
  else
    return -1;
}

static int reserveAvailableStreamNumber(int total_streams)
{
  int streak = 0;
  
  /* Get next available contiguous streams */
  for (int i = 0; i < num_streams; i++) {
    if (client[i] == NULL) {
      if (++streak == total_streams)
        // Don't modify num_streams
        return i + 1 - total_streams;
    } else if (streak) {
      streak = 0;
    }
  }
  // fall through
  if (num_streams + total_streams <= MAX_NUM_STREAMS) {
    const int stream_number = num_streams;
    num_streams += total_streams;
    return stream_number;
  } else {
    LOG_ERR("ERROR: trying to create more than " << MAX_NUM_STREAMS << " streams");
    return -1;
  }
}

static int reserveAvailablePathNumber()
{
  /* Get next available path */
  for (int i = 0; i < num_paths; ++i) {
    if (paths[i] == NULL)
      return i;
  }
  // fall through
  if (num_paths < MAX_NUM_PATHS) {
    return num_paths++;
  } else {
    LOG_ERR("ERROR: trying to create more than " << MAX_NUM_PATHS << " paths");
    return -1;
  }
}

/* Initialize the MPWide. set client to 1 for one machine, and to 0 for the other. */
int MPW_Init(std::string* url, int* ports, int* cports, int numstreams)
{
  LOG_INFO("Initialising...");

  const int start_stream = reserveAvailableStreamNumber(numstreams);
  if (start_stream == -1) return -1;
  
  int stream_indices[numstreams];
  for(int i = 0; i < numstreams; i++) {
    stream_indices[i] = start_stream + i; //if this is the first MPW_Init, then num_streams still equals 0 here.
  }

  MPW_AddStreams(url, ports, cports, stream_indices, numstreams);
  int ret = MPW_InitStreams(stream_indices, numstreams, true);
  showSettings();
  return ret;
}

/* Constructs a path. Return path id or negative error value. */
int MPW_CreatePathWithoutConnect(std::string host, int server_side_base_port, const int streams_in_path) {
  const int start_stream = reserveAvailableStreamNumber(streams_in_path);
  const int path_id = reserveAvailablePathNumber();
  
  if (start_stream == -1 || path_id == -1) return -1;

  int path_ports[streams_in_path];
  int path_cports[streams_in_path];

  std::string *hosts = new std::string[streams_in_path];
  int stream_indices[streams_in_path];
  
  for(int i = 0; i < streams_in_path; i++) {
    path_ports[i] = server_side_base_port + i;
    path_cports[i] = -2;
    hosts[i] = host;
    stream_indices[i] = start_stream + i;
  }
  
  MPW_AddStreams(hosts, path_ports, path_cports, stream_indices, streams_in_path);
  delete [] hosts;

  paths[path_id] = new MPWPath(host, stream_indices, streams_in_path);
  
#if MPW_PacingMode == 1
  if(MPWideAutoTune) {
    autotunePacingRate();
  }
#endif
  
  LOG_INFO("Creating New Path:");
  LOG_INFO(host << " " <<  server_side_base_port << " " << streams_in_path << " streams.");
  
  for(int i=0; i<streams_in_path; i++) {
    LOG_DEBUG("Stream[" << i << "]: " << paths[path_id]->streams[i]);
  } 

  /* Return the identifier for the MPWPath we just created. */
  return path_id;
}

/** Connect a path that has been created and provided with streams, to a remote endpoint,
 * or have it act as a server. 
 */
int MPW_ConnectPath(int path_id, bool server_wait) {
  int ret = MPW_InitStreams(paths[path_id]->streams, paths[path_id]->num_streams, server_wait);
  
  if (MPWideAutoTune && ret >= 0)
  {
    const int default_window = 32*1024*1024/paths[path_id]->num_streams;
    for(int j = 0; j < paths[path_id]->num_streams; j++)
      MPW_setWin(paths[path_id]->streams[j], default_window);
  }
  showSettings();

  return ret;
}

/* Creates and connects a path */
int MPW_CreatePath(std::string host, int server_side_base_port, int streams_in_path) {

  int path_id = MPW_CreatePathWithoutConnect(host, server_side_base_port, streams_in_path);
  int status = MPW_ConnectPath(path_id, true);
  if(status < 0) { 
    MPW_DestroyPath(path_id);
    return -1;
  }
  return path_id;
}

/* Remove a stream from a path. */
void EraseStream(int stream) {
  delete client[stream];
  client[stream] = NULL;
  delete ta[stream];
  
  // Deleted last stream, move the stream counter back
  if (stream + 1 == num_streams) {
    for (int i = stream - 1; i >= 0; --i) {
      if (client[i] != NULL) {
        num_streams = i + 1;
        return;
      }
    }
    // If no streams are found, num_streams is 0
    num_streams = 0;
  }
}

/* Attempt to change the TCP window size for a single stream. */
void MPW_setWin(int stream, int size) {
  client[stream]->setWin(size);
}

/* Attempt to change the TCP window size for a single path. */
void MPW_setPathWin(int path, int size) {
  for(int i=0; i < paths[path]->num_streams; i++) {
    client[paths[path]->streams[i]]->setWin(size);
  }
}

/** Destroy an MPWide path (disconnect, then delete).
 * Return 0 on success (negative on failure).
 */
int MPW_DestroyPath(int path) {
  for (int j = 0; j < paths[path]->num_streams; j++) {
    EraseStream(paths[path]->streams[j]);
  }

  delete paths[path];
  paths[path] = NULL;

  // Reset num_paths, if this was the last path
  if (path == num_paths - 1) {
    for (int i = path - 1; i >= 0; --i) {
      if (paths[i] != NULL) {
        num_paths = i + 1;
        return 0;
      }
    }
    // If no paths are found, num_streams is 0
    num_paths = 0;
  }
  
  return 0;
}

/* Path-based Send and Recv operations*/
int MPW_DSendRecv(char* sendbuf, long long int sendsize, char* recvbuf, long long int maxrecvsize, int path) {
  return MPW_DSendRecv(sendbuf, sendsize, recvbuf, maxrecvsize, paths[path]->streams, paths[path]->num_streams);
}

int MPW_SendRecv(char* sendbuf, long long int sendsize, char* recvbuf, long long int recvsize, int path) {
  return MPW_SendRecv(sendbuf, sendsize, recvbuf, recvsize,  paths[path]->streams, paths[path]->num_streams);
}

int MPW_Send(char* sendbuf, long long int sendsize, int path) {
  return MPW_SendRecv(sendbuf, sendsize, NULL, 0, paths[path]->streams, paths[path]->num_streams);
}

int MPW_Recv(char* recvbuf, long long int recvsize, int path) {
  return MPW_SendRecv(NULL, 0, recvbuf, recvsize,  paths[path]->streams, paths[path]->num_streams);
}


/* Legacy initialization function for MPWide that does not require client port binding. */
int MPW_Init(std::string* url, int* ports, int numstreams) 
{
  int cports[numstreams];

  for(int i=0; i<numstreams; i++) {
    cports[i] = -1;
  }
  return MPW_Init(url, ports, cports, numstreams);
}

/* Legacy shorthand initialization call for local processes that use a single stream. */
int MPW_Init(std::string url, int port) {
  std::string u1[1] = {url};
  int    p1[1] = {port};
  return MPW_Init(u1,p1,1);
}

/* AS the last two functions, but then compatible for C. */
extern "C" {
  int MPW_Init_c (char** url, int* ports, int numstreams) 
  {
    std::string* urls = new std::string[numstreams];
    for(int i=0;i<numstreams;i++) {
      urls[i].assign(url[i]);
    }
    int status = MPW_Init(urls,ports,numstreams);
    delete [] urls;
    return status;
  }

  int MPW_Init1_c (char* url, int port)
  {
    return MPW_Init(url, port);
  }
}

/* Close all sockets and free data structures related to the library. */
int MPW_Finalize()
{
#if MONITORING == 1
  stop_monitor = true;
#endif
  for (int i = 0; i < num_paths; i++) {
    if (paths[i])
      delete paths[i];
  }
  delete [] paths;
  num_paths = 0;
  
  for (int i = 0; i < num_streams; i++) {
    if (client[i]) {
      delete client[i];
      delete ta[i];
    }
  }
  delete [] client;
  delete [] ta;
  delete [] port;
  delete [] cport;
  delete [] remote_url;
  delete [] isclient;
  num_streams = 0;

  LOG_INFO("MPWide sockets are closed.");
  // Wait on any closing sockets
  sleep(1);
  return 1;
}



/* Wrapping function for SendRecv in case no receiving is required. */
void MPW_Send(char* sendbuf, long long int size, int* channels, int num_channels)
{
  MPW_SendRecv(sendbuf,size,NULL,0,channels,num_channels);
}

/* Wrapping function for SendRecv in case no sending is required. */
void MPW_Recv(char* buf, long long int size, int* channels, int num_channels)
{
  MPW_SendRecv(NULL,0,buf,size,channels,num_channels);
}

/* Send/Recv (part of) the data between two processes using a single TCP stream and a single thread. */
int *InThreadSendRecv(char* const sendbuf, const long long int sendsize, char* const recvbuf, const long long int recvsize, const int base_channel)
{
  int * const ret = new int(0);

#ifdef PERF_TIMING
  double t = GetTime();
#endif

  long long int a = 0;
  long long int b = 0;
  
  const Socket *wsock = client[base_channel % 65536];
  const Socket *rsock = base_channel < 65536 ? wsock : client[(base_channel/65536) - 1];
  
  int mask = (recvsize == 0 ? MPWIDE_SOCKET_RDMASK : 0)
           | (sendsize == 0 ? MPWIDE_SOCKET_WRMASK : 0);
  
  while (mask != (MPWIDE_SOCKET_RDMASK|MPWIDE_SOCKET_WRMASK)) {
    const int mode = Socket_select(rsock->getSock(), wsock->getSock(), mask, 10, 0);

    if (mode == -1) {
      // Continue after interrupt, but fail on other messages
      if (errno == EINTR)
        continue;
      else {
        *ret = -max(1, errno);
        break;
      }
    }
    if(FLAG_CHECK(mode,MPWIDE_SOCKET_RDMASK)) {
      const int n = rsock->irecv(recvbuf + b, min(tcpbuf_rsize,recvsize - b));
      if (n <= 0) {
        if (n == 0) // socket disconnected on other side, choose default -1 errno.
          *ret = -1;
        else
          *ret = -max(1, errno);
        break;
      }
      b += n;
      #if MONITORING == 1
      bytes_sent += n;
      #endif

      if(b == recvsize)
        mask |= MPWIDE_SOCKET_RDMASK; //don't check for read anymore
    }

    if(FLAG_CHECK(mode,MPWIDE_SOCKET_WRMASK)) {
      const int n = wsock->isend(sendbuf + a, min(tcpbuf_ssize, sendsize - a));

      if (n < 0) {
        *ret = -errno;
        break;
      }
      
      a += n;
      #if MONITORING == 1
      bytes_sent += n;
      #endif

      if(a == sendsize)
        mask |= MPWIDE_SOCKET_WRMASK; //don't check for write anymore
    }

    #if MPW_PacingMode == 1
    usleep(pacing_sleeptime);
    #endif
  }

  #ifdef PERF_TIMING
  t = GetTime() - t;
  LOG_DEBUG("This Send/Recv took: " << t << "s. Rate: " << (sendsize+recvsize)/(t*1024*1024)
            << "MB/s. ch=" << (base_channel % 65536) << "/" << ((base_channel/65536) - 1));
  SendRecvTime += t;
  #endif
  return ret;
}

/* Data type intended to contain information for MPW_Relay (which relies on threads) */
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
  
  LOG_DEBUG("Starting Relay Channel #" << channel);
  int tmp = 0;

  while(1) {

    mode  = client[channel]->select_me(0);
    mode2 = client[channel2]->select_me(0);
    //std::cout << "mode/mode2 = " << mode << "/" << mode2 << "--" << n << "/" << n2 << "/" << ns << "/" << ns2 <<std::endl;

    /* Recv from channel 1 */
    if(mode%2 == 1) {
      if(n == ns) {
        n = 0; ns = 0;
      }  
      tmp = client[channel]->irecv(buf+n,min(bufsize-n,relay_rsize));
      n += tmp; 
      LOG_TRACE("Retrieved from 1: " << n);
    }
    /* ...forward to channel 2. */
    if(mode2/2 == 1 && n > 0) {
      tmp = client[channel2]->isend(buf+ns,min(n-ns,relay_ssize));
      ns += tmp;
      LOG_TRACE("Sent to 2: " << ns);
    }
    
    // Recv from channel 2 
    if(mode2%2 == 1) {  
      if(n2 == ns2) {
        n2 = 0; ns2 = 0;
      }
      tmp = client[channel2]->irecv(buf2+n2,min(bufsize-n2,relay_rsize));
      n2 += tmp;
      LOG_TRACE("Retrieved from 2: " << n2);
    }
    // ...forward to channel 1. 
    if(mode/2 == 1 && n2 > 0) {
      tmp = client[channel]->isend(buf2+ns2,min(n2-ns2,relay_ssize));
      ns2 += tmp;
      LOG_TRACE("Sent to 1: " << ns2);
    }
    
    #if MPW_PacingMode == 1
    usleep(pacing_sleeptime);
    #endif
  }
  
  free(buf);
  free(buf2);

  return NULL;
}

/* Unused function that can allow automatic termination when a file named 'stop' is present. */
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

  int channel2 = cycling ? (ta->channel / 65536) - 1 : channel; //recv channel

  int id = ta->thread_id;
  long long int numschannels = ta->numchannels;
  long long int numrchannels = ta->numrchannels;

  long long int sendsize = totalsendsize / numschannels;
  if(id < (totalsendsize % numschannels)) {
    sendsize++;
  }

  #if SendRecvInputReport == 2
  std::cout << "TDynEx(sendsize="<<sendsize<<",maxrecvsize="<<maxrecvsize<<",ch_send="<<channel<<",ch_recv="<<channel2<<",nc="<<numschannels<<"/"<<numrchannels<<std::endl;
  #endif

  unsigned char net_size_found[8], net_send_size[8];
  long long int recvsizeall = -1;
  size_t a, b;
  long long int c,d;
  a = b = 0;
  c = d = 0;

  // recvsize is initially set to the size of the long long int which holds the message size.
  bool recv_settings_known = false;   //this thread knows how much data may be received.
  int mask = 0;
  long long int offset_r = 0; //stores correct recv buffer offset for this thread.

  /* Second: await the recvsize */

  while(recvsize > d || sendsize > c) {
    int mode = selectSockets(channel,channel2,mask);

    /* (1.) Receiving is possible, but only done by thread 0 until we know more. */
    if(mode%2 == 1) {
      if(!recv_settings_known) {
        int n = client[channel2]->irecv((char *)net_size_found+b,8-b);
        b += n;

        if(b == 8) { //recvsize data is now available.
          size_t size_found = ::deserialize_size_t(net_size_found);

          if (size_found > maxrecvsize) { //Would we want to do reallocs here???
            std::cerr << "ERROR: DynEx recv size is greater than given constraint." << std::endl;
            std::cerr << "(Size found = " << size_found << " bytes. Maxrecvsize = " << maxrecvsize << " bytes.)" << std::endl;
            std::cerr << "(Channel: " << channel <<", Totalsendsize = "<< totalsendsize <<")" << std::endl;
            std::cerr << "Going to sleep, so trace can be done. Press Ctrl-c to exit." << std::endl;
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
        int n = client[channel2]->irecv(recvbuf+d,min(tcpbuf_rsize,recvsize-d));
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
        ::serialize_size_t(net_send_size, (const size_t)totalsendsize);
        int n = client[channel]->isend((char*)net_send_size + a,8-a);
        a += n;
      }
      else { //send data after that, leave 16byte margin to prevent SendRecv from crashing.
        int n = client[channel]->isend(sendbuf+c,min(tcpbuf_ssize,sendsize-c)); 
        c += n;
        #if MONITORING == 1
        bytes_sent += n;
        #endif

        if(sendsize == c) {
          mask += 2; //don't check for write anymore
        }
      }
    }
    #if MPW_PacingMode == 1
    usleep(pacing_sleeptime);
    #endif
  }

  return NULL;
}

/* Better version of TSendRecv */
void *MPW_TSendRecv(void *args)
{
  thread_tmp *t = (thread_tmp *)args;
  return InThreadSendRecv(t->sendbuf, t->sendsize, t->recvbuf, t->recvsize, t->channel);
}

/* Better version of TSendRecv */
void *MPW_TSend(void *args)
{
  thread_tmp *t = (thread_tmp *)args;
  return InThreadSendRecv(t->sendbuf, t->sendsize, (char *)0, 0, t->channel);
}

/* Better version of TSendRecv */
void *MPW_TRecv(void *args)
{
  thread_tmp *t = (thread_tmp *)args;
  return InThreadSendRecv((char *)0, 0, t->recvbuf, t->recvsize, t->channel);
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
  //std::cout << sendbuf[0] << " / " << recvbuf[0] << " / " << num_channels << " / " << sendsize[0] << " / " << recvsize[0] << " / " << channel[0] << std::endl;

  pthread_t streams[num_channels];
  long long int dyn_recvsize = 0;

  for(int i=0; i<num_channels; i++){
      ta[channel[i]]->sendsize = totalsendsize;
      ta[channel[i]]->recvsize = maxrecvsize;
    
      // NOTE: after this function ends, this will be a dangling pointer
      ta[channel[i]]->dyn_recvsize = &dyn_recvsize; //one recvsize stored centrally. Read in by thread 0.
    
      ta[channel[i]]->channel = channel[i];
      ta[channel[i]]->sendbuf = sendbuf[i];
      ta[channel[i]]->recvbuf = recvbuf;
      ta[channel[i]]->thread_id = i;
      ta[channel[i]]->numchannels = num_channels;
      ta[channel[i]]->numrchannels = num_channels;
      if(i>0) {
        int code = pthread_create(&streams[i], NULL, MPW_TDynEx, ta[channel[i]]);
      }
  }

  MPW_TDynEx(ta[channel[0]]);

  for(int i=1; i<num_channels; i++) {
    pthread_join(streams[i], NULL);
  }

#ifdef PERF_TIMING
  t = GetTime() - t;
  
  #if LOG_LVL >= LOG_INFO
  long long int total_size = totalsendsize + dyn_recvsize;

  std::cout << "DSendRecv: " << t << "s. Size: " << (total_size/(1024*1024)) << "MB. Rate: " << total_size/(t*1024*1024) << "MB/s." << std::endl;
  #endif
  
  SendRecvTime += t;
#endif

  return ta[channel[0]]->dyn_recvsize[0];

}

/* buf,bsize and num_chunks contain parameters.
 * split_buf and chunksizes are placeholders for the splitted buffer and its properties. */
//TODO: Introduce a split in 4096-byte chunks using an ifdef mechanism?
void MPW_splitBuf(char* buf, long long int bsize, int num_chunks, char** split_buf, long long int* chunk_sizes) {

  if(num_chunks < 1) { 
    LOG_ERR("ERROR: MPW_splitBuf is about to split into 0 chunks.");
    exit(0);
  }
  size_t bsize_each = size_t(bsize / num_chunks);
  int bsize_each_odd = bsize % num_chunks;
  size_t offset = 0;
  size_t ii = 0;

  for(int i=0; i<num_chunks; i++) {
    ii = bsize_each;
    if(i < bsize_each_odd)  ii ++;
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

/* The core function implementing MPW_Cycle. */
long long int Cycle(char** sendbuf2, long long int sendsize2, char* recvbuf2, long long int maxrecvsize2, int* ch_send, int nc_send, int* ch_recv, int nc_recv, bool dynamic) {
  #ifdef PERF_TIMING
  double t = GetTime();
  #endif
  pthread_t streams[nc_send];
  char dummy_recv[nc_recv];
  char dummy_send[nc_send][1];

  long long int totalsendsize = sendsize2;
  long long int dyn_recvsize_sendchannel = 0; 
  long long int recv_offset = 0; //only if !dynamic

  //TODO: Add support for different number of send/recv streams.
  for (int i = 0; i < max(nc_send,nc_recv); i++)
  {
    thread_tmp &props = *ta[i];
    
    if(totalsendsize>0 && i<nc_send) {
      if(dynamic) { //overall sendsize given to all threads.
        props.sendsize = totalsendsize;
      } else { //1 sendsize separately for each thread.
        props.sendsize = totalsendsize / nc_send;
        if(i < (totalsendsize % nc_send)) {
          props.sendsize++;
        }
      }
      props.sendbuf = sendbuf2[i];
    }
    else {
      props.sendsize = 1*nc_send;
      props.sendbuf = dummy_send[i];
    }

    if(maxrecvsize2>0 && i<nc_recv) {
      if(dynamic) { //one recvbuf and size limit for all threads.
        props.recvsize = maxrecvsize2;
        props.recvbuf = recvbuf2;
      } else { //assign separate and fixed-size recv bufs for each thread.
        props.recvbuf = &(recvbuf2[recv_offset]);
        props.recvsize = maxrecvsize2 / nc_recv;
        if(i<(maxrecvsize2 % nc_recv)) { props.recvsize++; }
        recv_offset += props.recvsize;
      }
    }
    else {
      props.recvsize = 1*nc_recv;
      props.recvbuf = dummy_recv;
    }

    props.dyn_recvsize = &dyn_recvsize_sendchannel; //one recvsize stored centrally. Read in by thread 0.
    props.channel = 0;
    if(i<nc_send) {
      props.channel = ch_send[i]; 
    }
    if(i<nc_recv) {
      if(i<nc_send) {
        props.channel += ((ch_recv[i]+1)*65536);
      }
      else {
        props.channel = ch_recv[i];
      }
    }

    props.thread_id = i;
    props.numchannels  = nc_send;
    props.numrchannels = nc_recv;
    //printThreadTmp(ta[i]);
    if(i>0) {
      if(dynamic) {
        int code = pthread_create(&streams[i], NULL, MPW_TDynEx, ta[i]);
      } else {
        int code = pthread_create(&streams[i], NULL, MPW_TSendRecv, ta[i]);
      }
    }
  }

  if(dynamic) {
    MPW_TDynEx(ta[0]);
    if(max(nc_send,nc_recv)>1) {
      for(int i=1; i<max(nc_send,nc_recv); i++) {
        pthread_join(streams[i], NULL);
      }
    }
  } else {
    int* res = (int *)MPW_TSendRecv(ta[0]);
    // TODO: error checking on MPW_TSendRecv
    delete res;

    if(max(nc_send,nc_recv)>1) {
      for(int i=1; i<max(nc_send,nc_recv); i++) {
        pthread_join(streams[i], (void **)&res);
        // TODO: error checking on MPW_TSendRecv
        delete res;
      }
    }
  }


  #ifdef PERF_TIMING
    t = GetTime() - t;

    #if LOG_LVL >= LOG_INFO
      long long int total_size = sendsize2 + (ta[0]->dyn_recvsize)[0];
      std::cout << "Cycle: " << t << "s. Size: " << (total_size/(1024*1024)) << "MB. Rate: " << total_size/(t*1024*1024) << "MB/s." << std::endl;
    #endif
    SendRecvTime += t;
  #endif

//  return dyn_recvsize_recvchannel;
  return (ta[0]->dyn_recvsize)[0];
}

/** CycleWrapper
 * Recv from one set of channels. Send through to another set of channels. 
 * MPW_Cycle may become obsolete in time, if we are able to obtain the 
 * same performance with non-blocking transfer calls.
 * This is an internal function used by MPW_Cycle and MPW_DCycle.
 */
long long int CycleWrapper(char* sendbuf, long long int sendsize, char* recvbuf, long long int maxrecvsize,
             int* ch_send, int num_ch_send, int* ch_recv, int num_ch_recv, bool dynamic) 
{
  #if SendRecvInputReport == 1
  if(dynamic) {
    std::cout << "MPW_DCycle(sendsize="<<sendsize<<",maxrecvsize="<<maxrecvsize<<",ncsend="<<num_ch_send<<",ncrecv="<<num_ch_recv<<");"<<std::endl;
  } else {
    std::cout << "MPW_Cycle(sendsize="<<sendsize<<",recvsize="<<maxrecvsize<<",ncsend="<<num_ch_send<<",ncrecv="<<num_ch_recv<<");"<<std::endl;
  }
  for(int i=0; i<num_ch_send; i++) {
    std::cout << "send channel " << i << ": " << ch_send[i] << std::endl;
  }
  for(int i=0; i<num_ch_recv; i++) {
    std::cout << "recv channel " << i << ": " << ch_recv[i] << std::endl;
  }
  #endif

  if(sendsize<1 && maxrecvsize<1) {
    if(sendsize == 0 && maxrecvsize == 0) {
//      std::cout << "MPW_Cycle: called with empty send/recv buffers. Skipping transfer.\n" << std::endl;
    }
    else {
      std::cout << "MPW_Cycle error: sendsize = " << sendsize << ", maxrecvsize = " << maxrecvsize << std::endl;
      exit(-1);
    }
  }
//  std::cout << "MPW_Cycle: " << sendsize << "/" << maxrecvsize << "/" << ch_send[0] << "/" << num_ch_send << "/" << ch_recv[0] << "/" << num_ch_recv << std::endl;
  char **sendbuf2 = new char*[num_ch_send];
  long long int *sendsize2    = new long long int[num_ch_send]; //unused by Cycle.

  MPW_splitBuf( sendbuf, sendsize, num_ch_send, sendbuf2, sendsize2);

  long long int total_recv_size = Cycle( sendbuf2, sendsize, recvbuf, maxrecvsize, ch_send, num_ch_send, ch_recv, num_ch_recv, dynamic);

  delete [] sendbuf2;
  delete [] sendsize2;
  return total_recv_size;
}

/**
 * Dynamically-sized MPW_Cycle. Note, this may give problems when there are many messages of size 0, so it is good to avoid these.
 */
long long int MPW_DCycle(char* sendbuf, long long int sendsize, char* recvbuf, long long int maxrecvsize,
             int* ch_send, int num_ch_send, int* ch_recv, int num_ch_recv)
{
  return CycleWrapper(sendbuf, sendsize, recvbuf, maxrecvsize, ch_send, num_ch_send, ch_recv, num_ch_recv, true);
}

/**
 * Regular MPW_Cycle. Note, this may give problems when there are many messages of size 0, so it is good to avoid these.
 */
void MPW_Cycle(char* sendbuf, long long int sendsize, char* recvbuf, long long int maxrecvsize,
             int* ch_send, int num_ch_send, int* ch_recv, int num_ch_recv)
{
  CycleWrapper(sendbuf, sendsize, recvbuf, maxrecvsize, ch_send, num_ch_send, ch_recv, num_ch_recv, false);
}

/** MPW_PSendRecv
 * This function relies of buffers that have been split in advance. Unless there is a strong use case emerging, we will
 * render it obsolete in a future major release of MPWide.
 */
int MPW_PSendRecv(char** sendbuf, long long int* sendsize, char** recvbuf, long long int* recvsize, int* channel, int num_channels)
{
#ifdef PERF_TIMING
  double t = GetTime();
#endif

  //std::cout << sendbuf[0] << " / " << recvbuf[0] << " / " << num_channels << " / " << sendsize[0] << " / " << recvsize[0] << " / " << channel[0] << std::endl;
  pthread_t streams[num_channels];

  void *(*sendrecvFunc)(void *);
  
  for(int i = 0; i < num_channels; i++){
    const int stream = channel[i];

    if (recvsize[i]) {
      ta[stream]->recvbuf = recvbuf[i];
      ta[stream]->recvsize = recvsize[i];
    }
    if (sendsize[i]) {
      ta[stream]->sendsize = sendsize[i];
      ta[stream]->sendbuf = sendbuf[i];
    }
    
    if (sendsize[i] && recvsize[i])
      sendrecvFunc = &MPW_TSendRecv;
    else if (sendsize[i])
      sendrecvFunc = &MPW_TSend;
    else
      sendrecvFunc = &MPW_TRecv;
    
    if(i>0) {
      int code = pthread_create(&streams[i], NULL, sendrecvFunc, ta[stream]);
    }
  }
  
  int return_value = 0;
  int *res = (int *)sendrecvFunc(ta[channel[0]]);
  if (*res < 0)
    return_value = *res;
  delete res;

  if(num_channels > 1) {
    for(int i = 1; i < num_channels; i++) {
      pthread_join(streams[i], (void **)&res);
      if (*res < 0)
        return_value = *res;
      delete res;
    }
  }

  #ifdef PERF_TIMING
    t = GetTime() - t;

    #if LOG_LVL >= LOG_INFO
      long long int total_size = 0;
      for(int i=0;i<num_channels;i++) {
        total_size += sendsize[i]+recvsize[i];
      }
      std::cout << "PSendRecv: " << t << "s. Size: " << (total_size/(1024*1024)) << "MB. Rate: " << total_size/(t*1024*1024) << "MB/s." << std::endl;
    #endif
    SendRecvTime += t;
  #endif
  return return_value;
}

/** MPW_SendRecv
 * The core function implementing MPW_SendRecv, which does two-way data passing between endpoints over an array of MPWide streams.
 */
int MPW_SendRecv( char* sendbuf, long long int sendsize, char* recvbuf, long long int recvsize, int* channel, int nc){

#if SendRecvInputReport == 1
  std::cout << "MPW_SendRecv(sendsize=" << sendsize << ",recvsize=" << recvsize << ",nc=" << nc << ");" << std::endl;
  for(int i=0; i<nc; i++) {
    std::cout << "channel " << i << ": " << channel[i] << std::endl;
  }
#endif

#if OptimizeStreamCount == 1
  nc = max(1, min(nc, max(sendsize, recvsize)/BytesPerStream) );
#endif

  size_t sendsize_each = size_t(sendsize / nc);
  size_t recvsize_each = size_t(recvsize / nc);

  int sendsize_each_odd = sendsize % nc;
  int recvsize_each_odd = recvsize % nc;

  char **sendbuf2 = new char*[nc];
  char **recvbuf2 = new char*[nc];
  long long int *sendsize2 = new long long int[nc];
  long long int *recvsize2 = new long long int[nc];
  
  size_t offset = 0;
  size_t offset2 = 0;
  size_t ii=0;
  size_t iii=0;
  
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

  int ret = MPW_PSendRecv( sendbuf2, sendsize2, recvbuf2, recvsize2, channel, nc);

  delete [] sendbuf2;
  delete [] recvbuf2;
  delete [] sendsize2;
  delete [] recvsize2;
  return ret;
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
    client[i]->send("Test 1!",8);  
    client[i]->recv(s,8);
  }
  else {
    client[i]->recv(s,8);
    client[i]->send(s,8);
  }
  #ifdef PERF_TIMING
    BarrierTime += GetTime() - t;
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
    //std::string urls(url)
    MPW_SendRecv(sendbuf, sendsize, recvbuf, recvsize, base_channel);
  }
  void MPW_SendRecv_c (char* sendbuf, long long int sendsize, char* recvbuf, long long int recvsize, int* base_channel, int num_channels) {
    //std::string urls(url)
    MPW_SendRecv(sendbuf, sendsize, recvbuf, recvsize, base_channel, num_channels);
  }
  void MPW_PSendRecv_c(char** sendbuf, long long int* sendsize, char** recvbuf, long long int* recvsize, int* channel, int num_channels) {
    MPW_PSendRecv(sendbuf, sendsize, recvbuf, recvsize, channel, num_channels);
  }
}

/* Diagnostics */
void printThreadTmp (thread_tmp t) { //print thread specific information.
  std::cout << "Thread #" << t.thread_id << " of " << t.numchannels << " send channels and " << t.numrchannels << " recv channels." << std::endl;
  std::cout << "Sendsize: " << t.sendsize << ", Recvsize: " << t.recvsize << std::endl;
  std::cout << "DynRecvsize: " << t.dyn_recvsize[0] << ", channel: " << t.channel%65536 << "," << (t.channel/65536)-1 << std::endl;
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

static std::vector<MPW_NBE> MPW_nonBlockingExchanges;

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

/* Non-Blocking Exchange style MPWide receive. */
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
    std::cout << "WARNING: you used a non existent NonBlockingExchange ID number in MPW_Wait." << std::endl;
  }
  return element_number;
}

/* Check if a particular non-blocking exchange has completed. */
bool MPW_Has_NBE_Finished(int NBE_id) {
  int element_number = Find_NBE_By_ID(NBE_id);
  return(MPW_nonBlockingExchanges[element_number].NBE_args.channel >= 0);
}

/* Wait until a particular non-blocking exchange has completed. */
void MPW_Wait(int NBE_id) {
  int element_number = Find_NBE_By_ID(NBE_id);  
  pthread_join(MPW_nonBlockingExchanges[element_number].pthr_id, NULL);
  MPW_nonBlockingExchanges.erase(MPW_nonBlockingExchanges.begin()+element_number);
}
