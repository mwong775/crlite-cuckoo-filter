#ifndef CUCKOO_HASHTABLE_HH
#define CUCKOO_HASHTABLE_HH

#include <cstring>
#include <functional>
#include <cstdlib>
#include <unistd.h>
#include <utility>

#include "bucketcontainer.h";

namespace cuckoohashtable
{

    template <class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>,
              class Allocator = std::allocator<const Key>, std::size_t SLOT_PER_BUCKET = 4>
    class cuckoo_hashtable
    {

    private:
        using bucket_t = bucket_container<Key, Allocator, SLOT_PER_BUCKET>;
    public: 
        using key_type = typename buckets_t::key_type;
        using size_type = typename buckets_t::size_type;
        using difference_type = std::ptrdiff_t;
        using hasher = Hash;
        using key_equal = KeyEqual;
        using allocator_type = typename buckets_t::allocator_type;

        static constexpr uint16_t slot_per_bucket() { return SLOT_PER_BUCKET; }

        /**
     * Creates a new cuckoohashtable instance
     * 
     * @param n - number of elements to reserve space for initiallly
     * @param hf - hash function instance to use
     * @param equal - equality function instance to use
     * @param alloc ? 
     */
    cuckoohashtable(size_type n = DEFAULT_SIZE, const Hash &hf = Hash(),
                    const KeyEqual &equal = KeyEqual()) : hash_fn_(hf), eq_fn_(equal),
                    buckets_(reserve_calc(n), alloc) {}

    /**
     * Copy constructor
     */
    cuckoohashtable(const cuckoohashtable &other) = default;

    // template <typename K>
    int insert(const hash_value hv, TwoBuckets &b, K &key) {
        return 0;
    }

    /*! hash_function returns the hash function object used by the
     * table. */
    hasher hash_function() const { return hash_fn_; }

    /*! key_eq returns the equality predicate object used by the
     * table. */
    key_equal key_eq() { return eq_fn_; }

    // The hash function
    hasher hash_fn_;

    // The equality function
    key_equal eq_fn_;
};

}; // namespace cuckoohashtable

#endif // CUCKOO_HASHTABLE_HH