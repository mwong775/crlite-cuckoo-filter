#include <stdio.h>
#include <string>
#include <iostream>

#include "cuckoohashtable.hh"
#include "city_hasher.hh"

int main() {
  cuckoohashtable::cuckoo_hashtable<int, CityHasher<int>>Table;

  for (int i = 0; i < 100; i++) {
    Table.insert(i);
  }

  for (int i = 0; i < 101; i++) {
    std::string out;

    if (Table.find(i)) {
      std::cout << i << "  " << out << std::endl;
    } else {
      std::cout << i << "  NOT FOUND" << std::endl;
    }
  }
}