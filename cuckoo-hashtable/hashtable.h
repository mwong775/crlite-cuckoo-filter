#ifndef CUCKOO_HASHTABLE_HASHTABLE_H_
#define CUCKOO_HASHTABLE_HASHTABLE_H_
#include <vector>
#include <array>

#include <assert.h>
#include <sstream>

#include "cuckoo-config.h";
#include "cuckoohash_util.h";

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
    class Hashtable {
        private:
        using bucket_type = bucket<Key, Allocator, SLOT_PER_BUCKET>;
        static const size_t ItemsPerBucket = 4;
        static const size_t BytesPerBucket = 1234; // How to calculate this based on ItemType??

        struct Bucket {
        char bits_[BytesPerBucket];
    } __attribute__((__packed__));

    // using a pointer adds one more indirection
    Bucket *buckets_;
    size_t num_buckets_;

    public:
        explicit Hashtable(const size_t num): num_buckets_(num) {
            buckets_ = new Bucket[num_buckets_];
            memset(buckets_, 0, BytesPerBucket * num_buckets_);
        }

        ~SingleTable() { 
            delete[] buckets_;
        }

        size_t NumBuckets() const {
            return num_buckets_;
        }

        size_t SizeInBytes() const { 
            return BytesPerBucket * num_buckets_; 
        }

        size_t SizeInItems() const { 
            return ItemsPerBucket * num_buckets_; 
        }

        std::string Info() const {
            std::stringstream ss;
            ss << "SingleHashtable with item size: " << bits_per_item << " bits \n";
            ss << "\t\tAssociativity: " << ItemsPerBucket << "\n";
            ss << "\t\tTotal # of rows: " << num_buckets_ << "\n";
            ss << "\t\tTotal # slots: " << SizeInItems() << "\n";
            return ss.str();
        }

        inline void ReadItemsInBUckets(size_t i, const ItemType& items[4]) const {
            for(size_t j = 0; j < ItemsPerBucket; j++) {
                items[j] = ReadItem(i, j);
            }
        }

        inline ItemType& ReadItem(const size_t i, const size_t j) const {
            const char *p = buckets_[i].bits_;
            ItemType& item;
        }

        inline void WriteItem(const size_t i, const size_t j, const ItemType& item) {
            char *p = buckets_[i].bits_;

        }

        inline 
    }
}


#endif