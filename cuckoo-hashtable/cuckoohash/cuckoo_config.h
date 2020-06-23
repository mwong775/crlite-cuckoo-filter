#ifndef _CUCKOOHASH_CONFIG_H
#define _CUCKOOHASH_CONFIG_H

#include <cstddef>

namespace cuckoohash {

// Default maximum number of keys per bucket
constexpr size_t DEFAULT_SLOT_PER_BUCKET = 4;

// Default number of elements in an empty hash table
constexpr size_t DEFAULT_SIZE = (1U << 16) * DEFAULT_SLOT_PER_BUCKET;

// set to 1 to enable debug output
#define CUCKOO_DEBUG 0

} // namespace cuckoohash

#endif // _CUCKOO_CONFIG_H