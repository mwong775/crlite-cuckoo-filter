
#ifndef _CUCKOOHASH_UTIL_H_
#define _CUCKOOHASH_UTIL_H_

#include "cuckoo-config.h"
#include <exception>
#include <utility>
#include <vector>

namespace cuckoohash {
    #if CUCKOO_DEBUG

    #define CUCKOO_DEBUG(fmt, ...)

    #else

    #define CUCKOO_DEBUG(fmt, ...)

    #endif

}

#endif // _CUCKOOHASH_UTIL_H_