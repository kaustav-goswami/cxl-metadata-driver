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
    struct s_dmalloc_entry *sptr = secure_alloc(1, 0, 1, 1, true);

    if (sptr->data_start_address == NULL) {
        // This means that the data_start_address is not set. This means that
        // the user has no access to the data_start_address. This is a security
        // feature to ensure that the user cannot access the data_start_address
        // directly.
        fatal("data_start_address is NULL. This is a security feature. "
                "You cannot access the data_start_address directly.");
    }
    // global addresses are now assigned
    // assign_all_global_variables(sptr->start_address);

    struct simple **list = (struct simple **) malloc (sizeof(struct simple *));
    // struct simple *list = (struct simple *) &ptr[0];
    // *list = (struct simple *) &ptr[0];

    for (int i = 0 ; i < 100 ; i++) {
        // allocate the ptr
        list[i] = (struct simple *) &sptr->data_start_address[i];
        list[i]->addr = sptr->start_address;
        list[i]->other_addr = sptr->start_address + 0x40000000;
    

    printf("addr: %#lx \n", list[i]->addr);
    }

    // fatal("abc %d %d %d", 1, 2, 3);
    return 0;
}
