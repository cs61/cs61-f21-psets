#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
// Wild free inside more heap-allocated data.

int main() {
    void* ptr1 = malloc(1020);
    void* ptr2 = malloc(2308);
    void* ptr3 = malloc(6161);
    fprintf(stderr, "Bad pointer %p\n", (char*) ptr2 + 64);
    free((char*) ptr2 + 64);
    free(ptr1);
    free(ptr2);
    free(ptr3);
    m61_print_statistics();
}

//! Bad pointer ??{0x\w+}=ptr??
//! MEMORY BUG???: invalid free of pointer ??ptr??, not allocated
//! ???
