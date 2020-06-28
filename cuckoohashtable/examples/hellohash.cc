#include <stdio.h>
#include <string>
#include <iostream>

#include "../hashtable/cuckoohashtable.hh"
// #include "city_hasher.hh"

using namespace std;

int main()
{
  int size = 240000;
  int init_size = size / 0.95; // max load factor of 95%
  cout << "init size: " << init_size << "\n";
  cuckoohashtable::cuckoo_hashtable<int> Table(init_size); // , CityHasher<int>

  for (int i = 1; i <= size; i++)
  {
    pair<size_t, size_t> location = Table.insert(i);
    // cout << "insert " << i << " at index " << location.first << ", slot " << location.second << "\n";
  }

  for (int i = 1; i <= size; i++)
  {
    string out;

    if (Table.find(i))
    {
      // if (i % 100 == 0)
      //   cout << i << "  " << out << endl;
    }
    else
    {
      cout << i << "  NOT FOUND" << endl;
    }
  }

  Table.info();
}
