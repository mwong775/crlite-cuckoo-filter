#ifndef CUCKOO_HASHTABLE_H_
#define CUCKOO_HASHTABLE_H_

#include "cuckoo-config.h"


template<typename ItemType>
class CuckooHashtable {
    // Storage of items
    Hashtable<itemtype> *table_;

    size_t num_items_;

    inline size_t hash1(const ItemType& item) {

    }

    inline size_t hash2(const ItemType& item) {

    }

public:
    explicit CuckooHashtable(const size_t max_num_keys) : num_items_(0) {
        size_t assoc = 4;
        size_t num_buckets;
    }


    // struct CuckooHashtable {
//     CuckooHashtable(int capacity);
//     ~CuckooHashtable();

//     bool contains(size_t key);
//     void insert(size_t key);
//     void remove(size_t key);

//     size_t num_items;

// }
}