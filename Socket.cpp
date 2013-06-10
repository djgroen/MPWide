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

#define REPORT_BUFFERSIZES 0

#include "Socket.h"
#include "string.h"
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <cstdlib>
#include <stdio.h>

#define min(X,Y)   ((X) < (Y) ? (X) : (Y))
using namespace std;

Socket::Socket() :
  m_sock ( -1 )
{
  memset ( &m_addr,
	   0,
	   sizeof ( m_addr ) );
//  set_non_blocking(true);
}

Socket::~Socket()
{
  if ( is_valid() )
    ::close ( m_sock );
}

bool Socket::create()
{
  m_sock = socket ( AF_INET, SOCK_STREAM, 0);
  s_sock = m_sock;

  if ( ! is_valid() ) {
    cout << "Failed to create socket." << endl;
    return false;
  }

  int on = 1;
  if ( setsockopt ( m_sock, SOL_SOCKET, SO_REUSEADDR, ( const char* ) &on, sizeof ( on ) ) == -1 ) {
    cout << "Failed to set socket option." << endl;
    return false;
  }

  /* Setting a window size. */
  if(WINSIZE > 0) {
    setsockopt(m_sock, SOL_SOCKET, SO_SNDBUF, (char *) &WINSIZE, sizeof(WINSIZE));
    setsockopt(m_sock, SOL_SOCKET, SO_RCVBUF, (char *) &WINSIZE, sizeof(WINSIZE));
  }

  return true;
}

void Socket::setWin(int size)
{
  if(size > 0) {
    setsockopt(m_sock, SOL_SOCKET, SO_SNDBUF, (char *) &size, sizeof(int));
    setsockopt(m_sock, SOL_SOCKET, SO_RCVBUF, (char *) &size, sizeof(int));
  }
}

void Socket::close()
{
  shutdown(m_sock,SHUT_RDWR);
  ::close(m_sock);
}

void Socket::closeServer()
{
  ::close(s_sock);
}

bool Socket::bind ( const int port )
{
  if ( ! is_valid() )
    {
      cout << "bind: Invalide socket. " << endl;
      return false;
    }
  m_addr.sin_family = AF_INET;
  m_addr.sin_addr.s_addr = INADDR_ANY;
  m_addr.sin_port = htons(port);
  int bind_return = ::bind(m_sock, (struct sockaddr *) &m_addr, sizeof (m_addr));
  if ( bind_return == -1 )
    {
      cout << "bind: Failed to bind." << endl;
      return false;
    }
  return true;
}


bool Socket::listen() const
{
  if ( ! is_valid() )
    {
      cout << "listen: Invalid socket." << endl;
      return false;
    }
  int listen_return = ::listen ( m_sock, MAXCONNECTIONS );
  if ( listen_return == -1 )
    {
      cout << "listen: Failed to listen." << endl;
      return false;
    }

  #if REPORT_BUFFERSIZES > 0
  int tcp_sbuf = 0;
  int tcp_rbuf = 0;
  socklen_t tcp_srlen = sizeof(tcp_sbuf);
  getsockopt(m_sock, SOL_SOCKET, SO_SNDBUF, (char*) &tcp_sbuf, &tcp_srlen);
  getsockopt(m_sock, SOL_SOCKET, SO_RCVBUF, (char*) &tcp_rbuf, &tcp_srlen);

  cout << "Channel-specific tcp window sizes (send/recv): " << tcp_sbuf << "/" << tcp_rbuf << endl;
  #endif
  return true;
}

bool Socket::accept ( Socket& new_socket ) const
{
  new_socket.s_sock = s_sock;
  int addr_length = sizeof ( m_addr );
  new_socket.m_sock = ::accept ( s_sock, ( sockaddr * ) &m_addr, ( socklen_t * ) &addr_length );

  if ( new_socket.m_sock <= 0 ) {
    cout << "accept: Failed to accept. Returns " << new_socket.m_sock << endl;
    cout << "errno = " << errno << ". Message is: " << strerror(errno) << endl;
    return false;
  }
  else {
    return true;
  }
}


bool Socket::send ( const char* s, long long int size ) const
{
  struct timeval timeout;
  timeout.tv_sec = 10;
  timeout.tv_usec = 0;

  /* args: FD_SETSIZE,writeset,readset,out-of-band sent, timeout*/
  int ok = 0;

  fd_set sock;
  FD_ZERO(&sock);
  FD_SET(m_sock,&sock);
  ok = select(m_sock+1, (fd_set *) 0, &sock, (fd_set *) 0, &timeout);
  int count = 0;
  while(ok<1) {

    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    FD_ZERO(&sock);
    FD_SET(m_sock,&sock);
//BUG: ??? Test later!
    ok = select(m_sock+1, &sock, (fd_set *) 0, (fd_set *) 0, &timeout);
    if(ok<1) {
      usleep(50000);
      cout << "s";
      count++;
      if(ok<0) {
        fprintf(stderr,"Socket error: %d, %s\n",ok, strerror(errno));
        fflush(stderr);
        exit(-9);
        }
      }
    else { count = 0; }
    if(count>5) {
      cerr << "Send timeout: " << errno << " / " << strerror(errno) << endl;
      count = 0;
      break;
    }
  }

  int bytes_sent = 0;
  while(bytes_sent < size) {
    int status = ::send ( m_sock, s+bytes_sent, size-bytes_sent, tcp_send_flag );
    if ( status == -1 ) {
      cerr << "Socket.send error. Details: " << errno << "/"<< strerror(errno) << endl;
      return false;
    }
    bytes_sent += status;
  }
  return true;
}


int Socket::recv ( char* s, long long int size ) const
{
  struct timeval timeout;
  timeout.tv_sec = 10;
  timeout.tv_usec = 0;

  fd_set sock;
  FD_ZERO(&sock);
  FD_SET(m_sock,&sock);

  int ok = 0;
  int count = 0;

  while(ok<1) {
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    FD_ZERO(&sock);
    FD_SET(m_sock,&sock);

    ok = select(m_sock+1, &sock, (fd_set *) 0, (fd_set *) 0, &timeout);
    if(ok<1) {
      usleep(50000);
      cout << "r";
      count=1;
      if(ok<0) {
        fprintf(stderr,"Socket error: %d, %s\n",ok, strerror(errno));
        fflush(stderr);
        exit(-9);
      }
    }
    else { count = 0; }
    if(count == 1) {
      cerr << "Recv timeout: " << errno << " / " << strerror(errno) << endl;
      cerr << "We will keep trying to receive for a while..." << endl;
      count++;
    }
    if(count == 100) { /* Too many problems to get a socket. Let's just consider this receive to have failed. */
      return 0;
    }
  }

  int status = ::recv ( m_sock, s, size, 0 );

  if ( status < 0 ) {
    cout << "status == " << status << " errno == " << errno << "  in Socket::recv\n";
    cout << strerror(errno) << endl;
    return status;
  }
  else if ( status == 0 ) {
    return 0;
  }
  else {
    //memcpy(s,buf,size);
    return status;
  }
}

int Socket::irecv ( char* s, long long int size ) const
{
  int status = ::recv ( m_sock, s, size, 0 );
  if ( status < 0 ) {
    cout << "irecv: status = " << status << " errno = " << errno << "/" << strerror(errno) << endl;
    return 0;
    exit(-1);
  }
  return status;
}

int Socket::isend ( const char* s, long long int size ) const
{
  int status = ::send ( m_sock, s, size, tcp_send_flag );
  if ( status < 0 ) {
    cerr << "isend: status = " << status << " errno = " << errno << "/"<< strerror(errno) << endl;
    exit(-1);
    return 0;
  }
  return status;
}

int Socket::select_me (int mask, int timeout_val) const
/*
 Returns:
 0 if no access.
 1 if read.
 2 if write.
 3 if both.
 int mask is...
 0 if we check for read & write.
 1 if we check for write only.
 2 if we check for read only.
*/
{
  /* args: FD_SETSIZE,writeset,readset,out-of-band sent, timeout*/
  int ok = 0;
  int access = 0;

  fd_set rsock, wsock;
  FD_ZERO(&rsock);
  FD_ZERO(&wsock);
  if(mask%2 == 0) { FD_SET(m_sock,&rsock); }
  if(mask/2 == 0) { FD_SET(m_sock,&wsock); }

  struct timeval timeout;
  timeout.tv_sec  = timeout_val;
  timeout.tv_usec = 0;

  //  cout << "select(): mask = " << mask << endl;
  ok = select(m_sock+1, &rsock, &wsock, (fd_set *) 0, &timeout);
  if(ok) {
    if(mask%2 == 0) {
      if(FD_ISSET(m_sock,&rsock)) { access++;    }
    }
    if(mask/2 == 0) {
      if(FD_ISSET(m_sock,&wsock)) { access += 2; }
    }
  }
  else if (ok<0){
    cout << "select_me error: " << errno << " Msg: " << strerror(errno) << endl;
  }
  return access;
}

bool Socket::connect ( const string host, const int port )
{
  if ( ! is_valid() ) return false;

  m_addr.sin_family = AF_INET;
  m_addr.sin_port = htons ( port );
  int status = inet_pton ( AF_INET, host.c_str(), &m_addr.sin_addr );

  if ( errno == EAFNOSUPPORT ) return false;
//  status = ::connect ( m_sock, ( sockaddr * ) &m_addr, sizeof ( m_addr ) );

  set_non_blocking(true);
  status = ::connect(m_sock, ( sockaddr *) &m_addr, sizeof(m_addr));
  if(status == false)
  {   
    return false;
  }   

//  set_non_blocking(false);
  int write = select_me(1,1);
  set_non_blocking(false);

  #if REPORT_BUFFERSIZES > 0
  int tcp_sbuf = 0;
  int tcp_rbuf = 0;
  socklen_t tcp_srlen = sizeof(tcp_sbuf);
  getsockopt(m_sock, SOL_SOCKET, SO_SNDBUF, (char*) &tcp_sbuf, &tcp_srlen);
  getsockopt(m_sock, SOL_SOCKET, SO_RCVBUF, (char*) &tcp_rbuf, &tcp_srlen);

  cout << "Channel-specific tcp window sizes (send/recv): " << tcp_sbuf << "/" << tcp_rbuf << endl;
  #endif
  int error_buf = 0;
  socklen_t err_len = sizeof(error_buf);
  getsockopt(m_sock, SOL_SOCKET, SO_ERROR, (char*) &error_buf, &err_len);

  if(write>0 && error_buf<1) {
    #if REPORT_BUFFERSIZES > 0
    cout << "socket is connected! " << error_buf << endl;
    #endif
    return true;
  }
  else {
    #if REPORT_BUFFERSIZES > 0
    cout << "socket is NOT connected! " << error_buf << endl;
    #endif
    return false; 
  }
}

int Socket::select_me (int mask) const
{
  return select_me (mask, 10);
}

void Socket::set_non_blocking ( const bool b )
{
  int opts;
  opts = fcntl (m_sock, F_GETFL);
  if(opts < 0) return;
  if(b) {
    opts = ( opts | O_NONBLOCK );
  }
  else {
    opts = ( opts & ~O_NONBLOCK );
  }
  fcntl(m_sock, F_SETFL, opts);
}
