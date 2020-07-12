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

/**
     * CityHash usage:
     * init/declare: CityHasher<int> ch;
     * cout << key << " , cityhash: " << ch.operator()(key, seed);
    */

int main(int argc, char **argv)
{
    int seed = 1;
    mt19937 rd(seed);

    FILE *file = fopen("cuckoo_rehashing_fp.csv", "a"); // to print all stats to a file
    if (file == NULL)
    {
        perror("Couldn't open file\n");
        return 1;
    }

    if (argc <= 1)
    {
        cout << "enter number of items to insert\n";
        return 1;
    }

    uint64_t size = atoi(argv[1]); // 240000 for ~91% load factor
    // max load factor of 95%
    double max_lf = 0.95;
    uint64_t init_size = size / max_lf;
    cout << "init size: " << init_size << "\n";

    typedef uint64_t KeyType;
    cuckoohashtable::cuckoo_hashtable<KeyType, 12, CityHasher<KeyType>> table(init_size);

    vector<KeyType> r;
    vector<KeyType> s;

    // 64-bit random numbers to insert and lookup -> lookup_size = insert_size * 100
    random_gen(size, r, rd);
    random_gen(size * 100, s, rd);

    fprintf(file, "insert size, lookup size, init size, max percent load factor\n");
    fprintf(file, "%lu, %lu, %lu, %.1f\n\n", size, size * 100, init_size, max_lf * 100);

    // add set R to filter
    for (auto c : r)
    {
        table.insert(c);
    }

    // check for false negatives with set R
    size_t false_negs = 0;
    for (auto c : r)
    {
        if (table.lookup(c) < 0)
        {
            false_negs++;
        }
    }
    assert(false_negs == 0);

    // lookup set S and count false positives

    // track buckets needing rehash using a set
    std::unordered_set<KeyType> rehashBSet;
    int total_rehash = 0;

    /**
     * We check for false positives by looking up fingerprints using
     * mutually exclusive set S. Buckets yielding false positives are rehashed
     * with an incremented seed until no false positives remain from lookup.
     */
    fprintf(file, "rehash/lookup round, false positives, percent fp's\n");
    while (1)
    {
        size_t total_queries = 0;
        size_t false_queries = 0;
        size_t definite_queries = 0;

        table.start_lookup();
        for (auto l : s)
        {
            int index = table.lookup(l);
            if (index >= 0)
            {
                false_queries++;
                if (rehashBSet.find(index) == rehashBSet.end())
                {
                    rehashBSet.insert(index);
                }
            }
            if (table.find(l))
            {
                definite_queries++;
            }
            total_queries++;
        }
        assert(definite_queries == 0); // normal HT should only result in true negatives, no fp's

        double fp = (double)false_queries * 100.0 / total_queries;
        cout << "total false positives: " << false_queries << " out of " << total_queries
             << ", fp rate: " << fp << "%\n";

        fprintf(file, "%lu, %lu, %.6f\n", table.num_rehashes(), false_queries, fp);

        if (false_queries > 0)
        {
            total_rehash += table.rehash_buckets();
        }
        else
        {
            break;
        }
    }

    cout << table.info();
    fprintf(file, "slot per bucket, bucket count, capacity, load factor\n");
    fprintf(file, "%d, %lu, %lu, %.2f\n\n", table.slot_per_bucket(), table.bucket_count(), table.capacity(), table.load_factor() * 100.0);
    // table.bucketInfo();

    std::map<int, int> seed_map;
    table.seedInfo(seed_map);
    int max_rehash = 0;
    fprintf(file, "rehashes per bucket, count\n");
    for (auto &k : seed_map)
    {
        fprintf(file, "%d, %d\n", k.first, k.second);
    }

    double avg_rehashes = (double)total_rehash / table.bucket_count();
    double rehash_percent = (double)rehashBSet.size() * 100.0 / table.bucket_count();
    fprintf(file, "\ntotal rehashes, max rehash, average per bucket, percent rehashed buckets\n");
    fprintf(file, "%d, %lu, %.4f, %.3f\n\n", total_rehash, table.num_rehashes(), avg_rehashes, rehash_percent);

    fclose(file);

    return 0;
}