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
#include <cstdlib>
#include <unistd.h>
#include <cmath>
#include <iostream>
#include <pthread.h>
#include <stack>
#include <cstring>
#include <errno.h>

using namespace std;

#include "MPWide.h"

pthread_mutex_t log_mutex;
pthread_mutex_t accept_destroy_mutex;
pthread_rwlock_t use_rwmutex;
stack<int> destroypaths;
pthread_mutex_t destroy_mutex;
pthread_cond_t destroy_cond;

#define LOG(X) { pthread_mutex_lock(&log_mutex); cout << X << endl; pthread_mutex_unlock(&log_mutex); }

struct vars
{
  int num_channels_or_path_id;
  size_t msg_size;
  bool do_send;
};

void add_destroypath(int path_id)
{
  if (path_id >= 0)
    LOG("Scheduled to destroy path " << path_id);
  
  pthread_mutex_lock(&destroy_mutex);
  destroypaths.push(path_id);
  pthread_cond_signal(&destroy_cond);
  pthread_mutex_unlock(&destroy_mutex);
}

int do_connect(string host, int port, int num_channels, bool asServer)
{
  pthread_rwlock_wrlock(&use_rwmutex);
  int path_id = MPW_CreatePathWithoutConnect(host, port, num_channels); ///path version
  pthread_rwlock_unlock(&use_rwmutex);

  if (path_id >= 0) {
    LOG("Connecting to path " << path_id << "; server: " << asServer);

    if (asServer)
      pthread_mutex_lock(&accept_destroy_mutex);
    else
      pthread_rwlock_rdlock(&use_rwmutex);
    
    if (MPW_ConnectPath(path_id, asServer) < 0) {
      add_destroypath(path_id);
      path_id = -1;
    }
    
    if (asServer)
      pthread_mutex_unlock(&accept_destroy_mutex);
    else
      pthread_rwlock_unlock(&use_rwmutex);
  }
  else LOG("Failed to connecting to path " << path_id << "; server: " << asServer);

  
  return path_id;
}

void *server_thread(void * data)
{
  int num_channels = ((vars *)data)->num_channels_or_path_id;
  int *path_id = new int;
  LOG("Server thread accepting connections");
  *path_id = do_connect("0", 16256, num_channels, true);

  return path_id;
}

void *connecting_thread(void *data)
{
  int num_channels = ((vars *)data)->num_channels_or_path_id;
  size_t msg_size = ((vars *)data)->msg_size;

  // Try to connect until you succeed
  int path_id = do_connect("localhost", 16256, num_channels, false);
  while (path_id < 0) {
    // Sleep 0.2 seconds
    usleep(200000);
    path_id = do_connect("localhost", 16256, num_channels, false);
  }

  usleep(200000);
  if (path_id >= 0) {
    LOG("Succesfully connected to server");
    if (((vars *)data)->do_send) {
      char *buf = new char[msg_size];
    
      int res;
      for (int i = 0; i < 20; i++) {
        LOG("Starting iteration " << i << " of connecting thread; path " << path_id);
        pthread_rwlock_rdlock(&use_rwmutex);
        res = -MPW_SendRecv(0, 0, buf, msg_size, path_id);
        if (res > 0) LOG("Error receiving in connecting thread: " << strerror(res) << "/" << res);
        pthread_rwlock_unlock(&use_rwmutex);
        usleep(1000);
        pthread_rwlock_rdlock(&use_rwmutex);
        res = -MPW_SendRecv(buf, msg_size, 0, 0, path_id);
        if (res > 0) LOG("Error sending in connecting thread: " << strerror(res) << "/" << res);
        pthread_rwlock_unlock(&use_rwmutex);
        usleep(1000);
      }
      
      delete [] buf;
    }
  }
  
  int *res = new int(path_id);
  return res;
}

void *communicating_thread(void *data)
{
  int path_id = ((vars *)data)->num_channels_or_path_id;
  size_t msg_size = ((vars *)data)->msg_size;
  bool do_send = ((vars *)data)->do_send;
  
  char *buf = new char[msg_size];
  int res;
  
  for (int i = 0; i < 20; i++) {
    pthread_rwlock_rdlock(&use_rwmutex);
    if (do_send) {
      LOG("Starting iteration " << i << " of sending thread; path " << path_id);
      res = -MPW_SendRecv(buf, msg_size, 0, 0, path_id);
      if (res > 0) LOG("Error sending in sending thread: " << strerror(res) << "/" << res);
    } else {
      LOG("Starting iteration " << i << " of receiving thread; path " << path_id);
      res = -MPW_SendRecv(0, 0, buf, msg_size, path_id);
      if (res > 0) LOG("Error receiving in receiving thread: " << strerror(res) << "/" << res);
    }
    pthread_rwlock_unlock(&use_rwmutex);
  }
  
  delete [] buf;
  
  return NULL;
}

void *destroy_thread(void *)
{
  while (true) {
    // Get next path to destroy
    pthread_mutex_lock(&destroy_mutex);
    
    while (destroypaths.empty())
      pthread_cond_wait(&destroy_cond, &destroy_mutex);

    int path_id = destroypaths.top();
    destroypaths.pop();

    pthread_mutex_unlock(&destroy_mutex);
    
    // Signals end
    if (path_id == -1)
      break;
    
    // First, lock the accept lock, this will be locked
    // for a long time.
    pthread_mutex_lock(&accept_destroy_mutex);
    
    // Then the next lock should be easier
    // but we can't force it because we might deadlock
    // two messages
    while (pthread_rwlock_trywrlock(&use_rwmutex) != 0)
      usleep(100000);
    
    LOG("Destroying path " << path_id);
    MPW_DestroyPath(path_id);
    pthread_rwlock_unlock(&use_rwmutex);
  }
  return NULL;
}

int main(int argc, char** argv){
  pthread_mutex_init(&destroy_mutex, NULL);
  pthread_mutex_init(&accept_destroy_mutex, NULL);
  pthread_cond_init(&destroy_cond, NULL);
  pthread_rwlock_init(&use_rwmutex, NULL);
  pthread_mutex_init(&log_mutex, NULL);

  
  if(argc==1) {
    printf("usage: ./MPWConcurrentTest <channels (default: 1)> [<message size [kB] (default: 8 kB))>]\n");
    exit(EXIT_FAILURE);
  }

  /* Initialize */
  int num_channels = atoi(argv[1]);
  size_t msgsize = (argc>2) ? atoi(argv[2])*1024 : 8*1024;

  vars v = {num_channels, msgsize, true};
  
  pthread_t server_t, connect_t, send_t, recv_t, destroy_t;
  
  // Before anything, start a separate thread destroying paths that are finished
  pthread_create(&destroy_t, NULL, &destroy_thread, NULL);
  
  // Start accepting connections
  pthread_create(&server_t, NULL, &server_thread, &v);
  
  // Connect to server after giving it some time to start
  pthread_create(&connect_t, NULL, &connecting_thread, &v);
  
  // Accept the connection of the server
  int *accepted_path_id;
  int *connected_path_id;

  pthread_join(server_t, (void **)&accepted_path_id);
  
  // Failed until shown otherwise
  int exit_value = EXIT_FAILURE;
  
  if (*accepted_path_id >= 0) {    
    // Start accepting connections again
    pthread_create(&server_t, NULL, &server_thread, &v);
    
    vars sendv = {*accepted_path_id, msgsize, true};
    vars recvv = {*accepted_path_id, msgsize, false};
    delete accepted_path_id;
    
    pthread_create(&send_t, NULL, &communicating_thread, &sendv);
    pthread_create(&recv_t, NULL, &communicating_thread, &recvv);

    pthread_join(send_t, NULL);
    pthread_join(recv_t, NULL);
    pthread_join(connect_t, (void **)&connected_path_id);
    delete connected_path_id;
    
    vars connectv = {num_channels, msgsize, false};
    pthread_create(&connect_t, NULL, connecting_thread, &connectv);
  
    pthread_join(server_t, (void **)&accepted_path_id);

    if (*accepted_path_id >= 0)
      exit_value = EXIT_SUCCESS;
  }

  delete accepted_path_id;
  pthread_join(connect_t, (void **)&connected_path_id);
  delete connected_path_id;
  
  add_destroypath(-1);
  pthread_join(destroy_t, NULL);

  pthread_mutex_destroy(&log_mutex);
  pthread_mutex_destroy(&destroy_mutex);
  pthread_rwlock_destroy(&use_rwmutex);
  pthread_cond_destroy(&destroy_cond);
  
  MPW_Finalize();

  return exit_value;
}
