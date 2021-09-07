#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
#include <random>
// More diabolicality.

int main() {
    std::default_random_engine randomness(std::random_device{}());

    void* success = calloc(0x1000, 2);

    for (int i = 0; i != 100; ++i) {
        size_t a = uniform_int(size_t(0), size_t(0x2000000), randomness) * 16;
        size_t b = size_t(-1) / a;
        b += uniform_int(size_t(1), size_t(0x20000000) / a, randomness);
        void* p = calloc(a, b);
        assert(p == nullptr);
    }

    free(success);
    m61_print_statistics();
}

//! alloc count: active          0   total          1   fail        100
//! alloc size:  active          0   total       8192   fail        ???
