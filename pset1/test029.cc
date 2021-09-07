#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
// Double free at a distance.

int main() {
    constexpr int nptrs = 10;
    void* ptrs[nptrs];
    for (int i = 0; i != nptrs; ++i) {
        ptrs[i] = malloc(10);
    }
    fprintf(stderr, "Will free %p\n", ptrs[2]);
    for (int i = 0; i != nptrs; ++i) {
        free(ptrs[i]);
    }
    free(ptrs[2]);
    m61_print_statistics();
}

//! Will free ??{0x\w+}=ptr??
//! MEMORY BUG???: invalid free of pointer ??ptr??, double free
//! ???
