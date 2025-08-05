#ifndef __S_DMALLOC_H__
#define __S_DMALLOC_H__

// This file is taken from simple-graph to allocate memory for the graph
// software. It understands master and worker to make sure that the clients are
// not allowed to modify the graph in the remote host.
//
// For simple-graph,
// see https://github.com/kaustav-goswami/disaggregated-shared-graphs
// 
// The master node should ONLY be responsible for allocating the graph.
// For testing on a local host, the code can switch to use a hugepage. Huge
// pages must be enabled to do so.
// For local testing without sude, shmem was added too. By default, the shmem
// file is opened in the gapbs version.
// We need a overlord function that manages all the different objects for
// allocations. pvector is called multiple times, which makes managing a flat
// range of memory difficult.

// This version is specific for space-control, where software permissions are
// assigned to the head of the allocator that ensures the right permissions are
// assigned to the hosts.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>
#include <time.h>

#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

// This file is included for creating the head of the shared memory. The user
// starting the program will be trusted when setting up the permissions. The
// same user can be kicked out at a later time if this user becomes untrusted.
#include "s_permissions.hh"
#include "utility.hh"

// Standard definitions.
#define ONE_G 0x40000000

// The final thing missing in the design is the physical address as the
// starting address instead of virtual addresses. The driver doesn't understand
// virtual addresses. It's the job of the middleware to create that mapping!
#define WHERE_AM_I 0x0

// The first function is to do a secure dmalloc on a flat region of shared
// memory across hosts.
dmalloc_t* secure_alloc(size_t size, context_t context, bool permission,
                        int test_mode, bool verbose);
// TODO:
// There is no way I can extend the permission table with my existing ptr.
// on the existing s_ptr, there needs to be a way to extend the permission
// table with new entries.
void request_extension(dmalloc_t *s_ptr, int host_id, range_t range, context_t context, bool permission,
                                                                bool verbose);
// Now that we have created the mmap segment, we need to create the head if we
// are the primary host
void create_head(range_t range, context_t context, bool permission,
                                                                bool verbose);
// We need a function that traps illegal accesses!
void create_interrupt();
// Standard dmalloc functions are defined here.
int* dmalloc(size_t size, int host_id);
int* hmalloc(size_t size, int host_id);
int* shmalloc(size_t size, int host_id);

// Here are the rest of the utility functions except the inlines ones
void munmap_memory(size_t size, int test_mode, int host_id);
void flush_x86_cache(int *_mmap_pointer, size_t size);

// an asm utility function on x86 to force flush the cache.
[[gnu::unused]] static inline __attribute__((always_inline))
void clflushopt(volatile void *p) {
    // @params
    //
    // p: a pointer to the memory
    asm volatile("clflushopt (%0)\n"::"r"(p)
    : "memory");
}

// ---------------------- end of all utility functions --------------------- //
#endif  // __S_DMALLOC_HH__
