#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

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
    size_t size = 0x80000000;
    bool remove = false;

    if (argc > 1) {
        // info("Host ID is %s", argv[1]);
        printf("Host ID is %s\n", argv[1]);
        my_host_id = atoi(argv[1]);

        if (argc > 2) {
            size = (size_t) atoi(argv[2]) * 0x40000000;
            printf("size : %zu \n", size);
            if (argc > 3) {
                // need to remove an entry
                remove = true;
                printf("remove is true\n");
            }
        }
    }


    // define my context! We need a better looking function :(
    // unsigned int my_process_id[1] = { (unsigned int) getpid() };
    // context_t context = *create_context(
    //                             my_host_id, my_process_id, 1);
    context_t context = {my_host_id, {(unsigned int) getpid(), 0, 0, 0, 0, 0, 0, 0}, 1};
    // A single bit represents permissions: 0 -> read, 1 -> read/write
    bool permission = 0b1;
    bool test_mode = true;
    bool verbose = true;

    entry_t static_entry;
    printf("size: %d\n", sizeof(static_entry));
    // As a user I only know what memory I want for a given process.
    dmalloc_t *s_ptr = secure_alloc(size, context, permission, test_mode,
                                                                    verbose);
    // delete the local context
    // free(context);

    if (s_ptr->data_start_address == NULL) {
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

    sleep(2);

    // assume I am the FAM and lemme see the entire table.
    print_permission_table(MY_FAKE_HOST_ID);

    // TODO: You need to implement and test this!
    
    if (remove) {
        // create a context. Lets use the context from before
        entry_t removal_entry;
        // make this proposal as a deletion.
        removal_entry.initiator_host_id = 1;
        removal_entry.is_del = 1;
        removal_entry.is_valid = 1;

        // I only know who's permission I want to remove. If I want to get rid
        // of my own permission, then the proposer and the context.host_id
        // must match so that the PIDs are removed.
        // Let's try to remove someone else's permission first.
        // assert(my_host_id == 1);
        removal_entry.domain.context[0].host_id = 2;
        // IDK what is my virtual address or someone else's V.A but I know the
        // physical address
        removal_entry.range.pstart = WHERE_AM_I + ONE_G;
        removal_entry.range.size = size - 1;
        
        // create a proposal to remove this entry. tell the s_permission to do
        // this for you
        write_proposed_entry(my_host_id, &removal_entry);

    }


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
