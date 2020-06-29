#include "cuckoopair.hh"
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

    typedef uint64_t KeyType;
    int size = 500;               // 240000 for ~91% load factor
    int init_size = size / 0.95; // max load factor of 95%
    cout << "init size: " << init_size << "\n";
    cuckoo_pair<KeyType, 12> cp(size);

    // cp.info();

    vector<KeyType> r;
    vector<KeyType> s;

    // 64-bit random numbers to insert and lookup -> lookup_size = insert_size * 100
    random_gen(size, r, rd);
    random_gen(size * 100, s, rd);

    // add set R to filter
    for (auto c : r)
    {
        // cout << c << " ";
        cp.insert(c);
    }

        // lookup set S and count false positives
    cout << "s lookup\n";
    size_t total_queries = 0;
    size_t false_queries = 0;
    for (size_t i = 0; i < s.size(); i++)
    {
        // if ()
        // {
        //     false_queries++;
        // }
        total_queries++;
    }
    // Output the measured false positive rate
    cout << "total fp's: " << false_queries << " out of " << total_queries << "\n";
    cout << "false positive rate is " << 100.0 * false_queries / total_queries << "%\n";

    cp.info();

    return 0;
}