#ifndef CUCKOO_FILTER_CUCKOO_FILTER_H_
#define CUCKOO_FILTER_CUCKOO_FILTER_H_

#include <assert.h>
#include <algorithm>
#include <unordered_map>
#include <set>

#include "debug.h"
#include "hashutil.h"
#include "packedtable.h"
#include "printutil.h"
#include "singletable.h"
using namespace std;
namespace cuckoofilter {
// status returned by a cuckoo filter operation
enum Status {
  Ok = 0,
  NotFound = 1,
  NotEnoughSpace = 2,
  NotSupported = 3,
  InPrimary = 4,
  InAlternate = 5
};

// maximum number of cuckoo kicks before claiming failure
const size_t kMaxCuckooCount = 500;

// max number of rehashes for a given bucket before claiming failure
const size_t maxRehashCount = 200;

// A cuckoo filter class exposes a Bloomier filter interface,
// providing methods of Add, Delete, Contain. It takes three
// template parameters:
//   ItemType:  the type of item you want to insert
//   bits_per_item: how many bits each item is hashed into
//   TableType: the storage of table, SingleTable by default, and
// PackedTable to enable semi-sorting
template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType = SingleTable,
          typename HashFamily = TwoIndependentMultiplyShift>
class CuckooFilter {
  // Storage of items
  TableType<bits_per_item> *table_;

  // Number of items stored
  size_t num_items_;

  typedef struct {
    size_t index;
    uint32_t tag;
    bool used;
  } VictimCache;

  VictimCache victim_;

  HashFamily hasher_;
  

  size_t *indices;
  size_t maxRehashBucket = 0;
  size_t rehashTagCount = 0;
  size_t totalRehashCount = 0;
  size_t rehashBucketCount = 0;

  void IndicesInfo() const {
    cout << "resulting indices:\n";
    for(uint32_t i = 0; i < table_->NumBuckets(); i++) {
        // if(indices[i] > 0)
          cout << indices[i] << " ";
    }
    cout << "\nmax rehash count: " << maxRehashBucket << "\nrehash bucket count: " << rehashBucketCount << "\nrehash tag count: " << rehashTagCount << "\ntotal rehash count: " << totalRehashCount << "\n";
  }

  inline size_t IndexHash(uint32_t hv) const {
    // table_->num_buckets is always a power of two, so modulo can be replaced
    // with
    // bitwise-and:
    return hv & (table_->NumBuckets() - 1);
  }

  inline uint32_t TagHash(uint32_t hv) const {
    uint32_t tag;
    tag = hv & ((1ULL << bits_per_item) - 1);
    tag += (tag == 0);
    return tag;
  }

  inline void concatHash(const size_t i, uint32_t* tag, uint32_t* newtag) const {
    size_t concat = *tag * 10 + indices[i];
    uint64_t hash = hasher_(concat);
    *newtag = TagHash(hash);
    // cout << "concat: " << concat << " newtag: " << *newtag << "\n";
  }

  inline void GenerateIndexTagHash(const ItemType& item, size_t* index,
                                   uint32_t* tag, const size_t i = 0) const { // const i = 0 (i = index thing to concat for rehash)
    // uint64_t hash;
    uint64_t hash = hasher_(item);
    // if(i > 0) {
      // string s1 = to_string(item);
      // string s2 = to_string(i);
      // cout << "concat " << item << " & " << i << '\n';
      // string s = s1 + s2;

      // cout << "item: " << item << " concat: " << item * 10 + i << "\n";
    //   size_t c = item * 10 + i;
    //   hash = hasher_(c);
    // } else {
    //   hash = hasher_(item);
      *index = IndexHash(hash >> 32);
    // }
    // cout << "hash: " << hash << "\n";
    *tag = TagHash(hash);
    // if(i > 0) {
    //   uint32_t oldtag = tag;
    //   concatHash(i, oldtag, tag);
    // }
  }

  inline size_t AltIndex(const size_t index, const uint32_t tag) const {
    // NOTE(binfan): originally we use:
    // index ^ HashUtil::BobHash((const void*) (&tag), 4)) & table_->INDEXMASK;
    // now doing a quick-n-dirty way:
    // 0x5bd1e995 is the hash constant from MurmurHash2
    return IndexHash((uint32_t)(index ^ (tag * 0x5bd1e995)));
  }

  Status AddImpl(const size_t i, const uint32_t tag);

  int32_t FindTag(const ItemType &key, const bool rehashing) const;

  // load factor is the fraction of occupancy
  double LoadFactor() const { return 1.0 * Size() / table_->SizeInTags(); }

  double BitsPerItem() const { return 8.0 * table_->SizeInBytes() / Size(); }

 public:
  explicit CuckooFilter(const size_t max_num_keys) : num_items_(0), victim_(), hasher_(), indices() {
    size_t assoc = 4;
    size_t num_buckets = upperpower2(max<uint64_t>(1, max_num_keys / assoc));
    double frac = (double)max_num_keys / num_buckets / assoc;
    if (frac > 0.96) {
      num_buckets <<= 1;
    }
    // cout << "num buckets: " << num_buckets << "\n";
    victim_.used = false;
    indices = new size_t[num_buckets](); // () initializes all values to 0
    // for(uint8_t i = 0; i < num_buckets; i++) {
    //   cout << indices[i] << "\n";
    // }
    table_ = new TableType<bits_per_item>(num_buckets);
  }

  ~CuckooFilter() { delete table_; delete[] indices;}

  // Try hashing item for filter, returns i (index of bucket) and tag (fingerprint)
  Status AddAll(vector<ItemType> &r);

  // Compares tags at each bucket between r and s, rehash the tag exists in both r and s at the same bucket
  Status RehashCheck(vector<ItemType> &s); // unordered_map<size_t, vector<uint32_t>> &r_tagmap, unordered_map<size_t, vector<uint32_t>> &s_tagmap);

  // Rehashes tags at a given bucket index
  Status RehashBucket(size_t i, uint32_t tags[4]);

  // Add an item to the filter.
  Status Add(const ItemType &item);

  // Report if the item is inserted, with false positive rate.
  Status Contain(const ItemType &item) const;

  // Delete an key from the filter
  Status Delete(const ItemType &item);

  /* methods for providing stats  */
  // summary infomation
  string Info() const;

  // number of current inserted items;
  size_t Size() const { return num_items_; }

  // size of the filter in bytes.
  size_t SizeInBytes() const { return table_->SizeInBytes(); }
};

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
Status CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::AddAll( // Modifying Add to return i and tag without physically inserting to filter
    vector<ItemType> &r) {
    for(auto c : r) {
        assert(Add(c) == cuckoofilter::Ok);
        // Add(c);
        // assert(Contain(c) == Ok);
    }
  return Ok;
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
Status CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::RehashCheck(vector<ItemType> &s) {
    cout << "\nLookups for potential rehashing..." << "\n";
    for(auto l : s) {
        int32_t i = FindTag(l, true); // rehashing - don't apply concathash to item lookup
        if(i == -1)
          continue;
        else { // fp - rehash bucket
          cout << "beep beep collision at bucket " << i << "\n";
          if(indices[i] == 0)
            rehashBucketCount++;
          size_t index = i;
          uint32_t rehashTags[4] = {0};
          while (i >= 0) {
              table_->PrintBucket(i);
              totalRehashCount++;
              table_->ReadTagsInBuckets(i, rehashTags);
              if(RehashBucket(i, rehashTags) != Ok)
                return NotEnoughSpace;
              i = FindTag(l, true); // 2nd optional arg = lookup without rehashing
          }
          if(indices[index] > maxRehashBucket)
            maxRehashBucket = indices[index];
          cout << "rehashed bucket " << index << " " << indices[index] << " time(s)\n";
        }
    }
    table_->PrintTable();
    IndicesInfo();
    return Ok;
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
Status CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::RehashBucket(size_t i, uint32_t tags[4]) {
  if(indices[i] > maxRehashCount) {
    cout << "ERROR: rehash fail on bucket " << i << "\n";
    IndicesInfo();
    return NotEnoughSpace; // temp. constraint - max number of rehashes for a given bucket before claiming failure
  }
  indices[i]++;
  table_->ClearTagsFromBucket(i);
  // cout << "cleared bucket\n";
  for(uint8_t j = 0; j < 4; j++) {
      if(tags[j]) {
        // cout << tags[j] << " ";
        uint32_t oldtag = tags[j]; // temp fix for argument req. for inserting tag
        uint32_t newtag;
        concatHash(i, &oldtag, &newtag);
        if(!table_->InsertTagToBucket(i, newtag, oldtag)) {
          cout << "Error on rehashing: cannot add newtag" << newtag << "\n";
          return NotEnoughSpace;
        }
        rehashTagCount++;
      }
  }
  // cout << "\nfinish rehash insert:\n";
  // table_->PrintBucket(i);

  // should print original hashed tags
  // for(uint8_t k = 0; k < 4; k++) {
  //   cout << tags[k] << " ";
  // }
  // cout << "\n";
  return Ok;
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
Status CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::Add(
    const ItemType &item) {
  size_t i;
  uint32_t tag;

  if (victim_.used) {
    return NotEnoughSpace;
  }
  GenerateIndexTagHash(item, &i, &tag);
  // cout << "in Add: i = " << i << " tag = " << tag << "\n"; // i = index of chosen bucket, tag = fingerprint
  return AddImpl(i, tag);
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
Status CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::AddImpl(
    const size_t i, const uint32_t tag) {
  size_t curindex = i;
  uint32_t curtag = tag;
  uint32_t oldtag;
  int kicks = 0;

  for (uint32_t count = 0; count < kMaxCuckooCount; count++) {
    bool kickout = count > 0; // does not imply a kick - simply gives permission to kick if no space
    oldtag = 0;
    if (table_->InsertTagToBucket(curindex, curtag, oldtag, kickout)) { // returns true if space to insert, false if kicked out a tag
      num_items_++;
      // if(kicks)
      //   cout << "kicks: " << kicks << "\n";
      // if(count == 0)
      //   cout << "primary location" << "\n";
      return Ok;
    }
    if (kickout) {
      kicks++;
      // cout << "kicking tag " << oldtag << "\n"; 
      curtag = oldtag;
    }
    // cout << "alt index" << "\n";
    curindex = AltIndex(curindex, curtag);
  }

  victim_.index = curindex;
  victim_.tag = curtag;
  victim_.used = true;
  // cout << "finish addImpl, num items " << num_items_ << "\n";
  return Ok;
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
Status CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::Contain(
    const ItemType &key) const {
  int32_t i = FindTag(key, false); // not rehashing
  if (i >= 0) {
    return Ok;
  } else {
    return NotFound;
  }
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
int32_t CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::FindTag( // returns bucket index of where tag is found
    const ItemType &key, const bool rehashing) const {
    size_t i1, i2;
    uint32_t tag;
    // uint32_t rehashtag;

    GenerateIndexTagHash(key, &i1, &tag);
    i2 = AltIndex(i1, tag);

    assert(i1 == AltIndex(i2, tag));

    if(victim_.used && (tag == victim_.tag)) {
      if (i1 == victim_.index) return i1;
      if (i2 == victim_.index) return i2;
    }

    // cout << "rehash indices " << indices[i1] << " and " << indices[i2] << "\n";

    if(!rehashing && indices[i1] > 0) {
      concatHash(i1, &tag, &tag);
      // cout << "check rehashed primary " << i1 << ": tag " << tag << " to " << rehashtag << "\n";
    }
    int32_t found = table_->FindTagInBucket(i1, tag);
    if(found >= 0 && (unsigned)found == i1) {
      cout << "tag " << tag << " in primary " << found << "\n";
      return i1;
    }

    if(!rehashing && indices[i2] > 0) {
      // cout << "check rehashed secondary " << i2 << ": tag " << tag << " to " << rehashtag << "\n";
      concatHash(i2, &tag, &tag);
    }
    found = table_->FindTagInBucket(i2, tag);
    if(found >= 0 && (unsigned)found == i2) {
      cout << "tag " << tag << " in alt. " << found << "\n";
      return i2;
    }

    // int32_t found = table_->FindTagInBuckets(i1, i2, tag);
    // if(found >= 0 && (unsigned)found == i2)
    //   cout << "found at primary " << found << "\n";
    // else if(found >= 0 && (unsigned)found == i2)
    //   cout << "found at alternate " << found << "\n";
    // cout << "not found: " << found << "\n";
    return found;      
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
Status CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::Delete(
    const ItemType &key) {
  size_t i1, i2;
  uint32_t tag;

  GenerateIndexTagHash(key, &i1, &tag);
  i2 = AltIndex(i1, tag);

  if (table_->DeleteTagFromBucket(i1, tag)) {
    num_items_--;
    goto TryEliminateVictim;
  } else if (table_->DeleteTagFromBucket(i2, tag)) {
    num_items_--;
    goto TryEliminateVictim;
  } else if (victim_.used && tag == victim_.tag &&
             (i1 == victim_.index || i2 == victim_.index)) {
    // num_items_--;
    victim_.used = false;
    return Ok;
  } else {
    return NotFound;
  }
TryEliminateVictim:
  if (victim_.used) {
    victim_.used = false;
    size_t i = victim_.index;
    uint32_t tag = victim_.tag;
    AddImpl(i, tag);
  }
  return Ok;
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
string CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::Info() const {
  stringstream ss;
  ss << "CuckooFilter Status:\n"
     << "\t\t" << table_->Info() << "\n"
     << "\t\tKeys stored: " << Size() << "\n"
     << "\t\tLoad factor: " << LoadFactor() << "\n"
     << "\t\tHashtable size: " << (table_->SizeInBytes() >> 10) << " KB\n";
  if (Size() > 0) {
    ss << "\t\tbit/key:   " << BitsPerItem() << "\n";
  } else {
    ss << "\t\tbit/key:   N/A\n";
  }
  return ss.str();
}
}  // namespace cuckoofilter
#endif  // CUCKOO_FILTER_CUCKOO_FILTER_H_
