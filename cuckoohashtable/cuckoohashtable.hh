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
        size_t num_items_;

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
                         const KeyEqual &equal = KeyEqual(), const Allocator &alloc = Allocator()) : num_items_(0), hash_fn_(hf), eq_fn_(equal),
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
        key_equal key_eq() const { return eq_fn_; }

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
            return num_items_;
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

        // Table status & information to print
        void info() const
        {
            std::cout << "CuckooHashtable Status:\n"
                      << "\t\tKeys stored: " << size() << "\n"
                      << "\t\tBucket count: " << bucket_count() << "\n"
                      << "\t\tSlot per bucket: " << slot_per_bucket() << "\n"
                      << "\t\tCapacity: " << capacity() << "\n"
                      << "\t\tLoad factor: " << load_factor() << "\n";
            buckets_.info();
        }

        /**
   * Inserts the key-value pair into the table. Equivalent to calling @ref
   * upsert with a functor that does nothing.
   */
        template <typename K>                           // , typename... Args
        std::pair<size_type, size_type> insert(K &&key) // , Args &&... k
        {
            // std::cout << "inserting " << key << "\n";

            // get hashed key
            size_type hv = hashed_key(key);
            // std::cout << "hashed key: " << hv << "\n";
            // find position in table
            auto b = compute_buckets(hv);
            table_position pos = cuckoo_insert_loop(hv, b, key); // finds insert spot, does not actually insert
            // std::cout << "found spot @ index: " << pos.index << " slot: " << pos.slot << " status: " << pos.status << "\n";
            // add to bucket
            if (pos.status == ok)
            {
                add_to_bucket(pos.index, pos.slot, std::forward<K>(key)); // , std::forward<Args>(k)...
                num_items_++;
            }
            else
            {
                assert(pos.status == failure_key_duplicated);
            }
            return std::make_pair(pos.index, pos.slot);

            // return std::make_pair(iterator(map_.get().buckets_, pos.index, pos.slot),
            //                       pos.status == ok);
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
            // get hashed key
            size_type hv = hashed_key(key);
            // std::cout << "hashed key: " << hv << "\n";
            // find position in table
            auto b = compute_buckets(hv);
            // search in both buckets
            const table_position pos = cuckoo_find(key, b.i1, b.i2);
            if (pos.status == ok)
            {
                return buckets_[pos.index].key(pos.slot);
            }
            else
            {
                throw std::out_of_range("key not found in table :(");
            }

            // const hash_value hv = hashed_key(key);
            return true;
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

        // index_hash returns the first possible bucket that the given hashed key
        // could be.
        static inline size_type index_hash(const size_type hp, const size_type hv)
        {
            return hv & hashmask(hp);
        }

        // alt_index returns the other possible bucket that the given hashed key
        // could be. It takes the first possible bucket as a parameter. Note that
        // this function will return the first possible bucket if index is the
        // second possible bucket, so alt_index(ti, partial, alt_index(ti, partial,
        // index_hash(ti, hv))) == index_hash(ti, hv).
        static inline size_type alt_index(const size_type hp, const size_type hv,
                                          const size_type index)
        {
            // ensure tag is nonzero for the multiply. 0xc6a4a7935bd1e995 is the
            // hash constant from 64-bit MurmurHash2
            // const size_type nonzero_tag = static_cast<size_type>(partial) + 1;
            return (index ^ (hv * 0xc6a4a7935bd1e995)) & hashmask(hp);
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
        TwoBuckets compute_buckets(const size_type hv) const // size_type, size_type i1, size_type i2
        {
            const size_type hp = hashpower();
            const size_type i1 = index_hash(hp, hv);
            const size_type i2 = alt_index(hp, hv, i1);
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

        // Searching types and functions
        // cuckoo_find searches the table for the given key, returning the position
        // of the element found, or a failure status code if the key wasn't found.
        template <typename K>
        table_position cuckoo_find(const K &key, const size_type i1, const size_type i2) const
        {
            int slot = try_read_from_bucket(buckets_[i1], key); // check each slot in bucket 1
            if (slot != -1)
            {
                return table_position{i1, static_cast<size_type>(slot), ok};
            }
            slot = try_read_from_bucket(buckets_[i2], key); // check each slot in bucket 2
            if (slot != -1)
            {
                return table_position{i1, static_cast<size_type>(slot), ok};
            }
            return table_position{0, 0, failure_key_not_found};
        }

        // try_read_from_bucket will search the bucket for the given key and return
        // the index of the slot if found, or -1 if not found.
        template <typename K>
        int try_read_from_bucket(const bucket &b, const K &key) const
        {
            for (int i = 0; i < static_cast<int>(slot_per_bucket()); ++i)
            {
                if (key_eq()(b.key(i), key))
                {
                    return i;
                }
            }
            return -1;
        }

        // Insertion types and function

        /**
   * Runs cuckoo_insert in a loop until it succeeds in insert and upsert, so
   * we pulled out the loop to avoid duplicating logic.
   *
   * @param hv the hash value of the key
   * @param b bucket locks
   * @param key the key to insert
   * @return table_position of the location to insert the new element, or the
   * site of the duplicate element with a status code if there was a duplicate.
   * In either case, the locks will still be held after the function ends.
   * @throw load_factor_too_low if expansion is necessary, but the
   * load factor of the table is below the threshold
   */
        template <typename K>
        table_position cuckoo_insert_loop(size_type hv, TwoBuckets &b, K &key)
        {
            table_position pos;
            while (true)
            {
                const size_type hp = hashpower();
                pos = cuckoo_insert(hv, b, key);
                switch (pos.status)
                {
                case ok:
                case failure_key_duplicated:
                    return pos; // both cases return location
                case failure_table_full:
                    throw std::out_of_range("table full :(");
                    break;
                //     // Expand the table and try again, re-grabbing the locks
                //     cuckoo_fast_double<TABLE_MODE, automatic_resize>(hp);
                //     b = snapshot_and_lock_two<TABLE_MODE>(hv);
                //     break;
                // case failure_under_expansion:
                //     // The table was under expansion while we were cuckooing. Re-grab the
                //     // locks and try again.
                //     b = snapshot_and_lock_two<TABLE_MODE>(hv);
                //     break;
                default:
                    std::cout << "error on index: " << pos.index << " slot: " << pos.slot << " status: " << pos.status << "\n";
                    info();
                    assert(false);
                }
            }
        }

        // cuckoo_insert tries to find an empty slot in either of the buckets to
        // insert the given key into, performing cuckoo hashing if necessary. It
        // expects the locks to be taken outside the function. Before inserting, it
        // checks that the key isn't already in the table. cuckoo hashing presents
        // multiple concurrency issues, which are explained in the function. The
        // following return states are possible:
        //
        // ok -- Found an empty slot, locks will be held on both buckets after the
        // function ends, and the position of the empty slot is returned
        //
        // failure_key_duplicated -- Found a duplicate key, locks will be held, and
        // the position of the duplicate key will be returned
        //
        // failure_under_expansion -- Failed due to a concurrent expansion
        // operation. Locks are released. No meaningful position is returned.
        //
        // failure_table_full -- Failed to find an empty slot for the table. Locks
        // are released. No meaningful position is returned.
        template <typename K>
        table_position cuckoo_insert(const size_type hv, TwoBuckets &b, K &&key)
        {
            int res1, res2; // gets indices
            bucket &b1 = buckets_[b.i1];
            // std::cout << "b1: " << b.i1 << "\n";
            if (!try_find_insert_bucket(b1, res1, hv, key))
            {
                return table_position{b.i1, static_cast<size_type>(res1),
                                      failure_key_duplicated};
            }
            bucket &b2 = buckets_[b.i2];
            // std::cout << "b2: " << b.i2 << "\n";
            if (!try_find_insert_bucket(b2, res2, hv, key))
            {
                return table_position{b.i2, static_cast<size_type>(res2),
                                      failure_key_duplicated};
            }
            if (res1 != -1)
            {
                return table_position{b.i1, static_cast<size_type>(res1), ok};
            }
            if (res2 != -1)
            {
                return table_position{b.i2, static_cast<size_type>(res2), ok};
            }

            // We are unlucky, so let's perform cuckoo hashing ~
            size_type insert_bucket = 0;
            size_type insert_slot = 0;
            cuckoo_status st = run_cuckoo(b, insert_bucket, insert_slot);
            if (st == ok)
            {
                assert(!buckets_[insert_bucket].occupied(insert_slot));
                assert(insert_bucket == index_hash(hashpower(), hv) || insert_bucket == alt_index(hashpower(), hv, index_hash(hashpower(), hv)));
            }
            assert(st == failure);
            std::cout << "hashtable is full (hashpower = " << hashpower() << ", hash_items = " << size() << ", load factor = " << load_factor() << "), need to increase hashpower\n";
            return table_position{0, 0, failure_table_full};
        }

        // add_to_bucket will insert the given key-value pair into the slot. The key
        // and value will be move-constructed into the table, so they are not valid
        // for use afterwards.
        template <typename K>                                                         // , typename... Args
        void add_to_bucket(const size_type bucket_ind, const size_type slot, K &&key) // , Args &&... k
        {
            buckets_.setK(bucket_ind, slot, std::forward<K>(key)); // , std::forward<Args>(k)...
        }

        // try_find_insert_bucket will search the bucket for the given key, and for
        // an empty slot. If the key is found, we store the slot of the key in
        // `slot` and return false. If we find an empty slot, we store its position
        // in `slot` and return true. If no duplicate key is found and no empty slot
        // is found, we store -1 in `slot` and return true.
        template <typename K>
        bool try_find_insert_bucket(const bucket &b, int &slot,
                                    const size_type hv, K &&key) const
        {
            slot = -1;
            for (int i = 0; i < static_cast<int>(slot_per_bucket()); ++i)
            {
                if (b.occupied(i))
                {
                    if (key_eq()(b.key(i), key))
                    {
                        slot = i;
                        return false;
                    }
                }
                else
                {
                    slot = i;
                }
            }
            std::cout << "slot: " << slot << "\n";
            return true;
        }

        // CuckooRecord holds one position in a cuckoo path. Since cuckoopath
        // elements only define a sequence of alternate hashings for different hash
        // values, we only need to keep track of the hash values being moved, rather
        // than the keys themselves.
        typedef struct
        {
            size_type bucket;
            size_type slot;
            size_type hv;
        } CuckooRecord;

        // The maximum number of items in a cuckoo BFS path. It determines the
        // maximum number of slots we search when cuckooing.
        static constexpr uint8_t MAX_BFS_PATH_LEN = 5;

        // An array of CuckooRecords
        using CuckooRecords = std::array<CuckooRecord, MAX_BFS_PATH_LEN>;

        // run_cuckoo performs cuckoo hashing on the table in an attempt to free up
        // a slot on either of the insert buckets. On success, the bucket and slot
        // that was freed up is stored in insert_bucket and insert_slot. If run_cuckoo
        // returns ok (success), then `b` will be active, otherwise it will not.
        cuckoo_status run_cuckoo(TwoBuckets &b, size_type &insert_bucket,
                                 size_type &insert_slot)
        {
        }
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