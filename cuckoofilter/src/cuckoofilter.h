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
};

// maximum number of cuckoo kicks before claiming failure
const size_t kMaxCuckooCount = 500;

// max number of rehashes for a given bucket before claiming failure
const size_t maxRehashCount = 500;

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

  unordered_map<size_t, vector<ItemType>> r_items;
  unordered_map<size_t, vector<ItemType>> s_items;
  unordered_map<size_t, set<uint32_t>> r_tagmap;
  unordered_map<size_t, set<uint32_t>> s_tagmap;

  size_t *indices;

  void IndicesInfo() const {
    size_t maxRehashCount = 0;
    size_t totalRehashCount = 0;
    // cout << "resulting indices:\n";
    for(uint32_t i = 0; i < table_->NumBuckets(); i++) {
      if(indices[i] > maxRehashCount)
        maxRehashCount = indices[i];
      if(indices[i] > 0)
        totalRehashCount++;
      //   cout << indices[i] << " ";
    }
    cout << "\nmax rehash count: " << maxRehashCount << "\ntotal rehash count: " << totalRehashCount << "\n";
  }

  template<typename key, typename value>
  void PrintMap(unordered_map<key, value> &map) const {
    cout << "\nprinting map...\n{\n";
    for(auto const& pair : map) { // each bucket
      cout << pair.first << " : [ "; 
      for(auto const& i : pair.second) {
        cout << i << " ";
      }
      cout << "]\n";
    }
    cout << "}\n\n";
  }

  void PrintBucket(set<uint32_t> &bucket, size_t i) const { // TODO: reuse this in PrintMap
    cout << "\nbucket " << i << " (" << bucket.size() << "):\n";
    for(auto t : bucket)
      cout << t << " ";
    cout << "\n";
  }

// returns true on first match/collision, otherwise  returns false
template <class R, class S>
bool TagCollision(R Rfirst, R Rlast, S Sfirst, S Slast) {
  while(Rfirst != Rlast && Sfirst != Slast) {
    if(*Rfirst < *Sfirst)
      ++Rfirst;
    else if(*Sfirst < *Rfirst)
      ++Sfirst;
    else {
      cout << "collision on tag " << *Rfirst << "\n";
      return true;
    }
  }
  return false;
}

  inline size_t GenerateIndexHash(const ItemType& item) const {
    uint64_t hash = hasher_(item);
    return IndexHash(hash >> 32);
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
                                   uint32_t* tag, size_t i = 0) const { // i = index thing to concat for rehash
    uint64_t hash;
    if(i) {
      // string s1 = to_string(item);
      // string s2 = to_string(i);
      // cout << "concat " << item << " & " << i << '\n';
      // string s = s1 + s2;

      // cout << "item: " << item << " concat: " << item * 10 + i << "\n";
      size_t c = item * 10 + i;
      hash = hasher_(c); // stoi not large enough
    } else {
      hash = hasher_(item);
      *index = IndexHash(hash >> 32);
    }
    // cout << "hash: " << hash << "\n";
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
    // for(uint8_t i = 0; i < num_buckets; i++) {
    //   cout << indices[i] << "\n";
    // }
    table_ = new TableType<bits_per_item>(num_buckets);
  }

  ~CuckooFilter() { delete table_; delete[] indices;}

  // Try hashing item for filter, returns i (index of bucket) and tag (fingerprint)
  Status HashAll(vector<ItemType> &r, vector<ItemType> &s);

  // Compares tags at each bucket between r and s, rehash the tag exists in both r and s at the same bucket
  Status RehashCheck(); // unordered_map<size_t, vector<uint32_t>> &r_tagmap, unordered_map<size_t, vector<uint32_t>> &s_tagmap);

  // Rehashes tags at a given bucket index
  Status RehashBucket(size_t i);

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
Status CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::HashAll( // Modifying Add to return i and tag without physically inserting to filter
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

        r_tagmap[r_index].insert(r_tag);
        auto r_pr = r_items.insert(make_pair(r_index, vector<uint64_t>(1, r_item)));
        if(!r_pr.second) {
          r_pr.first->second.push_back(r_item);
        }

        s_tagmap[s_index].insert(s_tag);
        auto s_pr = s_items.insert(make_pair(s_index, vector<uint64_t>(1, s_item)));
        if(!s_pr.second) {
          s_pr.first->second.push_back(s_item);
        }
        it++;
      }

      while(it < s.size()) { // TODO: improve scrappy implementation based on assumption that r.size() < s.size(). 
        size_t i;
        uint32_t tag;
        const ItemType &item = s.at(it);
        GenerateIndexTagHash(item, &i, &tag);
        // cout << "hashing rest of s: i = " << i << " tag = " << tag << "\n";
        s_tagmap[i].insert(tag);
        auto pr = s_items.insert(make_pair(i, vector<uint64_t>(1, item)));
        if(!pr.second) {
          pr.first->second.push_back(item);
        }
        it++;
      }

      return RehashCheck();
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
Status CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::RehashCheck() {
    cout << "\nComparing tags for potential rehashing..." << "\n";
    for(uint i = 0; i < table_->NumBuckets(); i++) {
      // if no tags in a bucket, skip
      if(r_tagmap.find(i) == r_tagmap.end())
        continue;
      else if(s_tagmap.find(i) == s_tagmap.end())
        continue;
      // buckets exist, compare tags
      else {
        // cout << "bucket " << i << " exists for r and s" << "\n";
        set<uint32_t> &r_tags = r_tagmap[i];
        set<uint32_t> &s_tags = s_tagmap[i];

        // rehash bucket until no matching tags between r and s
        while(TagCollision(r_tags.begin(), r_tags.end(), s_tags.begin(), s_tags.end())) {
          cout << "beep beep collision at bucket " << i << "\n";
          // PrintBucket(r_tagmap[i], i);
          // PrintBucket(s_tagmap[i], i);
          if(RehashBucket(i) == NotSupported)
            return NotSupported;
        }
        // implement add here
        for(auto c : r_tagmap[i]) {
          if(AddImpl(i, c) == Ok) {
            // count to verify idk
          }
        }
      }
    }
    // cout << "final tagmaps:\n";
    // PrintMap(r_tagmap);
    // PrintMap(s_tagmap);
    IndicesInfo();
    return Ok;
}

template <typename ItemType, size_t bits_per_item,
          template <size_t> class TableType, typename HashFamily>
Status CuckooFilter<ItemType, bits_per_item, TableType, HashFamily>::RehashBucket(size_t i) {
  // empty to rehash
  r_tagmap[i].clear();
  s_tagmap[i].clear();

  if(indices[i] > maxRehashCount) {
    cout << "rehash fail on bucket " << i << "\n";
    IndicesInfo();
    return NotSupported; // temp. constraint - max number of rehashes for a given bucket before claiming failure
  }
  indices[i]++;
  cout << "inc. to " << indices[i] << "\n";
  vector<ItemType> &r = r_items[i];
  vector<ItemType> &s = s_items[i];
  size_t it = 0;
  while(it < r.size()) {
    uint32_t r_tag;
    uint32_t s_tag;
    GenerateIndexTagHash(r.at(it), &i, &r_tag, indices[i]);
    GenerateIndexTagHash(s.at(it), &i, &s_tag, indices[i]);
    r_tagmap[i].insert(r_tag);
    s_tagmap[i].insert(s_tag);
    it++;
  }

  while(it < s.size()) {
    uint32_t tag;
    GenerateIndexTagHash(s.at(it), &i, &tag, indices[i]);
    s_tagmap[i].insert(tag);
    it++;
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

  i1 = GenerateIndexHash(key);
  GenerateIndexTagHash(key, &i1, &tag, indices[i1]);
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
