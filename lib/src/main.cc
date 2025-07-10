#include <stdlib.h>
#include <stdio.h>

// #include "utility.hh"
// #include "dmalloc.h"
#include "s_dmalloc.hh"

struct simple {
    int *addr;
    int *other_addr;
};

int main() {
    int *ptr = shmalloc(1, 0);

    assign_all_global_variables(ptr);

    struct simple **list = (struct simple **) malloc (sizeof(struct simple *));
    // struct simple *list = (struct simple *) &ptr[0];
    // *list = (struct simple *) &ptr[0];

    for (int i = 0 ; i < 100 ; i++) {
        // allocate the ptr
        list[i] = (struct simple *) &ptr[i];
        list[i]->addr = ptr;
        list[i]->other_addr = ptr + 0x40000000;
    

    printf("addr: %#lx \n", list[i]->addr);
    }

    // fatal("abc %d %d %d", 1, 2, 3);
    return 0;
}
