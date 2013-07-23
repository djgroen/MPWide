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

<<<<<<< HEAD
#define REPORT_BUFFERSIZES 0
#define EXIT_ON_ERROR

=======
>>>>>>> 9adcb4c0c6c71c7d1af1ea451f7aa7e7f05f3a67
#include "Socket.h"
#include "string.h"
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <cstdlib>
#include <stdio.h>

#include "mpwide-macros.h"

using namespace std;

Socket::Socket() :
  m_sock ( -1 )
{
  memset(&m_addr, 0, sizeof( m_addr ));
  set_non_blocking(false);
  refs = new int(1);
}

Socket::Socket(const Socket& other) : m_sock(other.m_sock), m_addr(other.m_addr), refs(other.refs)
{
  ++(*refs);
}

Socket::~Socket()
{
  if (--(*refs) == 0) {
    if ( is_valid() )
      ::close ( m_sock );
    delete refs;
  }
}

bool Socket::create()
{
  m_sock = socket ( AF_INET, SOCK_STREAM, 0);

  if ( ! is_valid() ) {
    LOG_ERR("Failed to create socket.");
    return false;
  }

  int on = 1;
  if ( setsockopt ( m_sock, SOL_SOCKET, SO_REUSEADDR, ( const char* ) &on, sizeof ( on ) ) == -1 ) {
    LOG_ERR("Failed to set socket option.");
    return false;
  }

  setWin(WINSIZE);
  
  return true;
}

/* Setting a window size. */
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
      LOG_ERR("bind: Invalid socket. ");
      return false;
    }
  m_addr.sin_family = AF_INET;
  m_addr.sin_addr.s_addr = INADDR_ANY;
  m_addr.sin_port = htons(port);
  int bind_return = ::bind(m_sock, (struct sockaddr *) &m_addr, sizeof (m_addr));
  if ( bind_return == -1 )
    {
      LOG_ERR("bind: Failed to bind.");
      return false;
    }
  return true;
}


bool Socket::listen() const
{
  if ( ! is_valid() )
    {
      LOG_ERR("listen: Invalid socket.");
      return false;
    }
  int listen_return = ::listen ( m_sock, MAXCONNECTIONS );
  if ( listen_return == -1 )
  {
    LOG_ERR("listen failed: " << string(strerror(errno)) << "/" << errno);
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
    LOG_ERR("accept failed: " << string(strerror(errno)) << "/" << errno);
    return false;
  }
  else {
    set_non_blocking(true);
    return true;
  }
}


bool Socket::send ( const char* s, long long int size ) const
{
  /* args: FD_SETSIZE,writeset,readset,out-of-band sent, timeout*/
  int ok = 0;

  int count = 0;
  size_t bytes_sent = 0;
  size_t sendsize = (size_t)size;

  while(bytes_sent < size) {
    ok = Socket_select(0, m_sock, MPWIDE_SOCKET_RDMASK, 10, 0);
;
    if (ok == MPWIDE_SOCKET_WRMASK) {
      count = 0;
      ssize_t status = ::send(m_sock, s+bytes_sent, sendsize-bytes_sent, tcp_send_flag);
      if ( status == -1 ) {
        LOG_ERR("Socket.send error. Details: " << errno << "/" << strerror(errno));
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
<<<<<<< HEAD
        fprintf(stderr,"Socket error: %d, %s\n",ok, strerror(errno));
        fflush(stderr);
        #ifdef EXIT_ON_ERROR
        exit(9);
=======
        LOG_ERR("Socket error: " << ok << ", " << strerror(errno));
        #if EXIT_ON_SENDRECV_ERROR == 1
        exit(1);
        #else
        return false;
>>>>>>> 9adcb4c0c6c71c7d1af1ea451f7aa7e7f05f3a67
        #endif
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
  int ok = 0;
  
  int count = 0;
  size_t bytes_recv = 0;
  size_t recvsize = (size_t)size;
  
  while(bytes_recv < size) {
    ok = Socket_select(m_sock, 0, MPWIDE_SOCKET_WRMASK, 10, 0);
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
        LOG_ERR("Socket error: " << ok << ", " << strerror(errno));
        #if EXIT_ON_SENDRECV_ERROR == 1
          exit(1);
        #else
          return -1;
        #endif
      } else {
        usleep(50000);
        cout << "r";
        if(++count == 1) {
          LOG_ERR("Recv timeout: " << errno << " / " << strerror(errno));
          LOG_ERR("We will keep trying to receive for a while...");
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
<<<<<<< HEAD
  if ( status < 0 ) {
    cout << "irecv: status = " << status << " errno = " << errno << "/" << strerror(errno) << endl;
    #ifdef EXIT_ON_ERROR
    exit(1);
    #endif
  }
=======
  #if LOG_LVL >= LVL_ERR || EXIT_ON_SENDRECV_ERROR == 1
    if ( status <= 0 ) {
      #if LOG_LVL >= LVL_ERR
        if (status == 0) {
          cout << "irecv: connection reset by peer.status = " << status << " errno = " << errno << "/" << strerror(errno) << endl;
        } else {
          cout << "irecv: status = " << status << " errno = " << errno << "/" << strerror(errno) << endl;
        }
      #endif
      #if EXIT_ON_SENDRECV_ERROR == 1
        exit(1);
      #endif
    }
  #endif
>>>>>>> 9adcb4c0c6c71c7d1af1ea451f7aa7e7f05f3a67
  return status;
}

int Socket::isend ( const char* s, long long int size ) const
{
  int status = ::send ( m_sock, s, size, tcp_send_flag );
<<<<<<< HEAD
  if ( status < 0 ) {
    cerr << "isend: status = " << status << " errno = " << errno << "/"<< strerror(errno) << endl;
    #ifdef EXIT_ON_ERROR
    exit(1);
    #endif
  }
=======
  #if LOG_LVL >= LVL_ERR || EXIT_ON_SENDRECV_ERROR == 1
    if ( status < 0 ) {
      LOG_ERR("isend: status = " << status << " errno = " << errno << "/"<< strerror(errno));
      #if EXIT_ON_SENDRECV_ERROR == 1
        exit(1);
      #endif
    }
  #endif
>>>>>>> 9adcb4c0c6c71c7d1af1ea451f7aa7e7f05f3a67
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
    #if REPORT_ERRORS > 0
      cerr << "Could not convert address'" << host << "': " << string(strerror(errno)) << "/" << errno << endl;
    #endif
    return false;
  }

  socklen_t sz = sizeof(m_addr);
  status = ::connect(m_sock, (struct sockaddr *) &m_addr, sz);
  if(status == -1) {
      LOG_ERR("Could not connect to port: " << string(strerror(errno)) << "/" << errno);
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
    LOG_ERR("socket is connected! " << error_buf);
    return true;
  }
  else {
    LOG_ERR("socket is NOT connected! " << error_buf);
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
	
  	fd_set *rsock = 0, *wsock = 0;
	fd_set rfd, wfd;
	
	if ((mask&MPWIDE_SOCKET_RDMASK) != MPWIDE_SOCKET_RDMASK) {
		rsock = (fd_set *)&rfd;
		FD_ZERO(rsock);
		FD_SET(rs,rsock);
	}
	if ((mask&MPWIDE_SOCKET_WRMASK) != MPWIDE_SOCKET_WRMASK) {
		wsock = (fd_set *)&wfd;
		FD_ZERO(wsock);
		FD_SET(ws,wsock);
	}
	
	/* args: FD_SETSIZE,writeset,readset,out-of-band sent, timeout*/
	const int ok = select(max(rs, ws)+1, rsock, wsock, (fd_set *)0, &timeout);
	
	if(ok > 0) {
		return (rsock && FD_ISSET(rs,rsock) ? MPWIDE_SOCKET_RDMASK : 0)
		| (wsock && FD_ISSET(ws,wsock) ? MPWIDE_SOCKET_WRMASK : 0);
	} else if (ok == 0 || errno == EINTR) { // Interruptions don't matter
		return 0;
	} else {
    LOG_ERR("select_me: " << strerror(errno) << "/" << errno);
		return -1;
	}
}
