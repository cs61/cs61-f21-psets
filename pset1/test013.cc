#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
#include <random>
// Multiple callocs.

int main() {
    std::default_random_engine randomness(std::random_device{}());

    constexpr int nptrs = 5;
    char* ptrs[nptrs] = {nullptr, nullptr, nullptr, nullptr, nullptr};

    // do 10000 allocations and frees, checking each allocation for
    // zeroed contents
    for (int round = 0; round != 1000; ++round) {
        int index = uniform_int(0, nptrs - 1, randomness);
        if (!ptrs[index]) {
            // Allocate a new randomly-sized block of memory
            size_t size = uniform_int(1, 2000, randomness);
            char* p = (char*) calloc(size, 1);
            assert(p != nullptr);
            // check contents
            size_t i = 0;
            while (i != size && p[i] == 0) {
                ++i;
            }
            assert(i == size);
            // set to non-zero contents and save
            memset(p, 'A', size);
            ptrs[index] = p;

        } else {
            // Free previously-allocated block
            free(ptrs[index]);
            ptrs[index] = nullptr;
        }
    }

    for (int i = 0; i != nptrs; ++i) {
        free(ptrs[i]);
    }

    m61_print_statistics();
}

//! alloc count: active          0   total  ??>=500??   fail          0
//! alloc size:  active        ???   total        ???   fail          0
