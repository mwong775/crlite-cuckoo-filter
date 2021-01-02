#ifndef BUCKET_CONTAINER_H
#define BUCKET_CONTAINER_H

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>

namespace cuckoohashtable
{
    /**
     * manages storage of keys for the table
     * sized by powers of two
     * 
     * @tparam Key - type of keys in the table
     * @tparam Allocaator - type of key allocator
     * @tparam Partial - type of fingerprint/partial keys
     * @tparam SLOT_PER_BUCKET - number of slots for each bucket in the table
     */

    template <class Key, class Allocator, class Partial, std::size_t SLOT_PER_BUCKET>
    class bucket_container
    {
    public:
        using key_type = Key;

    private:
        using traits_ = typename std::allocator_traits<Allocator>::template rebind_traits<key_type>;

    public:
        using allocator_type = typename traits_::allocator_type;
        using partial_t = Partial;
        using size_type = typename traits_::size_type;
        using reference = key_type &;
        using const_reference = const key_type &;
        using pointer = typename traits_::pointer;
        using const_pointer = typename traits_::const_pointer;

        /**
             * bucket type holds SLOT_PER_BUCKET keys, along with occupancy info
             * uses aligned_storage arrays to store keys to allow constructing and destroying keys in place
             */
        class bucket
        {
        public:
            bucket() noexcept : occupied_() {}

            const key_type &key(size_type ind) const
            {
                return *static_cast<const key_type *>(
                    static_cast<const void *>(&keys_[ind]));
            }

            key_type &key(size_type ind)
            {
                return *static_cast<key_type *>(static_cast<void *>(&keys_[ind]));
            }

            partial_t partial(size_type ind) const { return partials_[ind]; }
            partial_t &partial(size_type ind) { return partials_[ind]; }

            bool occupied(size_type ind) const { return occupied_[ind]; }
            bool &occupied(size_type ind) { return occupied_[ind]; }

        public:
            friend class bucket_container;

            using storage_key_type = Key; // same as key_type

            const storage_key_type &storage_key(size_type ind) const
            {
                return *static_cast<const storage_key_type *>(
                    static_cast<const void *>(&keys_[ind]));
            }

            storage_key_type &storage_key(size_type ind)
            {
                return *static_cast<storage_key_type *>(static_cast<void *>(&keys_[ind]));
            }

            std::array<typename std::aligned_storage<sizeof(storage_key_type),
                                                     alignof(storage_key_type)>::type,
                       SLOT_PER_BUCKET>
                keys_;
            std::array<partial_t, SLOT_PER_BUCKET> partials_;
            std::array<bool, SLOT_PER_BUCKET> occupied_;
        };

        bucket_container(size_type hp, const allocator_type &allocator) : allocator_(allocator), bucket_allocator_(allocator),
                                                                          hashpower_(hp), buckets_(bucket_allocator_.allocate(size()))
        {
            // The bucket default constructor is nothrow, so we don't have to
            // worry about dealing with exceptions when constructing all the
            // elements.
            // static_assert(std::is_nothrow_constructible<bucket>::key, // 'key' is not a member of nothrow thing
            //               "bucket_container requires bucket to be nothrow "
            //               "constructible");
            for (size_type i = 0; i < size(); ++i)
            {
                traits_::construct(allocator_, &buckets_[i]);
            }
        }

        ~bucket_container() noexcept { destroy_buckets(); }

        size_type hashpower() const
        {
            return hashpower_.load(std::memory_order_acquire);
        }

        void hashpower(size_type val)
        {
            hashpower_.store(val, std::memory_order_release);
        }

        size_type size() const { return size_type(1) << hashpower(); }

        allocator_type get_allocator() const { return allocator_; }

        bucket &operator[](size_type i) { return buckets_[i]; }
        const bucket &operator[](size_type i) const { return buckets_[i]; }

        void info() const
        {
            // std::cout << "BucketContainer status:\n"
            //           << "\t\tslots per bucket: " << SLOT_PER_BUCKET << "\n\n";
            if (size() < 100)
                print(); // whole items
            print("fp"); // fingerprints
        }

        void printBucket(const size_t i)
        {
            bucket &b = buckets_[i];
            std::cout << "Bucket " << i << ": [ ";
            for (size_type j = 0; j < SLOT_PER_BUCKET; ++j)
            {
                if (b.occupied(j))
                    std::cout << b.key(j);
                else
                    std::cout << " ";

                if (j < SLOT_PER_BUCKET - 1)
                    std::cout << ", ";
            }
            std::cout << "]\tFP/partials: [ ";
            for (size_type j = 0; j < SLOT_PER_BUCKET; ++j)
            {
                if (b.occupied(j))
                    std::cout << b.partial(j);
                else
                    std::cout << " ";

                if (j < SLOT_PER_BUCKET - 1)
                    std::cout << ", ";
            }
            std::cout << "]\t";
        }

        void print(std::string arg = "") const
        {
            int it = size() > 40 ? 10 : size();
            if (arg == "fp")
            {
                if (it == 10)
                    std::cout << "fp's (first 10):\n";
                else
                    std::cout << "fingerprints:\n";
            }
            else
            {
                if (it == 10)
                    std::cout << "items (first 10):\n";
                else
                    std::cout << "items:\n";
            }
            for (size_type i = 0; i < it; ++i)
            {
                bucket &b = buckets_[i];
                std::cout << i << ": [ ";
                for (size_type j = 0; j < SLOT_PER_BUCKET; ++j)
                {
                    if (b.occupied(j))
                    {
                        if (arg == "fp")
                            std::cout << b.partial(j);
                        else
                            std::cout << b.key(j);
                    }
                    else
                    {
                        std::cout << " ";
                    }
                    if (j < SLOT_PER_BUCKET - 1)
                    {
                        std::cout << ", ";
                    }
                }
                std::cout << "]\n";
            }
        }

        // Constructs live data in a bucket
        template <typename K>                                        // , typename... Args
        void setK(size_type ind, size_type slot, partial_t p, K &&k) // , Args &&... args
        {
            bucket &b = buckets_[ind];
            assert(!b.occupied(slot));
            b.partial(slot) = p;
            traits_::construct(allocator_, std::addressof(b.storage_key(slot)), std::forward<K>(k));
            // This must occur last, to enforce a strong exception guarantee
            b.occupied(slot) = true;
            // std::cout << "finished adding " << k << " to bucket in slot " << slot << " & index " << ind << "\n";
        }

        // Destroys live data in a bucket
        void eraseK(size_type ind, size_type slot)
        {
            bucket &b = buckets_[ind];
            assert(b.occupied(slot));
            b.occupied(slot) = false;
            traits_::destroy(allocator_, std::addressof(b.storage_key(slot)));
        }

        // Adds fingerprint/partial to a bucket
        void setFP(size_type ind, size_type slot, partial_t p)
        {
            bucket &b = buckets_[ind];
            b.partial(slot) = p;
            b.occupied(slot) = true;
            // std::cout << "finished adding rehashed " << p << " to bucket in slot " << slot << " & index " << ind << "\n";
        }

        // Destroys all the live data in the buckets. Does not deallocate the bucket memory.
        void clear() noexcept
        {
            // static_assert(
            //     std::is_nothrow_destructible<key_type>::key,
            //     "bucket_container requires key to be nothrow destructible"); // throwing 'key' is not a member
            for (size_type i = 0; i < size(); ++i)
            {
                bucket &b = buckets_[i];
                for (size_type j = 0; j < SLOT_PER_BUCKET; ++j)
                {
                    if (b.occupied(j))
                    {
                        eraseK(i, j);
                    }
                }
            }
        }

        // Destroys and deallocates all data in the buckets. After this operation,
        // the bucket container will have no allocated data. It is still valid to
        // swap, move or copy assign to this container.
        void clear_and_deallocate() noexcept
        {
            destroy_buckets();
        }

    private:
        using bucket_traits_ = typename traits_::template rebind_traits<bucket>;
        using bucket_pointer = typename bucket_traits_::pointer;

        void destroy_buckets() noexcept
        {
            if (buckets_ == nullptr)
            {
                return;
            }
            // The bucket default constructor is nothrow, so we don't have to
            // worry about dealing with exceptions when constructing all the
            // elements.
            static_assert(std::is_nothrow_destructible<bucket>::value,
                          "bucket_container requires bucket to be nothrow "
                          "destructible");
            clear();
            for (size_type i = 0; i < size(); ++i)
            {
                traits_::destroy(allocator_, &buckets_[i]);
            }
            bucket_allocator_.deallocate(buckets_, size());
            buckets_ = nullptr;
        }

        // This allocator matches the value_type, but is not used to construct
        // storage_value_type pairs, or allocate buckets
        allocator_type allocator_;
        // This allocator is used for actually allocating buckets. It is simply
        // copy-constructed from `allocator_`, and will always be copied whenever
        // allocator_ is copied.
        typename traits_::template rebind_alloc<bucket> bucket_allocator_;

        // This needs to be atomic, since it can be read and written by multiple
        // threads not necessarily synchronized by a lock.
        std::atomic<size_type> hashpower_;
        // These buckets are protected by striped locks (external to the
        // BucketContainer), which must be obtained before accessing a bucket.
        bucket_pointer buckets_;
    };
} // namespace cuckoohashtable

#endif // BUCKET_CONTAINER_H