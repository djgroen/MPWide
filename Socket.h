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

#ifndef Socket_class
#define Socket_class

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <arpa/inet.h>

const int MAXHOSTNAME = 200;
const int MAXCONNECTIONS = 5;
const int WINSIZE = 1*1024*1024;

#define MPWIDE_SOCKET_RDMASK 1
#define MPWIDE_SOCKET_WRMASK 2

class Socket
{
 public:
  Socket();
  virtual ~Socket();

  // Server initialization
  bool create();
  bool bind ( const int port );
  bool listen() const;
  bool accept ( Socket& ) const;

  // Client initialization
  bool connect ( const std::string host, const int port );

  // Data Transimission
  bool send (const char* s, long long int size ) const;
  int recv (char* s, long long int size ) const;

  // Light-weight, non-blocking
  int isend (const char* s, long long int size ) const;
  int irecv (char* s, long long int size ) const;

  // Check if the socket is readable / writable. Timeout is 2 minutes.
  int select_me (int mask) const;
  int select_me (int mask, int timeout_val) const;

  void set_non_blocking ( const bool );
  void setWin(int size);

  bool is_valid() const { return m_sock != -1; }

  void close();
  void closeServer();

  int getSock() const { return m_sock; }

 private:
  int m_sock;
  int s_sock; //socket descriptor for server.
  sockaddr_in m_addr;
  #ifdef MSG_NOSIGNAL
    static const int tcp_send_flag = MSG_NOSIGNAL;
  #else //OSX Case
    static const int tcp_send_flag = SO_NOSIGPIPE;
  #endif

};

#endif
