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
    // get access to the remote memory region. this will give access to create
    // new permission entries to request for memory.

    size_t size = 0x0;      // specify 0 to get access to the entire memory
    int permission = 0x2;  // 0x0 -> no access, 0x1 -> read only, 0x2 rw-

    bool test_mode = true;
    bool verbose = true;

    // try another host with different params
    int host_id = MY_HOST_ID;
    bool is_del = false;
    if (argc > 1) {
        printf("%c\n", argv[1][0]);
        // also test removal
        if (argv[1][0] == 'x')
            is_del = true;
        else
            host_id = atoi(argv[1]);
    }


    // make sure to initializae the memory with the pid.
    dmalloc_t *s_ptr = secure_init(size, host_id, permission, test_mode, verbose);
    
    // if the start address is null that this will be caught in the allocation
    // files itself.
    if (s_ptr->start_address == NULL) {
        // allocation failed!
        fatal("Initialization failed for host %d", host_id);
    }

    // data_pointer_address will be valid at this point. this host can now
    // request for more memory. In this simple example, this host wants rw- on
    // the first 2 GiB of the shared memory.
    Addr start = 0x0;
    Addr end = 0x80000000;

    if (argc > 2) {
        start = 0x40000000;
        end = 0xC0000000;
    }

    entry_t *my_entry = create_entry(
                    start, end, permission, host_id, (unsigned int) getpid());

    printf("entry created from 0x%lx to 0x%lx with permissions %d by host %lu and "
           "PID %lu (%u) is_del %d\n",
            my_entry->start,
            my_entry->end,
            my_entry->permission,
            my_entry->host_mask,
            my_entry->pid_mask,
            (unsigned int) getpid(),
            is_del);
    
    // entry created, now request the driver to write this entry to the
    // `proposed update` section.
    if (!write_proposed_entry(host_id, my_entry, is_del)) {
        fatal("Host %d's request for the given range failed!", host_id);
    }

    // do regular business from here onwards!
    info("Initialization and allocation successful!");
    
    return 0;
}
