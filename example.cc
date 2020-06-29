#include "cuckoopair.hh"
#include "cuckoofilter/src/cuckoofilter.h"

#include <math.h>
#include <cstdlib>

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
    if(argc <= 1) {
        cout << "enter number of items to insert\n";
        return 1;
    }

    int seed = 1;
    mt19937 rd(seed);

    typedef uint64_t KeyType;
    int size = atoi(argv[1]);             // 240000 for ~91% load factor
    int init_size = size / 0.95; // max load factor of 95%
    cout << "init size: " << init_size << "\n";
    cuckoo_pair<KeyType, 12> cp(size);
    // original cuckoo filter for false positives comparison
    cuckoofilter::CuckooFilter<size_t, 12> cf(init_size);

    // cp.info();

    vector<KeyType> r;
    vector<KeyType> s;

    // 64-bit random numbers to insert and lookup -> lookup_size = insert_size * 100
    random_gen(size, r, rd);
    random_gen(size * 100, s, rd);

    // add set R to filter
    // cout << "R INSERT\n";
    for (auto c : r)
    {
        // cout << c << " ";
        cp.insert(c);
        cf.Add(c);
        // assert(cf.Add(c) == cuckoofilter::Ok);
    }

    // check for false negatives with set R
    
    cout << "R LOOKUP (FN)\n";
    size_t false_negs = 0;
    for (auto c : r)
    {
        if (!cp.lookup(c))
        {
            // cout << "fn tag: " << l << "\n";
            false_negs++;
        }
    }
    cout << "total fn's: " << false_negs << "\n";

    // lookup set S and count false positives

    cout << "S LOOKUP (FP)\n";
    size_t total_queries = 0;
    size_t false_queries = 0;
    size_t false_comp = 0;
    for (size_t i = 0; i < s.size(); i++)
    // for (auto l : s)
    {
        auto l = s.at(i);
        if (cp.lookup(l))
        {
            // cout << "fp tag: " << l << "\n";
            false_queries++;
        }
        if (cf.Contain(l) == cuckoofilter::Ok)
        {
            false_comp++;
        }
        total_queries++;
    }
    // Output the measured false positive rate
    cout << "total fp's: " << false_queries << " out of " << total_queries
         << ", fp rate: " << 100.0 * false_queries / total_queries << "%\n";
    cout << "Original CF: " << false_comp << " out of " << total_queries << ", fp rate: "
         << 100.0 * false_comp / total_queries << "%\n";
    /*
*/


    cp.hashInfo();
    cp.info();
    cout << "orig. CF:\n"
         << cf.Info() << "\n";

    return 0;
}