// There needs to be the main function that initializes the FAM and
// initializes the shared memory. This is the main function that will be called
// by the user to start the FAM. The user can then use the FAM to allocate
// memory and use it as a shared memory across hosts. The user can also use the
// FAM to allocate memory for the graph and use it as a shared memory across
// hosts. 

// I love the AI generated comments. Not really helpful always but they are
// useful to understand the code. I will keep them for now. (even this is AI
// generated comment lol)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>
#include <time.h>
#include <pthread.h>

// Lazy implementation of the FAM. This is the main file needs to compiled with

#include "s_dmalloc.hh"
#include "utility.hh"

#define VERBOSE 1
#define MY_HOST_ID -2

void *mon_udpate(void *start_address) {
    // This is the FAM's function that monitor and updates the permission table
    // FIXME: This needs to be behind the library interface.

    // Need a polling region of the memory to monitor the permission table.
    info("Starting the monitor thread for host %d", MY_HOST_ID);
    monitor_update(MY_HOST_ID, (int *) start_address);
    return NULL;

}

int main() {
    // This is the main function that initializes the FAM and initializes the
    // shared memory. The user can then use the FAM to allocate memory and use
    // it as a shared memory across hosts. The user can also use the FAM to
    // allocate memory for the graph and use it as a shared memory across hosts.
    
    // The FAM always resets the memory at start. If FAM is not running, then
    // none of the hosts can access the memory.
    munmap_memory(2, 1, 0); // reset the memory as the root user is done.

    // define my context! We need a better looking function :(
    context_t context = {MY_HOST_ID, {(unsigned int) getpid(),
                                                    0, 0, 0, 0, 0, 0, 0}, 1};
    // A single bit represents permissions: 0 -> read, 1 -> read/write
    bool permission = 0b1;
    bool test_mode = true;
    bool verbose = true;

    const size_t size = 0x100000000; // size of the memory to allocate.

    // Allocation as the FAM always goes through!
    dmalloc_t *sptr = secure_alloc(size, context, permission, test_mode, verbose);

    if (sptr->start_address == NULL) {
        fatal("start_address is NULL. This is a bug!.");
    }
    
    // Need threads for constant monitoring.
    init_fam(sptr->start_address);

    print_lock_info();

    pthread_t monitor_thread;
    pthread_create(&monitor_thread, NULL, mon_udpate,
        (void *) sptr->start_address);

     // assume I am the FAM and lemme see the entire table.
    print_permission_table(MY_HOST_ID);
    pthread_join(monitor_thread, NULL);
    
    return 0;

}