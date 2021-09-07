#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
// Another diabolical wild free.

int main() {
    char* a = (char*) malloc(200);
    char* b = (char*) malloc(50);
    char* c = (char*) malloc(200);
    char* p = (char*) malloc(3000);
    (void) a, (void) c;
    memcpy(p, b - 208, 450);
    free(b);
    memcpy(b - 208, p, 450);
    free(b);
    m61_print_statistics();
}

//! MEMORY BUG???: ??? free of pointer ???
//! ???
