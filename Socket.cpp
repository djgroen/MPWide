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

#define max(X,Y) ((X) > (Y) ? (X) : (Y))
#define FLAG_CHECK(X, Y) (((X)&(Y)) == (Y))

static int Socket_select(int rs, int ws, fd_set *rsock, fd_set *wsock, struct timeval *timeout);

using namespace std;

Socket::Socket() :
  m_sock ( -1 )
{
  memset(&m_addr, 0, sizeof( m_addr ));
  set_non_blocking(false);
}

Socket::~Socket()
{
  if ( is_valid() )
    ::close ( m_sock );
}

bool Socket::create()
{
  m_sock = socket ( AF_INET, SOCK_STREAM, 0);

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


bool Socket::listen()
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

bool Socket::accept()
{
  socklen_t addr_length = (socklen_t) sizeof ( m_addr );
  int new_m_sock = ::accept ( m_sock, ( sockaddr * ) &m_addr, &addr_length );

  ::close(m_sock);
  m_sock = new_m_sock;
  
  if ( m_sock <= 0 ) {
    cout << "accept: Failed to accept. Returns " << m_sock << endl;
    cout << "errno = " << errno << ". Message is: " << strerror(errno) << endl;
    return false;
  }
  else {
    set_non_blocking(true);
    return true;
  }
}


bool Socket::send ( const char* s, long long int size ) const
{
  fd_set sock;
  struct timeval timeout;

  /* args: FD_SETSIZE,writeset,readset,out-of-band sent, timeout*/
  int ok = 0;

  int count = 0;
  size_t bytes_sent = 0;
  size_t sendsize = (size_t)size;

  while(bytes_sent < size) {
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    ok = Socket_select(0, m_sock, 0, &sock, &timeout);
;
    if (ok == MPWIDE_SOCKET_WRMASK) {
      count = 0;
      ssize_t status = ::send(m_sock, s+bytes_sent, sendsize-bytes_sent, tcp_send_flag);
      if ( status == -1 ) {
        cerr << "Socket.send error. Details: " << errno << "/"<< strerror(errno) << endl;
        return false;
      }
      bytes_sent += status;
    }
    else {
      usleep(50000);
      #if REPORT_BUFFERSIZES > 0
      cout << "s";
      #endif
      if(ok<0) {
        fprintf(stderr,"Socket error: %d, %s\n",ok, strerror(errno));
        fflush(stderr);
        exit(-9);
        }
      }
      if(++count > 5) {
        cerr << "Send timeout: " << errno << " / " << strerror(errno) << endl;
        count = 0;
        break;
      }
  }

  return true;
}


int Socket::recv ( char* s, long long int size ) const
{
  fd_set sock;
  struct timeval timeout;

  int ok = 0;
  
  int count = 0;
  size_t bytes_recv = 0;
  size_t recvsize = (size_t)size;
  
  while(bytes_recv < size) {
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    ok = Socket_select(m_sock, 0, &sock, 0, &timeout);
    ;

    if (ok == MPWIDE_SOCKET_RDMASK) {
      ssize_t status = ::recv( m_sock, s + bytes_recv, recvsize - bytes_recv, 0 );
      bytes_recv -= status;
      
      if ( status <= 0 ) {
        cout << "status == " << status << " errno == " << errno << "  in Socket::recv\n";
        cout << strerror(errno) << endl;
        return -1;
      }

      count = 0;
    }
    else {
      if(ok < 0) {
        fprintf(stderr,"Socket error: %d, %s\n",ok, strerror(errno));
        return -1;
      } else {
        usleep(50000);
        cout << "r";
        if(++count == 1) {
          cerr << "Recv timeout: " << errno << " / " << strerror(errno) << endl;
          cerr << "We will keep trying to receive for a while..." << endl;
        }
        else if (count == 100) { /* Too many problems to get a socket. Let's just consider this receive to have failed. */
          return -1;
        }
      }
    }
  }

  return size;
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

/*
 Returns:
 -1 on error
 0 if no access.
 MPWIDE_SOCKET_RDMASK if read.
 MPWIDE_SOCKET_WRMASK if write.
 MPWIDE_SOCKET_RDMASK|MPWIDE_SOCKET_WRMASK if both.
 int mask is...
 0 if we check for read & write.
 MPWIDE_SOCKET_RDMASK if we check for write only.
 MPWIDE_SOCKET_WRMASK if we check for read only.
 */
int Socket::select_me (int mask) const
{
  return select_me (mask, 10, 0);
}

int Socket::select_me (int mask, int timeout_val) const
{
    return select_me(mask, timeout_val, 0);
}

int Socket::select_me (int mask, int timeout_s, int timeout_u) const
{
  return Socket_select(m_sock, m_sock, mask, timeout_s, timeout_u);
}

bool Socket::connect ( const string host, const int port )
{
  if ( ! is_valid() ) return false;

  m_addr.sin_family = AF_INET;
  m_addr.sin_port = htons ( port );
  int status = inet_pton ( AF_INET, host.c_str(), &m_addr.sin_addr );

  if ( status < 1 ) {
    #if REPORT_BUFFERSIZES > 0
      cerr << "Could not convert address'" << host << "': " << string(strerror(errno)) << "/" << errno << endl;
    #endif
    return false;
  }

  socklen_t sz = sizeof(m_addr);
  status = ::connect(m_sock, (struct sockaddr *) &m_addr, sz);
  if(status == -1) {
      #if REPORT_BUFFERSIZES > 0
        cerr << "Could not connect to port: " << string(strerror(errno)) << "/" << errno << endl;
      #endif
      return false;
  }

  set_non_blocking(true);
  int write = select_me(MPWIDE_SOCKET_RDMASK,10);

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

/**
 Returns:
 -1 on error
 0 if no access.
 MPWIDE_SOCKET_RDMASK if read.
 MPWIDE_SOCKET_WRMASK if write.
 MPWIDE_SOCKET_RDMASK|MPWIDE_SOCKET_WRMASK if both.
 int mask is...
 0 if we check for read & write.
 MPWIDE_SOCKET_RDMASK if we check for write only.
 MPWIDE_SOCKET_WRMASK if we check for read only.
 */
int Socket_select(int rs, int ws, int mask, int timeout_s, int timeout_u)
{
    struct timeval timeout;
    timeout.tv_sec  = timeout_s;
    timeout.tv_usec = timeout_u;
  
    if (mask == MPWIDE_SOCKET_RDMASK) {
      fd_set wsock;
      return Socket_select(0, ws, 0, &wsock, &timeout);
    }
    else if (mask == MPWIDE_SOCKET_WRMASK) {
      fd_set rsock;
      return Socket_select(rs, 0, &rsock, 0, &timeout);
    }
    else if (mask == 0) {
      fd_set rsock, wsock;
      return Socket_select(rs, ws, &rsock, &wsock, &timeout);
    }
    else return 0;
}

static int Socket_select(int rs, int ws, fd_set *rsock, fd_set *wsock, struct timeval *timeout)
{
  int access = 0;
  if (rsock) {
    FD_ZERO(rsock);
    FD_SET(rs,rsock);
  }
  if (wsock) {
    FD_ZERO(wsock);
    FD_SET(ws,wsock);
  }
  
  /* args: FD_SETSIZE,writeset,readset,out-of-band sent, timeout*/
  const int ok = select(max(rs, ws)+1, rsock, wsock, (fd_set *) 0, timeout);
  if(ok > 0) {
    if (rsock && FD_ISSET(rs,rsock)) {
      access = MPWIDE_SOCKET_RDMASK; // access is always zero by this time
    }
    if (wsock && FD_ISSET(ws,wsock)) {
      access |= MPWIDE_SOCKET_WRMASK;
    }
  }
  else if (ok<0){
    cout << "select_me error: " << errno << " Msg: " << strerror(errno) << endl;
    return -1;
  }
  return access;
}
