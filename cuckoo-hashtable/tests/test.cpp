#include "hashtable.h"
#include <assert.h>
#include <math.h>
#include <iostream>
#include <vector>
using namespace std;

// generate n 64-bit random numbers
void random_gen(int n, vector<uint64_t> &store, mt19937 &rd)
{
    store.resize(n);
    for (int i = 0; i < n; i++)
        store[i] = (uint64_t(rd()) << 32) + rd();
}

int main(int argc, char **argv) {
    cout << "cuckoo hashing test setup" << "\n";
    return 0;
}