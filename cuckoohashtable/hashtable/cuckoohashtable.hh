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
#include <bits/stdc++.h>

#include "bucketcontainer.hh"

// #include "../city_hasher.hh"

namespace cuckoohashtable
{

    template <class Key, std::size_t bits_per_key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>,
              class Allocator = std::allocator<Key>, std::size_t SLOT_PER_BUCKET = 4>
    class cuckoo_hashtable
    {

    private:
        // Type of the fingerprint/partial key. TODO: make it configurable
        using partial_t = uint32_t;
        // Type of the buckets container
        using buckets_t = bucket_container<Key, Allocator, partial_t, SLOT_PER_BUCKET>;
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
                                                                                                     buckets_(reserve_calc(n), alloc), seeds_(bucket_count()), num_lookup_rds_(0) {}

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
   * Returns the percenfpe the table is filled, that is, @ref size() &divide;
   * @ref capacity().
   *
   * @return load factor of the table
   */
        double load_factor() const
        {
            return static_cast<double>(size()) / static_cast<double>(capacity());
        }

        // Table status & information to print
        std::string info() const
        {
            std::stringstream ss;
            ss << "CuckooHashtable Status:\n"
               << "\t\tSlot per bucket: " << slot_per_bucket() << "\n"
               << "\t\tBucket count: " << bucket_count() << "\n"
               << "\t\tCapacity: " << capacity() << "\n\n"
               << "\t\tKeys stored: " << size() << "\n"
               << "\t\tLoad factor: " << load_factor() << "\n";
            return ss.str();
        }

        void bucketInfo() const
        {
            buckets_.info();
        }

        // bucket seeds info
        void seedInfo(std::map<uint16_t, uint16_t> &seed_map) const
        {
            // can use unordered map to track quantity of each rehash count (e.g. lots of 1's, less 2's, etc.)
            // std::cout << "Bucket Seed Info:\n";
            for (size_t i = 0; i < bucket_count(); i++)
            {
                // printSeed(i);
                seed_map[seeds_.at(i)]++;
            }
            std::cout << "rehashing seed map:\n(key : value) = # rehashes : # buckets\n";
            for (auto &k : seed_map)
            {
                std::cout << "{" << k.first << ", " << k.second << "}"
                          << "\n";
            }
            std::cout << "total buckets: " << bucket_count() << "\nlookup rounds: " << num_lookup_rds_ << "\n";
        }

        void printSeed(const size_t i) const
        {
            // if (seeds_.at(i) > 5)
            std::cout << i << ": [ " << seeds_.at(i) << " ]\n";
        }

        void printBucket(const size_t i)
        {
            buckets_.printBucket(i);
            bucket &b = buckets_[i];
            std::cout << "HV's : [";
            for (size_t j = 0; j < slot_per_bucket(); j++)
            {
                if (b.occupied(j))
                    std::cout << hashed_key(b.key(j));
                else
                    std::cout << " ";

                if (j < slot_per_bucket() - 1)
                    std::cout << ", ";
                // partial_t fp = partial_key(hv);
            }
            std::cout << "]\tseed: " << seeds_.at(i) << "\n";
        }

        // Table hash information
        void hashInfo() const
        {
            const size_type hp = hashpower();

            std::cout << "CuckkooHashtable Info:\n"
                      << "\t\tHash power: " << hp << "\n"
                      << "\t\tHash size: " << hashsize(hp) << "\n"
                      << "\t\tHash mask: " << hashmask(hp) << "\n";
        }

        /**
   * Inserts the key-value pair into the table (returns inserted location).
   */
        template <typename K>
        std::pair<size_type, size_type> insert(K &&key)
        {
            // get hashed key
            size_type hv = hashed_key(key);
            partial_t fp = partial_key(hv);

            // std::cout << "HT inserting " << key << " hv: " << hv << " fp: " << fp << "\n";

            // find position in table
            auto b = compute_buckets(key);
            table_position pos = cuckoo_insert_loop(b, key); // finds insert spot, does not actually insert
            // std::cout << "HT inserting key " << key << ": " << pos.index << ", " << pos.slot << "\n";// status: " << pos.status << "\n";

            // add to bucket
            if (pos.status == ok)
            {
                add_to_bucket(pos.index, pos.slot, fp, std::forward<K>(key));
                num_items_++;
            }
            else
            {
                std::cout << "status NOT ok: " << pos.status << "\n";
                assert(pos.status == failure_key_duplicated);
            }
            return std::make_pair(pos.index, pos.slot);
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
        std::pair<int32_t, int32_t> find(const K &key) const // std::pair<size_type, size_type>
        {
            // get hashed key
            // size_type hv = hashed_key(key);

            // find position in table
            auto b = compute_buckets(key);

            // search in both buckets
            const table_position pos = cuckoo_find(key, b.i1, b.i2);
            // return pos.status == ok;
            // return std::make_pair(pos.index, pos.slot);
            if (pos.status == ok)
            {
                // return buckets_[pos.index].key(pos.slot); // this is dumb, maybe update to bool?
                return std::make_pair(pos.index, pos.slot);
            }
            else
            {
                // return -1;
                return std::make_pair(-1, -1);
                // throw std::out_of_range("key not found in table :(");
            }
        }

        void start_lookup() const
        {
            num_lookup_rds_++;
            // std::cout << "starting lookup round " << num_lookup_rds_ << "\n";
        }

        size_t num_rehashes()
        {
            // last lookup should result in no fp's, so no rehash
            return num_lookup_rds_ - 1;
        }

        template <typename K>
        int32_t lookup(const K &key) const
        {
            // find position in table
            auto b = compute_buckets(key);

            // get fingerprint
            uint64_t hv1 = hashed_key(key, seeds_.at(b.i1));
            uint64_t hv2 = hashed_key(key, seeds_.at(b.i2));
            partial_t fp1 = partial_key(hv1);
            partial_t fp2 = partial_key(hv2);

            // search in both buckets
            const table_position pos1 = cuckoo_find_fp(fp1, b.i1);
            const table_position pos2 = cuckoo_find_fp(fp2, b.i2);
            // return pos.status == ok;

            // omg this outer conditional is necessary if false pos. happen to occur in BOTH buckets - pos1 cannot return yet to check pos2 also
            // TODO: modify function to determine if one/both buckets rehashed - perhaps std::pair?? WAIT JK if index is not important, can just return 0 1 or 2 for # buckets HAH
            if (pos1.status == ok || pos2.status == ok)
            {
                if (pos1.status == ok)
                {
                    // assert(pos1.index == b.i1);
                    if (pos1.index != b.i1)
                        return -1;

                    uint16_t &seed = seeds_.at(pos1.index);
                    if (seed < num_lookup_rds_)
                    {
                        seed++;
                        // std::cout << "fp on key: " << key << " hv: " << hv << " fp: " << fp << " at pos " << pos.index << ", " << pos.slot << ", seed to " << seed << "\n";
                    }
                    // return fp1;
                    // return pos1.index;
                }

                // uint64_t hv2 = hashed_key(key, seeds_.at(b.i2));
                // partial_t fp2 = partial_key(hv2);
                // const table_position pos2 = cuckoo_find_fp(fp2, b.i2);

                // if (pos2.index == 1487)
                //     std::cout << "LOOKUP 1487 pos2 key: " << key << " fp: " << fp2 << " status: " << pos2.status << "\t";

                if (pos2.status == ok)
                {
                    assert(pos2.index == b.i2);
                    uint16_t &seed = seeds_.at(pos2.index);
                    if (seed < num_lookup_rds_)
                    {
                        seed++;
                        // std::cout << "fp on key: " << key << " hv: " << hv << " fp: " << fp << " at pos " << pos.index << ", " << pos.slot << ", seed to " << seed << "\n";
                    }
                    // return fp2;
                    return pos2.index;
                }
                return pos1.index; // TODO: INACCURATE TRACKING OF REHASH INDEX - not_found gives index 0 if false pos. is in alt. bucket ONLY
            }
            return -1;
        }

        // returns number of buckets rehashed during after a lookup round
        uint32_t rehash_buckets()
        {
            uint32_t b_count = 0;
            uint32_t last_index;
            for (uint32_t i = 0; i < seeds_.size(); i++)
            {
                if (seeds_.at(i) == num_lookup_rds_) // incremented if false pos. during lookup for fp's
                {
                    last_index = i;
                    b_count++;
                    bucket &b = buckets_[i];
                    for (uint8_t j = 0; j < static_cast<int>(slot_per_bucket()); ++j)
                    {
                        // rehash fp's at bucket index i
                        const size_type key = b.key(j);
                        size_type hv = hashed_key(key, seeds_.at(i));
                        partial_t fp = partial_key(hv);
                        if (b.occupied(j))
                            fp_to_bucket(i, j, fp);
                    }
                }
            }

            // if (b_count == 1)
            //     cout << "LAST REHASHED BUCKET: " << last_index << "\n";

            return b_count;
        }

        uint16_t get_seed(const size_t i) const
        {
            return seeds_.at(i);
        }

        std::vector<uint16_t> get_seeds() const
        { // std::vector<int> &seeds
            return seeds_;
            // seeds.resize(seeds_.size());
            // for(int i = 0; i < seeds_.size(); i++) {
            //     seeds[i] = seeds_.at(i);
            // }
        }

        template <typename key_type>
        void export_table(std::vector<std::vector<key_type>> &fp_table)
        {
            for (int i = 0; i < bucket_count(); i++)
            {
                std::vector<key_type> fp_bucket;
                for (int j = 0; j < static_cast<int>(slot_per_bucket()); j++)
                {
                    fp_bucket.push_back(buckets_[i].partial(j));
                }
                fp_table.push_back(fp_bucket);
            }
        }

    private:
        template <typename K>
        inline size_type hashed_key(const K &key, uint32_t seed = 0) const
        {
            return hash_function()(key, seed);
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

        static inline partial_t partial_key(const size_type hv)
        {
            partial_t fp;
            // fp = (static_cast<uint32_t>(hv)^static_cast<uint32_t>(hv >>
            // bits_per_item));
            fp = hv & ((1ULL << bits_per_key) - 1);
            fp += (fp == 0);

            // std::cout << "fp: " << fp << "\n";

            return fp;
        }

        // index_hash returns the first possible bucket that the given hashed key
        // could be.
        static inline size_type index_hash(const size_type hp, const size_type key) // hv
        {
            const uint32_t hash = key >> 32;
            return hash & hashmask(hp);
            // return hv & hashmask(hp);
        }

        // alt_index returns the other possible bucket that the given hashed key
        // could be. It takes the first possible bucket as a parameter. Note that
        // this function will return the first possible bucket if index is the
        // second possible bucket, so alt_index(ti, partial, alt_index(ti, partial,
        // index_hash(ti, hv))) == index_hash(ti, hv).
        static inline size_type alt_index(const size_type hp, const size_type key,
                                          const size_type index)
        {
            // (libcuckoo) ensure fp is nonzero for the multiply. 0xc6a4a7935bd1e995 is the
            // hash constant from 64-bit MurmurHash2
            // const size_type nonzero_fp = static_cast<size_type>(partial) + 1;
            // (DRECHT) ensure fp is nonzero for the multiply

            // >> (right shift)
            const size_t fp = (key >> hp) + 1;
            // std::cout << "HT hp: " << hp << ", right shift: " << fp << "key: " << key << "\n";

            // ^ (bitwise XOR), & (bitwise AND)
            // std::cout << "HT hashmask(hp): " << hashmask(hp) << "\n";
            return (index ^ (fp * 0xc6a4a7935bd1e995)) & hashmask(hp);
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
        TwoBuckets compute_buckets(const size_type key) const // size_type, size_type i1, size_type i2
        {
            const size_type hp = hashpower();
            const size_type i1 = index_hash(hp, key);
            const size_type i2 = alt_index(hp, key, i1);
            // std::cout << key << ": HT computed buckets " << i1 << " and " << i2 << "\n";
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
            int_least16_t slot;
            slot = try_read_from_bucket(buckets_[i1], key); // check each slot in bucket 1

            if (slot != -1)
                return table_position{i1, static_cast<size_type>(slot), ok};

            slot = try_read_from_bucket(buckets_[i2], key); // check each slot in bucket 2

            if (slot != -1)
                return table_position{i2, static_cast<size_type>(slot), ok};

            return table_position{0, 0, failure_key_not_found};
        }

        table_position cuckoo_find_fp(const partial_t &fp1, const size_type i1) const
        {
            int slot;
            slot = try_fp_in_bucket(buckets_[i1], fp1);

            if (slot != -1)
                return table_position{i1, static_cast<size_type>(slot), ok};
            return table_position{0, 0, failure_key_not_found};

            // slot = try_fp_in_bucket(buckets_[i2], fp2);

            // if (slot != -1)
            //     return table_position{i2, static_cast<size_type>(slot), ok};
            // return table_position{0, 0, failure_key_not_found};
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

        // try_fp_in_bucket will search the bucket for the fingerprint of the given key
        // and return the index of the slot if found, of -1 if not found.
        int try_fp_in_bucket(const bucket &b, const partial_t &p) const
        {
            for (int i = 0; i < static_cast<int>(slot_per_bucket()); ++i)
            {
                if (key_eq()(p, b.partial(i)))
                {
                    // std::cout << "found " << p << " == " << b.partial(i) << " at slot " << i << "\n";
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
        table_position cuckoo_insert_loop(TwoBuckets &b, K &key)
        {
            table_position pos;
            while (true)
            {
                const size_type hp = hashpower();
                pos = cuckoo_insert(b, key);
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
        table_position cuckoo_insert(TwoBuckets &b, K &&key)
        {
            int res1, res2; // gets indices
            bucket &b1 = buckets_[b.i1];
            // std::cout << "b1: " << b.i1 << "\n";
            if (!try_find_insert_bucket(b1, res1, key))
            {
                return table_position{b.i1, static_cast<size_type>(res1),
                                      failure_key_duplicated};
            }
            bucket &b2 = buckets_[b.i2];
            // std::cout << "b2: " << b.i2 << "\n";
            if (!try_find_insert_bucket(b2, res2, key))
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
                assert(insert_bucket == index_hash(hashpower(), key) || insert_bucket == alt_index(hashpower(), key, index_hash(hashpower(), key)));

                return table_position{insert_bucket, insert_slot, ok};
            }
            assert(st == failure);
            std::cout << "hashtable is full (hashpower = " << hashpower() << ", hash_items = " << size() << ", load factor = " << load_factor() << "), need to increase hashpower\n";
            return table_position{0, 0, failure_table_full};
        }

        // add_to_bucket will insert the given key-value pair into the slot. The key
        // and value will be move-constructed into the table, so they are not valid
        // for use afterwards.
        template <typename K>                                                                             // , typename... Args
        void add_to_bucket(const size_type bucket_ind, const size_type slot, const partial_t fp, K &&key) // , Args &&... k
        {
            buckets_.setK(bucket_ind, slot, fp, std::forward<K>(key)); // , std::forward<Args>(k)...
        }

        void fp_to_bucket(const size_type bucket_ind, const size_type slot, const partial_t fp)
        {
            buckets_.setFP(bucket_ind, slot, fp);
        }

        // try_find_insert_bucket will search the bucket for the given key, and for
        // an empty slot. If the key is found, we store the slot of the key in
        // `slot` and return false. If we find an empty slot, we store its position
        // in `slot` and return true. If no duplicate key is found and no empty slot
        // is found, we store -1 in `slot` and return true.
        template <typename K>
        bool try_find_insert_bucket(const bucket &b, int &slot,
                                    K &&key) const
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
            // std::cout << "slot: " << slot << "\n";
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
            size_type key;
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
        cuckoo_status run_cuckoo(TwoBuckets &b, size_type &insert_bucket, size_type &insert_slot)
        {
            // std::cout << "run_cuckoo\n";
            // cuckoo_search and cuckoo_move
            size_type hp = hashpower();
            CuckooRecords cuckoo_path;
            bool done = false;
            while (!done)
            {
                const int depth = cuckoopath_search(hp, cuckoo_path, b.i1, b.i2);
                // std::cout << "depth: " << depth << "\n";
                if (depth < 0)
                {
                    break;
                }

                if (cuckoopath_move(hp, cuckoo_path, depth, b))
                {
                    // store freed up bucket and slot
                    insert_bucket = cuckoo_path[0].bucket;
                    insert_slot = cuckoo_path[0].slot;
                    // std::cout << "insert: " << insert_bucket << ", " << insert_slot << "\n";
                    assert(insert_bucket == b.i1 || insert_bucket == b.i2);
                    assert(!buckets_[insert_bucket].occupied(insert_slot));
                    done = true;
                    break;
                }
            }
            return done ? ok : failure;
        }

        // cuckoopath_search finds a cuckoo path from one of the starting buckets to
        // an empty slot in another bucket. It returns the depth of the discovered
        // cuckoo path on success, and -1 on failure.
        int cuckoopath_search(const size_type hp, CuckooRecords &cuckoo_path,
                              const size_type i1, const size_type i2)
        {
            b_slot x = slot_search(hp, i1, i2);
            if (x.depth == -1)
            {
                return -1;
            }
            // Fill in the cuckoo path slots from the end to the beginning.
            for (int i = x.depth; i >= 0; i--)
            {
                cuckoo_path[i].slot = x.pathcode % slot_per_bucket();
                x.pathcode /= slot_per_bucket();
            }
            // Fill in the cuckoo_path buckets and keys from the beginning to the
            // end, using the final pathcode to figure out which bucket the path
            // starts on.
            CuckooRecord &first = cuckoo_path[0];
            if (x.pathcode == 0)
            {
                first.bucket = i1;
            }
            else
            {
                assert(x.pathcode == 1);
                first.bucket = i2;
            }
            {
                const bucket &b = buckets_[first.bucket];
                if (!b.occupied(first.slot))
                {
                    return 0;
                }
                first.key = b.key(first.slot);
            }
            for (int i = 1; i <= x.depth; ++i)
            {
                CuckooRecord &curr = cuckoo_path[i];
                const CuckooRecord &prev = cuckoo_path[i - 1];
                assert(prev.bucket == index_hash(hp, prev.key) || prev.bucket == alt_index(hp, prev.key, index_hash(hp, prev.key)));
                // We get the bucket that this slot is on by computing the alternate index of the previous bucket
                curr.bucket = alt_index(hp, prev.key, prev.bucket);
                const bucket &b = buckets_[curr.bucket];
                if (!b.occupied(curr.slot))
                {
                    // We can terminate here!
                    return i;
                }
                curr.key = b.key(curr.slot);
            }
            return x.depth;
        }

        // cuckoopath_move moves keys along the given cuckoo path in order to make
        // an empty slot in one of the buckets in cuckoo_insert.
        bool cuckoopath_move(const size_type hp, CuckooRecords &cuckoo_path, size_type depth, TwoBuckets &b)
        {
            // std::cout << "cuckoopath_move\n";
            if (depth == 0)
            {
                // There is a chance that depth == 0, when try_add_to_bucket sees
                // both buckets as full and cuckoopath_search finds one empty? Probably not w/o concurrency
                const size_type bucket_i = cuckoo_path[0].bucket;
                assert(bucket_i == b.i1 || bucket_i == b.i2);
                if (!buckets_[bucket_i].occupied(cuckoo_path[0].slot))
                {
                    std::cout << "found unoccupied\n";
                    return true;
                }
                else
                {
                    std::cout << "cuckoo bucket occupied :(\n";
                    return false;
                }
            }

            while (depth > 0)
            {
                CuckooRecord &from = cuckoo_path[depth - 1];
                CuckooRecord &to = cuckoo_path[depth];
                const size_type fs = from.slot;
                const size_type ts = to.slot;
                TwoBuckets twob;

                bucket &fb = buckets_[from.bucket];
                bucket &tb = buckets_[to.bucket];

                // std::cout << "from: " << from.bucket << ", " << from.slot << "\n";
                // std::cout << "to: " << to.bucket << ", " << to.slot << "\n";

                // checks valid cuckoo, and that the hash value is the same
                if (tb.occupied(ts) || !fb.occupied(fs) || fb.key(fs) != from.key)
                {
                    return false;
                }

                buckets_.setK(to.bucket, ts, fb.partial(fs), std::move(fb.key(fs)));
                buckets_.eraseK(from.bucket, fs);
                depth--;
                // std::cout << "depth: " << depth << "\n";
            }
            return true;
        }

        // A constexpr version of pow that we can use for various compile-time
        // constants and checks.
        static constexpr size_type const_pow(size_type a, size_type b)
        {
            return (b == 0) ? 1 : a * const_pow(a, b - 1);
        }
        // b_slot holds the information for a BFS path through the table.
        struct b_slot
        {
            // The bucket of the last item in the path.
            size_type bucket;
            // a compressed representation of the slots for each of the buckets in
            // the path. pathcode is sort of like a base-slot_per_bucket number, and
            // we need to hold at most MAX_BFS_PATH_LEN slots. Thus we need the
            // maximum pathcode to be at least slot_per_bucket()^(MAX_BFS_PATH_LEN).
            uint16_t pathcode;
            static_assert(const_pow(slot_per_bucket(), MAX_BFS_PATH_LEN) <
                              std::numeric_limits<decltype(pathcode)>::max(),
                          "pathcode may not be large enough to encode a cuckoo "
                          "path");
            // The 0-indexed position in the cuckoo path this slot occupies. It must
            // be less than MAX_BFS_PATH_LEN, and also able to hold negative values.
            int8_t depth;
            static_assert(MAX_BFS_PATH_LEN - 1 <=
                              std::numeric_limits<decltype(depth)>::max(),
                          "The depth type must able to hold a value of"
                          " MAX_BFS_PATH_LEN - 1");
            static_assert(-1 >= std::numeric_limits<decltype(depth)>::min(),
                          "The depth type must be able to hold a value of -1");
            b_slot() {}
            b_slot(const size_type b, const uint16_t p, const decltype(depth) d)
                : bucket(b), pathcode(p), depth(d)
            {
                assert(d < MAX_BFS_PATH_LEN);
            }
        };

        // b_queue is the queue used to store b_slots for BFS cuckoo hashing.
        class b_queue
        {
        public:
            b_queue() noexcept : first_(0), last_(0) {}

            void enqueue(b_slot x)
            {
                assert(!full());
                slots_[last_++] = x;
            }

            b_slot dequeue()
            {
                assert(!empty());
                assert(first_ < last_);
                b_slot &x = slots_[first_++];
                return x;
            }

            bool empty() const { return first_ == last_; }

            bool full() const { return last_ == MAX_CUCKOO_COUNT; }

        private:
            // The size of the BFS queue. It holds just enough elements to fulfill a
            // MAX_BFS_PATH_LEN search for two starting buckets, with no circular
            // wrapping-around. For one bucket, this is the geometric sum
            // sum_{k=0}^{MAX_BFS_PATH_LEN-1} slot_per_bucket()^k = (1 - slot_per_bucket()^MAX_BFS_PATH_LEN) / (1 - slot_per_bucket())
            //
            // Note that if slot_per_bucket() == 1, then this simply equals MAX_BFS_PATH_LEN.
            static_assert(slot_per_bucket() > 0, "SLOT_PER_BUCKET msut be greater than 0!");
            static constexpr size_type MAX_CUCKOO_COUNT =
                2 * ((slot_per_bucket() == 1)
                         ? MAX_BFS_PATH_LEN
                         : (const_pow(slot_per_bucket(), MAX_BFS_PATH_LEN) - 1) /
                               (slot_per_bucket() - 1));

            // An array of b_slots. Since we allocate just enough space to complete a full search,
            // we should never exceed the end of the array.
            b_slot slots_[MAX_CUCKOO_COUNT];
            // The index of the head of the queue in the array
            size_type first_;
            // One past the index of the last_ item of the queue in the array.
            size_type last_;
        };

        // slot_search searches for a cuckoo path using breadth-first search. It
        // starts with the i1 and i2 buckets, and, until it finds a bucket with an
        // empty slot, adds each slot of the bucket in the b_slot. If the queue runs
        // out of space, it fails.
        //
        // throws hashpower_changed if it changed during the search
        b_slot slot_search(const size_type hp, const size_type i1, const size_type i2)
        {
            b_queue q;
            // The initial pathcode informs cuckoopath_search which bucket the path starts on
            q.enqueue(b_slot(i1, 0, 0));
            q.enqueue(b_slot(i2, 1, 0));
            while (!q.empty())
            {
                b_slot x = q.dequeue();
                bucket &b = buckets_[x.bucket];
                // Picks a (sort-of) random slot to start from
                size_type starting_slot = x.pathcode % slot_per_bucket();
                for (size_type i = 0; i < slot_per_bucket(); ++i)
                {
                    uint16_t slot = (starting_slot + i) % slot_per_bucket();
                    if (!b.occupied(slot))
                    {
                        // We can terminate the search here
                        x.pathcode = x.pathcode * slot_per_bucket() + slot;
                        return x;
                    }

                    // If x has less than the maximum number of path components,
                    // create a new b_slot item, that represents the bucket we could
                    // have to come from if we kicked out the item at this slot.
                    const size_type key = b.key(slot);
                    if (x.depth < MAX_BFS_PATH_LEN - 1)
                    {
                        assert(!q.full());
                        b_slot y(alt_index(hp, key, x.bucket), x.pathcode * slot_per_bucket() + slot, x.depth + 1);
                        q.enqueue(y);
                    }
                }
            }
            // We didn't find a short-enough cuckoo path, so the search terminated :(
            // Return a failure value
            return b_slot(0, 0, -1);
        }

        // Miscellaneous functions

        // reserve_calc takes in a parameter specifying a certain number of slots
        // for a table and returns the smallest hashpower that will hold n elements.
        static size_type
        reserve_calc(const size_type n)
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

        mutable std::vector<uint16_t> seeds_;
        mutable size_t num_lookup_rds_;
    };

}; // namespace cuckoohashtable

#endif // CUCKOO_HASHTABLE_HH