#include "cuckoofilter/src/cuckoofilter.h"
#include "cuckoohashtable/hashtable/cuckoohashtable.hh"
#include <math.h>

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

    int seed = 1;
    mt19937 rd(seed);

    int size = 240;               // 240000 for ~91% load factor
    int init_size = size / 0.95; // max load factor of 95%
    cout << "init size: " << init_size << "\n";
    typedef uint32_t KeyType;
    cuckoohashtable::cuckoo_hashtable<KeyType> Table(init_size);
    // should pass table init configs to filter: rows/bucket count & capacity
    cuckoofilter::CuckooFilter<size_t, 12> cf(init_size, Table.bucket_count());

    vector<uint64_t> r;
    vector<uint64_t> s;

    // 64-bit random numbers to insert and lookup -> lookup_size = insert_size * 100
    random_gen(size, r, rd);
    random_gen(size * 100, s, rd);

    // add set R to filter
    for (auto c : r)
    {
        pair<size_t, size_t> location = Table.insert(c);

        assert(cf.Add(c) == cuckoofilter::Ok);
        assert(cf.Contain(c) == cuckoofilter::Ok);
        // if(cf.Contain(c) != cuckoofilter::Ok) {
        //     cout << "ERROR\n";
        //     return 1;
        // }
    }

        // lookup set S and count false positives
    cout << "s lookup\n";
    size_t total_queries = 0;
    size_t false_queries = 0;
    for (size_t i = 0; i < s.size(); i++)
    {
        if (cf.Contain(s.at(i)) != cuckoofilter::NotFound)
        {
            false_queries++;
        }
        total_queries++;
    }
    // Output the measured false positive rate
    cout << "total fp's: " << false_queries << " out of " << total_queries << "\n";
    cout << "false positive rate is " << 100.0 * false_queries / total_queries << "%\n";


    Table.info();
    cout << cf.Info() << "\n";

    return 0;
}