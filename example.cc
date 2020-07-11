#include "cuckoofilter/src/cuckoofilter.h"

#include <math.h>
#include <cstdlib>

#include "cuckoohashtable/city_hasher.hh"
#include "cuckoohashtable/hashtable/cuckoohashtable.hh"

using namespace std;

// generate n 64-bit random numbers for running insert & lookup
void random_gen(int n, vector<uint64_t> &store, mt19937 &rd)
{
    store.resize(n);
    for (int i = 0; i < n; i++)
        store[i] = (uint64_t(rd()) << 32) + rd();
}

int main(int argc, char **argv)
{
    if (argc <= 1)
    {
        cout << "enter number of items to insert\n";
        return 1;
    }

    int seed = 1;
    mt19937 rd(seed);

    typedef uint64_t KeyType;
    int size = atoi(argv[1]);    // 240000 for ~91% load factor
    int init_size = size / 0.95; // max load factor of 95%
    cout << "init size: " << init_size << "\n";

    cuckoohashtable::cuckoo_hashtable<KeyType, 12, CityHasher<int>> table(init_size);

    vector<KeyType> r;
    vector<KeyType> s;

    vector<KeyType> fp;

    // 64-bit random numbers to insert and lookup -> lookup_size = insert_size * 100
    random_gen(size, r, rd);
    random_gen(size * 100, s, rd);

    // CityHasher<int> ch;

    // add set R to filter
    // cout << "R INSERT\n";
    // for (size_t i = 0; i < r.size(); i++)
    for (auto c : r)
    {
        // auto c = r.at(i);
        // cout << c << " , cityhash: " << ch.operator()(c, 0);
        table.insert(c);
    }

    // check for false negatives with set R

    // cout << "R LOOKUP (FN)\n";
    size_t false_negs = 0;
    for (auto c : r)
    {
        if (!table.lookup(c))
        {
            // cout << "fn tag: " << l << "\n";
            false_negs++;
        }
    }
    // cout << "total false negatives: " << false_negs << "\n";
    assert(false_negs == 0);

    // lookup set S and count false positives

    // cout << "S LOOKUP (FP)\n";
    size_t total_queries = 0;
    size_t false_queries = size;
    size_t definite_queries = 0;
    // size_t false_comp = 0;

    while (false_queries)
    {
        false_queries = 0;
        total_queries = 0;
        definite_queries = 0;
        fp.clear();
        table.start_lookup();
        // table.bucketInfo();
        for (size_t i = 0; i < s.size(); i++)
        // for (auto l : s)
        {
            auto l = s.at(i);
            int fingerprint = table.lookup(l);
            if (fingerprint)
            {
                // cout << "fp lup: " << l << "\n";
                fp.push_back(fingerprint);
                false_queries++;
            }
            if (table.find(l))
            {
                definite_queries++;
            }
            // if (cf.Contain(l) == cuckoofilter::Ok)
            // {
            //     false_comp++;
            // }
            total_queries++;
        }

        // Output the measured false positive rate
        // cout << "fp's: [";
        // for (auto f : fp)
        //     cout << f << " ";
        // cout << "]";

        cout << "total false positives: " << false_queries << " out of " << total_queries
             << ", fp rate: " << 100.0 * false_queries / total_queries << "%\n";
        assert(definite_queries / total_queries == 0);

        // cout << "Original CF: " << false_comp << " out of " << total_queries << ", fp rate: "
        //      << 100.0 * false_comp / total_queries << "%\n";

        if (false_queries > 0)
        {
            table.rehash_buckets();
        }
    }

    table.info();
    // table.bucketInfo();
    table.seedInfo();
    /*    

*/
    return 0;
}