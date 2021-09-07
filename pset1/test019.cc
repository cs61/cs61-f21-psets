#include "m61.hh"
#include <cstdio>
#include <deque>
#include <sys/resource.h>
// Many allocations and frees, tracking peak memory usage.
// (Bounded metadata test.)

int main() {
    std::default_random_engine randomness(std::random_device{}());

    // get initial memory usage
    struct rusage ubefore, uafter;
    int r = getrusage(RUSAGE_SELF, &ubefore);
    assert(r >= 0);

    // make at least 10M 20-byte allocations and free them;
    // at most 100 allocations are active at a time
    constexpr int nptrs = 100;
    std::deque<void*> ptrs;
    for (int i = 0; i != 20000000; ++i) {
        if (ptrs.size() >= nptrs
            || (ptrs.size() > 0 && uniform_int(0, 2, randomness) == 0)) {
            free(ptrs.front());
            ptrs.pop_front();
        } else {
            ptrs.push_back(malloc(20));
        }
    }
    while (!ptrs.empty()) {
        free(ptrs.front());
        ptrs.pop_front();
    }
    m61_print_statistics();

    // report peak memory usage
    r = getrusage(RUSAGE_SELF, &uafter);
    assert(r >= 0);
    if (uafter.ru_maxrss < ubefore.ru_maxrss) {
        printf("memory usage decreased over test?!\n");
    } else {
        size_t kb = uafter.ru_maxrss - ubefore.ru_maxrss;
        if (ubefore.ru_maxrss > 100000) {
            // Mac OS X reports memory usage in *bytes*, not KB
            kb /= 1024;
        }
        printf("peak memory used: %lukb\n", kb);
    }
}

//! alloc count: active          0   total ??>=10000000??   fail        ???
//! alloc size:  active          0   total ??>=200000000??   fail        ???
//! peak memory used: ??{\d+kb}=peak_memory??
