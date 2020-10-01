#ifndef CUCKOO_PAIR_HH
#define CUCKOO_PAIR_HH

#include "cuckoofilter/src/cuckoofilter.h"
#include "cuckoohashtable/hashtable/cuckoohashtable.hh"
#include <math.h>
#include <bits/stdc++.h> 
#include "cuckoohashtable/city_hasher.hh"

using namespace std;

template <typename KeyType, size_t bits_per_fp, class Hash = CityHasher<KeyType>>
class cuckoo_pair
{
    cuckoohashtable::cuckoo_hashtable<KeyType, bits_per_fp> *table_; // , CityHasher<KeyType>
    cuckoofilter::CuckooFilter<KeyType, bits_per_fp> *filter_;
private:
    size_t num_items_;
    size_t size_;

public:
    explicit cuckoo_pair(const size_t n) : num_items_(n)
    {
        size_ = n / 0.95;
        table_ = new cuckoohashtable::cuckoo_hashtable<KeyType, bits_per_fp>(size_);
        // filter_ = new cuckoofilter::CuckooFilter<KeyType, bits_per_fp>(size_, table_->hash_function(), table_->bucket_count());
    }

    ~cuckoo_pair()
    {
        // delete table_;
        // delete filter_;
    }

    void insert(const KeyType &key)
    {
        // first insert key into hashtable to get storage location (index & slot)
        pair<size_t, size_t> pos = table_->insert(key);
    }

    bool lookup(const KeyType &key)
    {
        // extra check for fp lookup: hashtable should not have fp's at all
        // if(table_->find(key)) {
        //     cout << "ERROR: fp in hashtable?!?!\n";
        //     return true;
        // }

        return table_->lookup(key);
        // return filter_->Contain(key) == cuckoofilter::Ok;
    }

    bool find(const KeyType &key) {
        return table_->find(key);
    }



    void info()
    {
        table_->info();
        // cout << filter_->Info() << "\n";
    }

    void hashInfo() {
        table_->hashInfo();
    }
    // hashtable_t table_;
    // filter_t filter_;
};

#endif // CUCKOO_PAIR_HH