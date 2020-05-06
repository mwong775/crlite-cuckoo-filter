#ifndef CUCKOO_FILTER_CUCKOO_FILTER_H_
#define CUCKOO_FILTER_CUCKOO_FILTER_H_

#include <assert.h>
#include <algorithm>
#include <unordered_map>

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
};

// maximum number of cuckoo kicks before claiming failure
const size_t kMaxCuckooCount = 500;

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

  unordered_map<size_t, pair<vector<uint64_t>, vector<uint32_t>>> r_itemtagmap; // pair<item, tag>
  unordered_map<size_t, pair<vector<uint64_t>, vector<uint32_t>>> s_itemtagmap;
  // unordered_map<size_t, vector<uint32_t>> r_tagmap;
  // unordered_map<size_t, vector<uint32_t>> s_tagmap;
  size_t *indices;

  template<typename key, typename value>
  void PrintMap(unordered_map<key, value> &map) const {
    cout << "\nprinting map...\n{\n";
    for(auto const& pair : map) {
      cout << pair.first << " : [ "; 
      for(uint32_t const& i : pair.second) {
        cout << i << " ";
      }
      cout << "]\n";
    }
    cout << "}\n\n";
  }

template <class R, class S>
bool CollisionCheck(R Rfirst, R Rlast, S Sfirst, S Slast) {
  while(Rfirst != Rlast && Sfirst != Slast) {
    if(*Rfirst < *Sfirst)
      ++Rfirst;
    else if(*Sfirst < *Rfirst)
      ++Sfirst;
    else
      return true;
  }
  return false;
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

  inline void GenerateIndexTagHash(const ItemType& item, size_t* index,
                                   uint32_t* tag) const {
    const uint64_t hash = hasher_(item);
    // cout << "hash: " << hash << "\n";
    *index = IndexHash(hash >> 32);
    *tag = TagHash(hash);
  }

  inline size_t AltIndex(const size_t index, const uint32_t tag) const {
    // NOTE(binfan): originally we use:
    // index ^ HashUtil::BobHash((const void*) (&tag), 4)) & table_->INDEXMASK;
    // now doing a quick-n-dirty way:
    // 0x5bd1e995 is the hash constant from MurmurHash2
    return IndexHash((uint32_t)(index ^ (tag * 0x5bd1e995)));
  }

  Status AddImpl(const size_t i, const uint32_t tag);

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
    // for(int i = 0; i < sizeof(indices); i++) {
    //   cout << indices[i] << "\n";
    // }
    // cout << "indices array size: " << (sizeof(indices)/sizeof(*indices)) << "\n";
    table_ = new TableType<bits_per_item>(num_buckets);
  }

  ~CuckooFilter() { delete table_; delete[] indices;}

  // Try hashing item for filter, returns i (index of bucket) and tag (fingerprint)
  Status TryHash(vector<ItemType> &r, vector<ItemType> &s);

  // Compares tags at each bucket between r and s, rehash the tag exists in both r and s at the same bucket
  Status RehashCheck(); // unordered_map<size_t, vector<uint32_t>> &r_tagmap, unordered_map<size_t, vector<uint32_t>> &s_tagmap);

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
Status CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::TryHash( // Modifying Add to return i and tag without physically inserting to filter
    vector<ItemType> &r, vector<ItemType> &s) {
      size_t it = 0;

      while(it < r.size()) { // iterates thru both vectors, hashing items to get indices and fp's to store.
        size_t r_index;
        size_t s_index;
        uint32_t r_tag;
        uint32_t s_tag;
        // get item from both r and s
        const ItemType &r_item = r.at(it);
        const ItemType &s_item = s.at(it);
        
        // hash both items to get index and tag
        GenerateIndexTagHash(r_item, &r_index, &r_tag);
        GenerateIndexTagHash(s_item, &s_index, &s_tag);

        // cout << "r_index = " << r_index << " r_tag = " << r_tag << "\n";
        // cout << "s_index = " << s_index << " s_tag = " << s_tag << "\n";

        // unordered_map<size_t, vector<pair<uint64_t, uint32_t>>> r_itemtagmap; // pair<item, tag>

        // insert into maps -> i : vector<pair<item, tag>>
        auto pr1 = r_itemtagmap.insert(make_pair(r_index, vector<pair<uint64_t, uint32_t>>(1, make_pair(r_item, r_tag))));
        if(!pr1.second) {
          pr1.first->second.push_back(make_pair(r_item, r_tag));
        }
        // cout << "r vector size? " << pr1.first->second.size() << "\n"; // output is tags associated with the bucket (vector size for specific key)

        auto pr2 = s_itemtagmap.insert(make_pair(s_index, vector<pair<uint64_t, uint32_t>>(1, make_pair(s_item, s_tag))));
        if(!pr2.second) {
          pr2.first->second.push_back(make_pair(s_item, s_tag));
        }
        // cout << "s vector size? " << pr2.first->second.size() << "\n\n";
        it++;
      }
      while(it < s.size()) { // TODO: improve scrappy implementation based on assumption that r.size() < s.size(). 
        size_t i;
        uint32_t tag;
        const ItemType &item = s.at(it);
        GenerateIndexTagHash(item, &i, &tag);
        // cout << "hashing rest of s: i = " << i << " tag = " << tag << "\n";
        auto pr = s_itemtagmap.insert(make_pair(i, vector<pair<uint64_t, uint32_t>>(1, make_pair(item, tag))));
        if(!pr.second) {
          pr.first->second.push_back(make_pair(item, tag));
        }
        it++;
      }
      // TODO: update PrintMap() for upgraded itemtagmap DS
      // TODO: also consider unordered_map<index, array[vector<items>, vector<tags>]??
      // PrintMap(r_itemtagmap);
      // PrintMap(s_itemtagmap);
      RehashCheck();
  return Ok;
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
Status CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::RehashCheck() {
  // unordered_map<size_t, vector<uint32_t>> &r_tagmap, unordered_map<size_t, vector<uint32_t>> &s_tagmap) {
    cout << "\nComparing tags for potential rehashing..." << "\n";
    // cout << "num buckets: " << table_->NumBuckets() << "\n";
    for(int i = 0; i < int(table_->NumBuckets()); i++) {
      // if no tags in a bucket, skip
      if(r_itemtagmap.find(i) == r_itemtagmap.end())
        continue;
      else if(s_itemtagmap.find(i) == s_itemtagmap.end())
        continue;
      // buckets exist, compare tags
      else {
        // TODO: reusable function to print each bucket

        // cout << "bucket " << i << " exists for r and s" << "\n";
        vector<pair<uint64_t, uint32_t>> &r_pairs = r_itemtagmap.find(i)->second;
        cout << "\nbucket " << i << " in r (" << r_pairs.size() << "):\n";
        for(auto r : r_pairs)
          cout << r.first << " : " << r.second << " | ";

        vector<pair<uint64_t, uint32_t>> &s_pairs = s_itemtagmap.find(i)->second;
        cout << "\nbucket " << i << " in s (" << s_pairs.size() << "):\n";
        for(auto s : s_pairs)
          cout << s.first << " : " << s.second << " | ";
        cout << "\n";
        if(CollisionCheck(r_pairs.begin(), r_pairs.end(), s_pairs.begin(), s_pairs.end())) {
          cout << "beep beep collision" << "\n";
        }
        /*
        vector<uint32_t> match(r_pairs.size() + s_pairs.size());
        vector<uint32_t>::iterator it, st;
        it = set_intersection(r_pairs.begin(), r_pairs.end(), s_pairs.begin(), s_pairs.end(), match.begin());
        cout << "\nCommon elements:\n"; 
        for (st = match.begin(); st != it; ++st) 
            cout << *st << ", "; 
        cout << "\n";
        */
      }
    }
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
  cout << "in Add: i = " << i << " tag = " << tag << "\n"; // i = index of chosen bucket, tag = fingerprint
  return AddImpl(i, tag);
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
Status CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::AddImpl(
    const size_t i, const uint32_t tag) {
  size_t curindex = i;
  uint32_t curtag = tag;
  uint32_t oldtag;

  for (uint32_t count = 0; count < kMaxCuckooCount; count++) {
    bool kickout = count > 0;
    oldtag = 0;
    if (table_->InsertTagToBucket(curindex, curtag, kickout, oldtag)) {
      num_items_++;
      return Ok;
    }
    if (kickout) {
      curtag = oldtag;
    }
    curindex = AltIndex(curindex, curtag);
  }

  victim_.index = curindex;
  victim_.tag = curtag;
  victim_.used = true;
  return Ok;
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
Status CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::Contain(
    const ItemType &key) const {
  bool found = false;
  size_t i1, i2;
  uint32_t tag;

  GenerateIndexTagHash(key, &i1, &tag);
  i2 = AltIndex(i1, tag);

  assert(i1 == AltIndex(i2, tag));

  found = victim_.used && (tag == victim_.tag) &&
          (i1 == victim_.index || i2 == victim_.index);

  if (found || table_->FindTagInBuckets(i1, i2, tag)) {
    return Ok;
  } else {
    return NotFound;
  }
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
