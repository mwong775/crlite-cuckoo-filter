#include <stdio.h>
#include <string>
#include <iostream>

#include "cuckoohashtable.hh"
#include "city_hasher.hh"

using namespace std;

int main() {
  cuckoohashtable::cuckoo_hashtable<int, CityHasher<int>>Table;
  int size = 10;

  for (int i = 0; i < size; i++) {
    pair<size_t, size_t> location = Table.insert(i);
    cout << "insert " << i << " at index " << location.first << ", slot " << location.second << "\n";
  }

  for (int i = 0; i < size; i++) {
    string out;

    if (Table.find(i)) {
      // cout << i << "  " << out << endl;
    } else {
      cout << i << "  NOT FOUND" << endl;
    }
  }

  Table.info();
}
