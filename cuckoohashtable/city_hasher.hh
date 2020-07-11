#include "city.cc"
#include <string>

/*! CityHasher is a std::hash-style wrapper around CityHash. We
 *  encourage using CityHasher instead of the default std::hash if
 *  possible. */
template <class Key>
class CityHasher {
public:
    size_t operator()(const Key& k, uint64 seed = 0) const {
        // return CityHash64((const char*) &k, sizeof(k));
        return CityHash64WithSeed((const char*) &k, sizeof(k), seed);
    }
};

/*! This is a template specialization of CityHasher for
template <>
class CityHasher<std::string> {
public:
    size_t operator()(const std::string& k) const {
        return CityHash64(k.c_str(), k.size());
    }
};
 *  std::string. */
