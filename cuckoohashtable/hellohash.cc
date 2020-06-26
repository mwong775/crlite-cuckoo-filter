#include <stdio.h>
#include <string>
#include <iostream>

#include "cuckoohashtable.hh"
#include "city_hasher.hh"

using namespace std;

int main() {
  cuckoohashtable::cuckoo_hashtable<int, CityHasher<int>>Table;

  for (int i = 0; i < 100; i++) {
    Table.insert(i);
  }

  for (int i = 0; i < 101; i++) {
    string out;

    if (Table.find(i)) {
      // cout << i << "  " << out << endl;
    } else {
      cout << i << "  NOT FOUND" << endl;
    }
  }
}
