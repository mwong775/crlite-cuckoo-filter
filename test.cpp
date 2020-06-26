#include "cuckoofilter/src/cuckoofilter.h"
#include <assert.h>
#include <math.h>
#include <iostream>
#include <vector>
#include <tr1/unordered_map>
using namespace std;

// generate n 64-bit random numbers
void random_gen(int n, vector<uint64_t> &store, mt19937 &rd)
{
    store.resize(n);
    for (int i = 0; i < n; i++)
        store[i] = (uint64_t(rd()) << 32) + rd();
}

size_t lookup(cuckoofilter::CuckooFilter<size_t, 16> &cf, vector<uint64_t> &s)
{
    // lookup set S and count false positives
    cout << "s lookup\n";
    size_t total_queries = 0;
    size_t false_queries = 0;
    for (size_t i = 0; i < s.size(); i++)
    {
        // assert(cf.Contain(l) == cuckoofilter::NotFound); // not accurate - haven't accounted for alternate buckets
        if (cf.Contain(s.at(i)) != cuckoofilter::NotFound)
        {
            false_queries++;
        }
        total_queries++;
    }
    // Output the measured false positive rate
    cout << "total fp's: " << false_queries << " out of " << total_queries << "\n";
    cout << "false positive rate is " << 100.0 * false_queries / total_queries << "%\n";
    return false_queries;
}

int main(int argc, char **argv)
{
    int seed = 1;
    mt19937 rd(seed);

    size_t rehash_limit = 5;

    size_t total_items = 50;
    cout << "inserting " << total_items << " items\n";
    size_t n = total_items / 0.95;
    // int n = total_items/1.2;
    cout << "creating filter size " << n << "\n";
    cuckoofilter::CuckooFilter<size_t, 16> cf(n);
    vector<uint64_t> r;
    vector<uint64_t> s;

    // 64-bit random numbers to insert and lookup -> lookup_size = insert_size * 100
    random_gen(total_items, r, rd);
    random_gen(total_items * 100, s, rd);

    // add set R to filter
    for (auto c : r)
    {
        assert(cf.Add(c) == cuckoofilter::Ok);
        assert(cf.Contain(c) == cuckoofilter::Ok);
        // if(cf.Contain(c) != cuckoofilter::Ok) {
        //     cout << "ERROR\n";
        //     return 1;
        // }
    }

    // cout << cf.Info() << "\n";
    size_t rehash_lup = 0;
    while (lookup(cf, s) > 0 && rehash_lup < rehash_limit)
    {
        cf.RehashCheck(s);
        rehash_lup++;
        if (rehash_lup >= rehash_limit)
            cout << "rehashed too many times...\n";
    }
    cout << "full rehash lookup count: " << rehash_lup << "\n";
    // lookup(cf, s);
    // for(auto c : r)
    //     assert(cf.Contain(c) == cuckoofilter::Ok);
    return 0;
}
