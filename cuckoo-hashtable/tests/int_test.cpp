#include <iostream>
#include <string>

#include <cuckoo_hashtable.h>

int main() {
  cuckoohash::hashtable<int> Table;

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
