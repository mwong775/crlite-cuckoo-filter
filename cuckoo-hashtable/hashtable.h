#ifndef CUCKOO_HASH_HASHTABLE_H_
#define CUCKOO_HASH_HASHTABLE_H_
#include <vector>
#include <array>

#include <assert.h>
#include <sstream>

#include "cuckoo_config.h";
#include "cuckoohash_util.h";
#include "bucket_container.h";

namespace cuckoohash {
    /**
     * Key - type of value sin the table
     * Hash - type of hash func
     * KeyEqual - type of equality caomparison func
     * Allocator - type of allocator
     * SLOT_PER_BUCKET = number of slots for each bucket in the table
     */
    template<class Key, class Hash = std::hash<Key>, 
        class KeyEqual = std::equal_to<Key>,
        class Allocator = std::allocator<const Key>,
        std::size_t SLOT_PER_BUCKET = DEFAULT_SLOT_PER_BUCKET> 
    class hashtable {
        private:
            using bucket_t = bucket_container<Key, Allocator, SLOT_PER_BUCKET>;

        public:
            /** @name Type Declarations */
   
    }
}

#endif // CUCKOO_HASH_HASHTABLE_H_