#ifndef CUCKOO_HASHTABLE_HH
#define CUCKOO_HASHTABLE_HH

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "bucketcontainer.hh"

namespace cuckoohashtable
{

    template <class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>,
              class Allocator = std::allocator<Key>, std::size_t SLOT_PER_BUCKET = 4>
    class cuckoo_hashtable
    {

    private:
        using buckets_t = bucket_container<Key, Allocator, SLOT_PER_BUCKET>;

    public:
        using key_type = typename buckets_t::key_type;
        using size_type = typename buckets_t::size_type;
        // using difference_type = std::ptrdiff_t; // don't think this is needed
        using hasher = Hash;
        using key_equal = KeyEqual;
        using allocator_type = typename buckets_t::allocator_type;
        using reference = typename buckets_t::reference;
        using const_reference = typename buckets_t::const_reference;
        using pointer = typename buckets_t::pointer;
        using const_pointer = typename buckets_t::const_pointer;

        static constexpr uint16_t slot_per_bucket() { return SLOT_PER_BUCKET; }

        /**
     * Creates a new cuckoohashtable instance
     * 
     * @param n - number of elements to reserve space for initiallly
     * @param hf - hash function instance to use
     * @param equal - equality function instance to use
     * @param alloc ? 
     */
        cuckoo_hashtable(size_type n = (1U << 16) * 4, const Hash &hf = Hash(),
                         const KeyEqual &equal = KeyEqual(), const Allocator &alloc = Allocator()) : hash_fn_(hf), eq_fn_(equal),
                                                                                                     buckets_(reserve_calc(n), alloc) {}

        /**
     * Copy constructor
     */
        cuckoo_hashtable(const cuckoo_hashtable &other) = default;

        /**
     * Returns the function that hashes the keys
     *
     * @return the hash function
     */
        hasher hash_function() const { return hash_fn_; }

        /**
     * Returns the function that compares keys for equality
     *
     * @return the key comparison function
     */
        key_equal key_eq() { return eq_fn_; }

        /**
   * Returns the allocator associated with the map
   *
   * @return the associated allocator
   */
        allocator_type get_allocator() const { return buckets_.get_allocator(); }

        /**
   * Returns the hashpower of the table, which is log<SUB>2</SUB>(@ref
   * bucket_count()).
   *
   * @return the hashpower
   */
        size_type hashpower() const { return buckets_.hashpower(); }

        /**
   * Returns the number of buckets in the table.
   *
   * @return the bucket count
   */
        size_type bucket_count() const { return buckets_.size(); }

        /**
   * Returns whether the table is empty or not.
   *
   * @return true if the table is empty, false otherwise
   */
        bool empty() const { return size() == 0; }

        /**
     * Returns the number of elements in the table.
     *
     * @return number of elements in the table
     */
        size_type size() const
        { // TODO: fix this
            return 0;
        }

        /** Returns the current capacity of the table, that is, @ref bucket_count()
   * &times; @ref slot_per_bucket().
   *
   * @return capacity of table
   */
        size_type capacity() const { return bucket_count() * slot_per_bucket(); }

        /**
   * Returns the percentage the table is filled, that is, @ref size() &divide;
   * @ref capacity().
   *
   * @return load factor of the table
   */
        double load_factor() const
        {
            return static_cast<double>(size()) / static_cast<double>(capacity());
        }

        /**
   * Inserts the key-value pair into the table. Equivalent to calling @ref
   * upsert with a functor that does nothing.
   */
        template <typename K>
        bool insert(K &&key)
        {
            // get hashed key
            size_type hv = hashed_key(key);
            std::cout << "hashed key: " << hv << "\n";
            // find position in table
            // add to bucket
            // return status

            // std::cout << "inserting " << key << "\n";
            return ok == true;
            // return upsert(
            //     std::forward<K>(key), [](mapped_type &) {},
            //     std::forward<Args>(val)...);
        }

        /** Searches the table for @p key, and returns the associated value it
   * finds. @c mapped_type must be @c CopyConstructible.
   *
   * @tparam K type of the key
   * @param key the key to search for
   * @return the value associated with the given key
   * @throw std::out_of_range if the key is not found
   */
        template <typename K>
        key_type find(const K &key) const
        {
            // std::cout << "finding " << key << "\n";
            // const hash_value hv = hashed_key(key);
            return key;
            // const auto b = snapshot_and_lock_two<normal_mode>(hv);
            // const table_position pos = cuckoo_find(key, hv.partial, b.i1, b.i2);
            // if (pos.status == ok)
            // {
            //     return buckets_[pos.index].key(pos.slot);
            // }
            // else
            // {
            //     throw std::out_of_range("key not found in table");
            // }
        }

    private:
        template <typename K>
        size_type hashed_key(const K &key) const
        {
            return hash_function()(key);
        }

        // hashsize returns the number of buckets corresponding to a given
        // hashpower.
        static inline size_type hashsize(const size_type hp)
        {
            return size_type(1) << hp;
        }

        // hashmask returns the bitmask for the buckets array corresponding to a
        // given hashpower.
        static inline size_type hashmask(const size_type hp)
        {
            return hashsize(hp) - 1;
        }

        class TwoBuckets
        {
        public:
            TwoBuckets() {}
            TwoBuckets(size_type i1_, size_type i2_)
                : i1(i1_), i2(i2_) {}

            size_type i1, i2;
        };

        // locks the two bucket indexes, always locking the earlier index first to
        // avoid deadlock. If the two indexes are the same, it just locks one.
        //
        // throws hashpower_changed if it changed after taking the lock.
        TwoBuckets lock_two(size_type, size_type i1, size_type i2) const
        {
            return TwoBuckets(i1, i2);
        }

        // Data storage types and functions

        // The type of the bucket
        using bucket = typename buckets_t::bucket;

        // Status codes for internal functions

        enum cuckoo_status
        {
            ok,
            failure,
            failure_key_not_found,
            failure_key_duplicated,
            failure_table_full,
            failure_under_expansion,
        };

        // A composite type for functions that need to return a table position, and
        // a status code.
        struct table_position
        {
            size_type index;
            size_type slot;
            cuckoo_status status;
        };

        // Miscellaneous functions

        // reserve_calc takes in a parameter specifying a certain number of slots
        // for a table and returns the smallest hashpower that will hold n elements.
        static size_type reserve_calc(const size_type n)
        {
            const size_type buckets = (n + slot_per_bucket() - 1) / slot_per_bucket();
            size_type blog2;
            for (blog2 = 0; (size_type(1) << blog2) < buckets; ++blog2)
                ;
            assert(n <= buckets * slot_per_bucket() && buckets <= hashsize(blog2));
            return blog2;
        }

        // Member variables

        // The hash function
        hasher hash_fn_;

        // The equality function
        key_equal eq_fn_;

        // container of buckets. The size or memory location of the buckets cannot be
        // changed unless all the locks are taken on the table. Thus, it is only safe
        // to access the buckets_ container when you have at least one lock held.
        //
        // Marked mutable so that const methods can rehash into this container when
        // necessary.
        mutable buckets_t buckets_;
    };

}; // namespace cuckoohashtable

#endif // CUCKOO_HASHTABLE_HH