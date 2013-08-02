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

//// Logging macros ////

#define LVL_NONE -1
#define LVL_ERR 0
#define LVL_WARN 2
#define LVL_INFO 4
#define LVL_DEBUG 6
#define LVL_TRACE 6

// SET THE LOG LEVEL
#define LOG_LVL LVL_ERR

#if LOG_LVL >= LVL_ERR
#define LOG_ERR(MSG) cout << MSG << endl
#else
#define LOG_ERR(MSG)
#endif
#if LOG_LVL >= LVL_WARN
#define LOG_WARN(MSG) cout << MSG << endl
#else
#define LOG_WARN(MSG)
#endif
#if LOG_LVL >= LVL_INFO
#define LOG_INFO(MSG) cout << MSG << endl
#else
#define LOG_INFO(MSG)
#endif
#if LOG_LVL >= LVL_DEBUG
#define LOG_DEBUG(MSG) cout << MSG << endl
#else
#define LOG_DEBUG(MSG)
#endif
#if LOG_LVL >= LVL_TRACE
#define LOG_TRACE(MSG) cout << MSG << endl
#else
#define LOG_TRACE(MSG)
#endif

#define max(X,Y) ((X) > (Y) ? (X) : (Y))
#define min(X,Y) ((X) < (Y) ? (X) : (Y))
#define FLAG_CHECK(X, Y) (((X)&(Y)) == (Y))

#endif
