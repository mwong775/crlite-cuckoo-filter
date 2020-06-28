#ifndef CUCKOO_PAIR_HH
#define CUCKOO_PAIR_HH

#include "cuckoofilter/src/cuckoofilter.h"
#include "cuckoohashtable/hashtable/cuckoohashtable.hh"
#include <math.h>

using namespace std;

template <typename KeyType, size_t bits_per_fp>
class cuckoo_pair
{
    cuckoohashtable::cuckoo_hashtable<KeyType> *table_;
    cuckoofilter::CuckooFilter<KeyType, bits_per_fp> *filter_;
private:
    // using hashtable_t = cuckoohashtable::cuckoo_hashtable<KeyType>;
    // using filter_t = cuckoofilter::CuckooFilter<KeyType, bits_per_fp>;
    size_t num_items_;
    size_t size_;

public:
    explicit cuckoo_pair(const size_t n) : num_items_(n)
    {
        size_ = n / 0.95;
        table_ = new cuckoohashtable::cuckoo_hashtable<KeyType>(size_);
        filter_ = new cuckoofilter::CuckooFilter<KeyType, bits_per_fp>(size_, table_->bucket_count());
    }

    ~cuckoo_pair()
    {
        // delete table_;
        // delete filter_;
    }

    void insert(const KeyType &key)
    {
        pair<size_t, size_t> location = table_.insert(key);

        assert(filter_.Add(key) == cuckoofilter::Ok);
        assert(filter_.Contain(key) == cuckoofilter::Ok);
        // if(cf.Contain(c) != cuckoofilter::Ok) {
        //     cout << "ERROR\n";
        //     return 1;
        // }
    }

    bool lookup(const KeyType &key)
    {
        return filter_.Contain(key) != cuckoofilter::NotFound;
    }

    void info()
    {
        table_.info();
        cout << filter_.Info() << "\n";
    }
    // hashtable_t table_;
    // filter_t filter_;
};

#endif // CUCKOO_PAIR_HH