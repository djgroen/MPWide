# distutils: language = c++
# distutils: sources = ../MPWide.cpp ../Socket.cpp

cimport MPWide

class MPWide_python:

#char* MPW_DNSResolve(char* host);
  def MPW_DNSResolve(self, host):
    MPWide.MPW_DNSResolve(host)
#/* Enable/disable autotuning. Set before initialization. */
#void MPW_setAutoTuning(bool b);
  def MPW_setAutoTuning(self, b):
    MPWide.MPW_setAutoTuning(b)

#/* Print all connections. */
  def MPW_Print(self):
    MPWide.MPW_Print()

#/* Print the number of available streams. */
#def MPW_NumChannels():

# Return path id or negative error value. 
#int MPW_CreatePathWithoutConnect(string host, int server_side_base_port, int streams_in_path); 
#def MPW_CreatePathWithoutConnect(host, server_side_base_port, streams_in_path);

#int MPW_ConnectPath(int path_id, bool server_wait);
#def MPW_ConnectPath(path_id, server_wait):

#int MPW_CreatePath(string host, int server_side_base_port, int num_streams); 
  def MPW_CreatePath(self, host, server_side_base_port, num_streams):
    return MPWide.MPW_CreatePath(host, server_side_base_port, num_streams)

# Return 0 on success (negative on failure).
#int MPW_DestroyPath(int path);
  def MPW_DestroyPath(self, path):
    return MPWide.MPW_DestroyPath(path)

#int MPW_Send(char* sendbuf, long long int sendsize, int path);
#def MPW_Send(sendbuf, sendsize, path):

#int MPW_Recv(char* recvbuf, long long int recvsize, int path);
#def MPW_Recv(recvbuf, recvsize, path):

#int MPW_SendRecv(char* sendbuf, long long int sendsize, char* recvbuf, long long int recvsize, int path);
  def MPW_SendRecv(self, sendbuf, sendsize, recvbuf, recvsize, path):
    return MPWide.MPW_SendRecv(sendbuf, sendsize, recvbuf, recvsize, path)

# returns the size of the newly received data. 
#int MPW_DSendRecv(char* sendbuf, long long int sendsize, char* recvbuf, long long int maxrecvsize, int path);
  def MPW_DSendRecv(self, sendbuf, sendsize, recvbuf, maxrecvsize, path):
    return MPWide.MPW_DSendRecv(sendbuf, sendsize, recvbuf, maxrecvsize, path)

#int MPW_Init(string* url, int* server_side_ports, int* client_side_ports, int num_channels);
#def MPW_Init(url, server_side_ports, client_side_ports, num_channels):

#int MPW_Init(string* url, int* server_side_ports, int num_channels); //this call omits client-side port binding.
#def MPW_Init(url, server_side_ports, num_channels):

#int MPW_Init(string url, int port);
#def MPW_Init(url, port)


# Set tcp window size. 
#void MPW_setWin(int channel, int size);
#def MPW_setWin(channel, size):

#void MPW_setPathWin(int path, int size);
#def MPW_setPathWin(path, size):

# Close channels. 
#void MPW_CloseChannels(int* channels , int num_channels);
#def MPW_CloseChannels(channels, num_channels):

# Reopen them. 
#void MPW_ReOpenChannels(int* channels, int num_channels);
#def MPW_ReOpenChannels(channels, num_channels):

# Close all sockets and free data structures related to the library. 
#def MPW_Finalize():

#def MPW_Barrier(channel):

# Adjust the global feeding pace. 
#def MPW_setChunkSize(sending, receiving);

# Non-blocking functionalities. 
#def MPW_ISendRecv(sendbuf, sendsize, recvbuf, recvsize, path):

#def MPW_Has_NBE_Finished(NBE_id):

#def MPW_Wait(NBE_id):

# Get and set rates for pacing data. 
#def MPW_getPacingRate():
#def MPW_setPacingRate(double rate):

