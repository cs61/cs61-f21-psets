#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
// Advanced error message for freeing data inside another heap block.

int main() {
    void* ptr1 = malloc(1020);
    void* ptr2 = malloc(2308);
    void* ptr3 = malloc(6161);
    free((char*) ptr2 + 64);
    free(ptr1);
    free(ptr2);
    free(ptr3);
    m61_print_statistics();
}

//! MEMORY BUG: test???.cc:11: invalid free of pointer ???, not allocated
//!   test???.cc:9: ??? is 64 bytes inside a 2308 byte region allocated here
//! ???
