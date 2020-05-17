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

int main(int argc, char **argv) {
    int seed = 1;
    mt19937 rd(seed);

    int total_items = 100000;
    cuckoofilter::CuckooFilter<size_t, 16> cf(total_items);
    vector<uint64_t> r;
    vector<uint64_t> s;

    random_gen(total_items, r, rd);
    random_gen(total_items*100, s, rd);

    // for(int i = 0; i < total_items; i++) {
    //     r.push_back(i);
    // }
    // for(int j = total_items; j < total_items*300; j++) {
    //     s.push_back(j);
    // }

    // cf.HashAll(r, s);
    // cout << cf.Info() << "\n";

    for(auto c : r) { // still need to implement indices for lookup
        cf.Add(c);
        assert(cf.Contain(c) == cuckoofilter::Ok);
    }

    size_t total_queries = 0;
    size_t false_queries = 0;
    for(size_t i = 0; i < s.size(); i++) {
        // assert(cf.Contain(l) == cuckoofilter::NotFound); // not accurate - haven't accounted for alternate buckets
        if(cf.Contain(s.at(i)) != cuckoofilter::NotFound) {
            false_queries++;
        }
        total_queries++;
    }

    // Output the measured false positive rate
    cout << "total fp's: " << false_queries << "\n";
    cout << "false positive rate is " << 100.0 * false_queries / total_queries << "%\n";

    // cout << "Values returned by Pair: " << endl; 
    // cout << p.first << " " << p.second << endl; 
    // r.insert(p); // TODO: change value to list/vector to hold multiple tags/fp's to each bucket

    // print_map(r);
    
    // cout << cf.Info() << endl;
    return 0;
}
