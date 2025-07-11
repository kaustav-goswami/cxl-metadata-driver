#include <stdlib.h>
#include <stdio.h>

// #include "utility.hh"
// #include "dmalloc.h"
#include "s_dmalloc.hh"

// remembe the structure of the head
/*
______________________________________ .. _____________________________ .. ____
| is     | who    | participant | participant | proposed | count | entries    |
| locked | locked | count (N)   | host IDs    | update   |       |            |   
|________|________|_____________|_____ .. ____|__________|_______|_____ .. ___|
<--4B---><--4B---><-----4B-----><----N*4B----><--entry--><--4B--><-- entries ->
*/

// defien a bunch of stuff to start debugginf
#define VERBOSE 1
// This is the host ID of the FAM. This is used to print the permission table
// and the lock information. This is a hardcoded value.
#define MY_FAKE_HOST_ID -2
#define MY_HOST_ID 0

struct simple {
    int *addr;
    int *other_addr;
};

int main(int argc, char **argv) {
    // munmap_memory(1, 1, MY_HOST_ID);

    // if there areguments
    int my_host_id = MY_HOST_ID;
    if (argc > 1) {
        my_host_id = atoi(argv[1]);
    }
    const size_t size = 1; // size of the memory to allocate
    struct s_dmalloc_entry *sptr = secure_alloc(size, my_host_id, 1, 1, true);

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

    // The assignment is done successfully. Now we can use the
    // sptr->data_start_address to access the data.

    // We need getter function to access the different sections of the head. We
    // already have the getter functions but this needs to be behind a better
    // looking API. Since this is not a C++ program, the user can still access
    // these getter functions directly.

    print_lock_info();

    // print the proposed update section.
    print_proposed_update(MY_FAKE_HOST_ID);

    sleep(5);

    // assume I am the FAM and lemme see the entire table.
    print_permission_table(MY_FAKE_HOST_ID);


    // struct simple **list = (struct simple **) malloc (sizeof(struct simple *));
    // // struct simple *list = (struct simple *) &ptr[0];
    // // *list = (struct simple *) &ptr[0];

    // for (int i = 0 ; i < 100 ; i++) {
    //     // allocate the ptr
    //     list[i] = (struct simple *) &sptr->data_start_address[i];
    //     list[i]->addr = sptr->start_address;
    //     list[i]->other_addr = sptr->start_address + 0x40000000;
    

    // printf("addr: %#lx \n", list[i]->addr);
    // }

    // fatal("abc %d %d %d", 1, 2, 3);

    // reset the memory as the root user is done!
    // munmap_memory(1, 1, MY_HOST_ID);
    return 0;
}
