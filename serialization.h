//
//  serialization.h
//  MPWide
//
//  These functions are endian-neutral, and assume big-endian byte order on networks.
//  They are inline and with proper compiler flags will be be very efficient.
//
//  Created by Joris Borgdorff on 02-07-13.
//  Copyright (c) 2013 Derek Groen. All rights reserved.
//

#ifndef __MPWide__serialization__
#define __MPWide__serialization__

#include <cstddef> // size_t
#include <cassert> // assert
#include <cstring> // memset

inline void
serialize_size_t(unsigned char *net_number, const size_t native_number)
{   
    if (sizeof(size_t) >= 8)
    {
        net_number[0] = (native_number >> 56) & 0xff;
        net_number[1] = (native_number >> 48) & 0xff;
        net_number[2] = (native_number >> 40) & 0xff;
        net_number[3] = (native_number >> 32) & 0xff;
    }
    else
    {
        assert(sizeof(size_t) >= 4);
        // First bytes are not defined -- 0
        memset(net_number, 0, 4);
    }
    net_number[4] = (native_number >> 24) & 0xff;
    net_number[5] = (native_number >> 16) & 0xff;
    net_number[6] = (native_number >>  8) & 0xff;
    net_number[7] =  native_number        & 0xff;
}

inline size_t
deserialize_size_t(const unsigned char *net_number)
{
    if (sizeof(size_t) >= 8)
    {
        return ((size_t)net_number[0] << 56)
             | ((size_t)net_number[1] << 48)
             | ((size_t)net_number[2] << 40)
             | ((size_t)net_number[3] << 32)
             | ((size_t)net_number[4] << 24)
             | ((size_t)net_number[5] << 16)
             | ((size_t)net_number[6] <<  8)
             |  (size_t)net_number[7];
    }
    else
    {
        // may cause problems if the sent size is larger than size_t max - but then the
        // memory won't be large enough to hold the data anyway.
        assert(net_number[0] == 0);
        assert(net_number[1] == 0);
        assert(net_number[2] == 0);
        assert(net_number[3] == 0);
        assert(sizeof(size_t) >= 4);
        return ((size_t)net_number[4] << 24)
             | ((size_t)net_number[5] << 16)
             | ((size_t)net_number[6] <<  8)
             |  (size_t)net_number[7];
    }
}

#endif /* defined(__MPWide__serialization__) */
