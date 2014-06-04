//
//  mpwide-macros.h
//  CMuscle
//
//  Created by Joris Borgdorff on 22-07-13.
//  Copyright (c) 2013 Joris Borgdorff. All rights reserved.
//

#ifndef CMuscle_mpwide_macros_h
#define CMuscle_mpwide_macros_h

#define MAX_NUM_STREAMS 10000
#define MAX_NUM_PATHS 1000

/* Enable (define)/Disable(don't define) Performance Timing Measurements */
#define PERF_TIMING

// Run a performance monitoring thread
#define MONITORING 1

// Report the buffer sizes of sockets
#define REPORT_BUFFERSIZES 1

// Immediately exit if a send or receive error is found
#define EXIT_ON_SENDRECV_ERROR 1

// Use a smaller number of streams for small messages 
#define OptimizeStreamCount 1
// standard maximum segment size * 2.
#define BytesPerStream (2*1380)

/* TimeOut in milliseconds. 0 means no timeout */
#define InitStreamTimeOut 1

#define SendRecvInputReport 0

/* MPW-CP loads data in chunks and then ships it out using MPWSendRecv.
   this paramater determines the chunk size. A larger size means a more
   efficient transfer but also larger memory usage. */

#define MpwCpReadBufferSize (1024*1024*1024)

//// Logging macros ////

#define LVL_NONE -1
#define LVL_ERR 0
#define LVL_WARN 2
#define LVL_INFO 4
#define LVL_DEBUG 6
#define LVL_TRACE 6

// SET THE LOG LEVEL
#define LOG_LVL LVL_WARN

#if LOG_LVL > LVL_NONE
#include <pthread.h>
static pthread_mutex_t __log_mutex__ = PTHREAD_MUTEX_INITIALIZER;
#define DO_LOG_(MSG) { pthread_mutex_lock(&__log_mutex__); std::cout << MSG << std::endl; pthread_mutex_unlock(&__log_mutex__); }
#endif

#if LOG_LVL >= LVL_ERR
#define LOG_ERR(MSG) DO_LOG_(MSG)
#else
#define LOG_ERR(MSG)
#endif
#if LOG_LVL >= LVL_WARN
#define LOG_WARN(MSG) DO_LOG_(MSG)
#else
#define LOG_WARN(MSG)
#endif
#if LOG_LVL >= LVL_INFO
#define LOG_INFO(MSG) DO_LOG_(MSG)
#else
#define LOG_INFO(MSG)
#endif
#if LOG_LVL >= LVL_DEBUG
#define LOG_DEBUG(MSG) DO_LOG_(MSG)
#else
#define LOG_DEBUG(MSG)
#endif
#if LOG_LVL >= LVL_TRACE
#define LOG_TRACE(MSG) DO_LOG_(MSG)
#else
#define LOG_TRACE(MSG)
#endif

#define max(X,Y) ((X) > (Y) ? (X) : (Y))
#define min(X,Y) ((X) < (Y) ? (X) : (Y))
#define FLAG_CHECK(X, Y) (((X)&(Y)) == (Y))

#endif
