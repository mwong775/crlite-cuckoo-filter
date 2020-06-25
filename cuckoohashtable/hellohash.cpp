#include <stdio.h>
#include <string>
#include <iostream>

// #include <libcuckoo/cuckoohash_map.hh>
// #include <libcuckoo/city_hasher.hh>
#include "cuckoohashtable.h";
#include "city_hasher.hh";

int main() {
  cuckoohashtable::cuckoo_hashtable<int, std::string, CityHasher<int> >Table;

  for (int i = 0; i < 100; i++) {
    Table.insert(i, "hello"+std::to_string(i));
  }

  for (int i = 0; i < 101; i++) {
    std::string out;

    if (Table.find(i, out)) {
      std::cout << i << "  " << out << std::endl;
    } else {
      std::cout << i << "  NOT FOUND" << std::endl;
    }
  }
}
