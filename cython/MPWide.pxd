# file: MPWide.pxd
from libcpp.string cimport string
from libcpp cimport bool

cdef extern from "../MPWide.h":

  cdef char* MPW_DNSResolve(char* host)

#/* Enable/disable autotuning. Set before initialization. */
  cdef void MPW_setAutoTuning(bool b)

#/* Print all connections. */
  cdef void MPW_Print()

#/* Print the number of available streams. */
#  void MPW_NumChannels()

# Return path id or negative error value. 
#  int MPW_CreatePathWithoutConnect(string host, int server_side_base_port, int streams_in_path) 

#  int MPW_ConnectPath(int path_id, bool server_wait)

  cdef int MPW_CreatePath(string host, int server_side_base_port, int num_streams) 

# Return 0 on success (negative on failure).
  cdef int MPW_DestroyPath(int path)

#  int MPW_Send(char* sendbuf, long long int sendsize, int path)

#  int MPW_Recv(char* recvbuf, long long int recvsize, int path)

  cdef int MPW_SendRecv(char* sendbuf, long long int sendsize, char* recvbuf, long long int recvsize, int path)

# returns the size of the newly received data. 
  cdef int MPW_DSendRecv(char* sendbuf, long long int sendsize, char* recvbuf, long long int maxrecvsize, int path)

#  int MPW_Init(string* url, int* server_side_ports, int* client_side_ports, int num_channels)

#  cdef int MPW_Init(string* url, int* server_side_ports, int num_channels) #this call omits client-side port binding.

#  int MPW_Init(string url, int port)

# Set tcp window size. 
#  void MPW_setWin(int channel, int size)

#  void MPW_setPathWin(int path, int size)

# Close channels. 
#  void MPW_CloseChannels(int* channels , int num_channels)

# Reopen them. 
#  void MPW_ReOpenChannels(int* channels, int num_channels)

# Close all sockets and free data structures related to the library. 
#  void MPW_Finalize()

#  void MPW_Barrier(channel)

# Adjust the global feeding pace. 
#  void MPW_setChunkSize(sending, receiving)

# Non-blocking functionalities. 
#  void MPW_ISendRecv(sendbuf, sendsize, recvbuf, recvsize, path)

#  void MPW_Has_NBE_Finished(NBE_id)

#  void MPW_Wait(NBE_id)

# Get and set rates for pacing data. 
#  void MPW_getPacingRate()

