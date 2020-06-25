#ifndef BUCKET_CONTAINER_H
#define BUCKET_CONTAINER_H

#include <array>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>

namespace cuckoohashtable {
    /**
     * manages storage of keys for the table
     * sized by powers of two
     * 
     * @param Key - type of keys in the table
     * @param SLOT_PER_BUCKET - number of slots for each bucket in the table
     */ 

    template<class Key, class Allocator, std::size_t SLOT_PER_BUCKET>
    class bucket_container {
        public:
            using key_type = const Key;

            using allocator_type = typename traits_::allocator_type;
            using size_type = typename traits_::size_type; // traits????
        
        private:
            using traits_ = typename std::allocator_traits<
                Allocator>::template rebind_traits<key_type>;

            /**
             * bucket type holds SLOT_PER_BUCKET keys, along with occupancy info
             * uses aligned_storage arrays to store keys to allow constructing and destroying keys in place
             */ 
            class bucket {
                public:
                    bucket() noexcept : occupied_() {}

                    const key_type &key(size_type ind) const {
                    return *static_cast<const key_type *>(
                        static_cast<const void *>(&keys_[ind]));
                    }

                    key_type &key(size_type ind) {
                    return *static_cast<key_type *>(static_cast<void *>(&keys_[ind]));
                    }

                    bool occupied(size_type ind) const { return occupied_[ind]; }
                    bool &occupied(size_type ind) { return occupied_[ind]; }
                public:
                    friend class bucket_container;

                    using storage_key_type = Key; // same as key_type

                    const storage_key_type &key(size_type ind) const {
                    return *static_cast<const storage_key_type *>(
                        static_cast<const void *>(&keys_[ind]));
                    }

                    storage_key_type &key(size_type ind) {
                    return *static_cast<storage_key_type *>(static_cast<void *>(&keys_[ind]));
                    }

                    std::array<typename std::aligned_storage<sizeof(storage_key_type),
                                alignof(storage_key_type)>::type, SLOT_PER_BUCKET> keys_;
                    std::array<bool, SLOT_PER_BUCKET> occupied_;
            };

            bucket_container() {

            }
    };
} // namespace cuckoohashtable

#endif // BUCKET_CONTAINER_H