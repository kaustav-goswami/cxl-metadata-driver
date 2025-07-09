#ifndef __DMALLOC_H__
#define __DMALLOC_H__

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

// The first function is to do a secure dmalloc on a flat region of shared
// memory across hosts.
struct s_dmalloc_entry *secure_alloc(
    size_t size, int host_id, int permissions, int test_mode, bool verbose);
// Now that we have created the mmap segment, we need to create the head if we
// are the primary host
void create_head(int host_id, int permissions, bool verbose);
// We need a function that traps illegal accesses!
void create_interrupt();
// Standard dmalloc functions are defined here.
int* dmalloc(size_t size, int host_id);
int* shmalloc(size_t size, int host_id);

// Here are the rest of the utility functions except the inlines ones
void munmap_memory(size_t size, int test_mode, int host_id);
void flush_x86_cache(int *_mmap_pointer, size_t size);

// an asm utility function on x86 to force flush the cache.
[[gnu::unused]] static inline __attribute__((always_inline)) void clflushopt(volatile void *p) {
    // @params
    //
    // p: a pointer to the memory
    asm volatile("clflushopt (%0)\n"::"r"(p)
    : "memory");
}

// a simple print function to indicate who locked the memory
void info_who_locked(int host_id) {
    printf("info: locked acquired by host %d\n", host_id);
}

// simple function to define the head of the shared memory. The head needs to
// be a fixed region of the shared memory. Entries are defined in the
// scontrol.hh file. The head is forced uncacheable in the hardware.
bool create_head(int host_id, bool permissions, bool verbose) {
    // This function only creates the head. Must have the host_id as 0 as the
    // first host who wants to create the sharing range must call this
    // function.
    assert(host_id == 0);

    // create the head and the initial entry. How to assume the variable range?
    //
    // 
    // Assign the initial lock to be -1 (default)
    write_lock_ver2(IDLE, -1);

    // Now lock the memory as myself.
    while (write_lock_ver2(WRITE, host_id) != true) {
        // wait until the lock is acquired. This is safe programming IMO.
    }
    // print the status if the user wants to debug this.
    if (verbose) {
        info_who_locked(host_id);
    }
    // FIXME: The number of hosts is fixed in this implementation. Make sure
    // to not do this for the full version.
    // XXX: The number of maximum participant hosts is 1024.
    
    // write participant count = 1. I am the only one right now.
    *(start + PARTICIPANT_COUNT) = 1;

    // write the ID of the host in the host ID section
    *(start + PARTICIPANT_COUNT + sizeof(int) * (host_id + 1)) = host_id;

    // write my entry. Since I am trusted and I create this region, I initially
    // own the entire region.
    struct table_entry initial_entry;
    initial_entry.domain_id = 0;
    initial_entry.permissions = 2;
    initial_entry.shared_mask = 1;

    *(start + COUNT) = 


    return true;    
}

// ---------------------- end of all utility functions --------------------- //
//
int* dmalloc(size_t size, int host_id) {
    // hehe, I love the name
    //
    // This function will create a MAP_SHARED memory map for the graph. The
    // idea is to allocate the graph (potentially a large graph) in the
    // remote/disaggregated memory and only the master will have RDWR flag. All
    // other hosts should only have READ permission.
    //
    // @params
    // :size: Size of requested memory in GiB
    // :host_id: An ID sent to dictate whether this is a master or a worker
    //
    // @returns
    // A pointer to the mmap call
    //
    int fd;
    // It is assumed that we are working with DAX devices. Will change change
    // to something else if needed later.
    const char *path = "/dev/dax0.0";
    
    // check if the caller is the master node
    if (host_id == 0)
        fd = open(path, O_RDWR);
    else
        // these are all graph workers. Prevent these workers from writing into
        // the graph.
        fd = open(path, O_RDONLY);

    // make sure that the open call was successful. Otherwise notify the user
    // before exiting.
    if (fd < 0) {
        printf("Error mounting! Make sure that the mount point %s is valid\n",
                path);
        exit(EXIT_FAILURE);
    }
    
    // Try allocating the required size for the graph. This might be
    // complicated if the graph is very large!
    // With all the testings, it seems that the mmap call is not a problem.
    // Please submit a bug report/issue if you see an error.

    // Make sure only the allocator has READ/WRITE permission. This forces
    // software coherence!
    int *ptr;
    if (host_id == 0) {
        ptr = (int *) mmap (nullptr, size * ONE_G,
                                    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    }
    else {
        ptr = (int *) mmap (nullptr, size * ONE_G,
                                            PROT_READ, MAP_SHARED, fd, 0);

    }

    // The map may fail due to several reasons but we notify the user.
    if (ptr == MAP_FAILED) {
        printf("The mmap call failed! Maybe it's too huge?\n");
        exit(EXIT_FAILURE);
    }
    // The mmap was successful! return the pointer to the user.
    return ptr;
}

int* hmalloc(size_t size, int host_id) {
    // To be used with HUGETLBFS page on a single host for testing.
    //
    // This function will create a MAP_SHARED memory map for the graph. The
    // idea is to allocate the graph (potentially a large graph) in the
    // remote/disaggregated memory and only the master will have RDWR flag. All
    // other hosts should only have READ permission.
    //
    // @params
    // :size: Size of requested memory in GIB
    // :host_id: An ID sent to dictate whether this is a master or a worker
    //
    // @returns
    // A pointer to the mmap call
    //
    FILE *fp;
    
    // XXX: WARNING, COHERENCE IS NOT ENFORCED!
    if (host_id == 0)
        // fp = fopen("/mnt/huge", "w+");
        fp = fopen("/dev/zero", "w+");
        // fp = shm_open("/myregion", O_RDWR | O_CREAT, 0666);
    else
        // fp = shm_open("/myregion", O_RDONLY, 0666);
        fp = fopen("/dev/zero", "w+");

    // It is assumed that we are working with DAX devices. Will change change
    // to something else if needed later.

    // make sure that the open call was successful. Otherwise notify the user
    // before exiting.
    if (fp == nullptr) {
        printf("Error mounting! Make sure that the mount point %s is valid\n",
                "non");
        exit(EXIT_FAILURE);
    }
    
    // Try allocating the required size for the graph. This might be
    // complicated if the graph is very large!
    /*
    int* ptr = (int *) mmap(
        0x0, 1 << 30 , PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS|  MAP_HUGETLB | (30UL << MAP_HUGE_SHIFT),
        fileno(fp), 0);
    */
    // try without the huge page but keep the backed file same
    int *ptr;
    if (host_id == 0)
        ptr = (int *) mmap(0x0, size * ONE_G,
                            PROT_READ | PROT_WRITE, MAP_SHARED, fileno(fp), 0);
    else
        ptr = (int *) mmap(0x0, size * ONE_G, 
                                        PROT_READ , MAP_SHARED, fileno(fp), 0);

    // The map may fail due to several reasons but we notify the user.
    if (ptr == MAP_FAILED) {
        printf("The mmap call failed! Maybe it's too huge?\n");
        exit(EXIT_FAILURE);
    }

    // The mmap was successful! return the pointer to the user.
    return ptr;

}

int* shmalloc(size_t size, int host_id) {
    // To be used with LINUX SHMEM on a single host for testing only
    //
    // This function will create a MAP_SHARED memory map for the graph. The
    // idea is to allocate the graph (potentially a large graph) in the
    // remote/disaggregated memory and only the master will have RDWR flag. All
    // other hosts should only have READ permission.
    //
    // @params
    // :size: Size of requested memory in GIB
    // :host_id: An ID sent to dictate whether this is a master or a worker
    //
    // @returns
    // A pointer to the mmap call
    //
    // XXX: WARNING, COHERENCE IS NOT ENFORCED when opening the file!
    int fd = shm_open("/my_shmem2", O_CREAT | O_RDWR | O_LARGEFILE, 0666);
    if (fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd, size * ONE_G) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    int *ptr;
    // PROT_READ/WRITE is enforced here.
    // depending upon the host id, we'll set the read/write permissons.
    if (host_id == 0) {
        ptr = (int *) mmap(NULL, size * ONE_G,
                                    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    }
    else {
        // This is a client host.
        ptr = (int *) mmap(NULL, size * ONE_G, PROT_READ, MAP_SHARED, fd, 0);
    }
    // The map may fail due to several reasons but we notify the user.
    if (ptr == MAP_FAILED) {
        printf("The mmap call failed! Maybe it's too huge?\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void flush_x86_cache(int *_mmap_pointer, size_t size) {
    // This function forces the entire mmapped region to be flushed out of the
    // cache.
    //
    // @params
    // :_mmap_pointer_: pointer to the start of the shared mapping area.
    // :size: size of the shared memory region in GiB

    size_t *_start = (size_t *) _mmap_pointer;
    #pragma omp parallel for
    for (size_t i = 0 ; i < (((size * ONE_G) / sizeof(size_t))) ; i += sizeof(size_t))
        clflushopt((_start) + i);
}

void munmap_memory(size_t size, int test_mode, int host_id) {
    // This is a utility function to zero out the memory allocated to the graph
    // explicitly. This has to be called by any host. Security checks aren't
    // present if the host wants to change it's ID
    //
    // @params
    // size_t size: size of the shared memory in GiB
    // int test_mode: indicate if you're using /dev/dax or /dev/shmem
    //
    // @returns
    // none
    
    int *start;
    if (host_id == 0) {
        if (test_mode)
            start = shmalloc(size, host_id);
        else
            start = dmalloc(size, host_id);
    }
    else {
        printf("warn: cannot munmap! Needs to be the allocator\n");
        return;
    }
    // absolutely bad programming right here!
    char *arr = (char *) &start[0];
    #pragma omp parallel for
    for (size_t i = 0 ; i < size * ONE_G; i++)
        arr[i] = 0; 
}

#endif // __DMALLOC_H__
