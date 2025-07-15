#ifndef __GAPBS_MIDDLEWARE_HH__
#define __GAPBS_MIDDLEWARE_HH__

// The purpose of this file is to include the necessary headers for the
// GAPBS middleware. The end user does not know where are the graphs. So this
// files uses the s_dmalloc.hh to return gapbs specific pointers to a subset of
// the graph.

#include "s_dmalloc.hh"

typedef struct gapbs_entry gapbs_t;

struct gapbs_entry {
    // The start address of the graph
    struct s_dmalloc_entry *entry;
    int *nodes_;
    int *edges_;
    // The size of the graph as the cmd line argument
    size_t size;
    // The permissions of the graph
    int permissions;
};

struct gapbs_entry* gapbs_middleware(size_t size, int host_id, int permissions,
                                      int test_mode, bool verbose) {
    // This function is the middleware for GAPBS. It returns a pointer to the
    // graph that can be used by the GAPBS library.
    //
    // @params
    // :size: size of the graph as the command line argument -g
    // :host_id: ID of the host that is requesting the graph
    // :permissions: permissions of the graph
    // :test_mode: test mode (0 for /dev/dax, 1 for /dev/shmem)
    // :verbose: verbose mode (true or false)
    //
    // @returns
    // A pointer to the gapbs_entry structure that contains the start address of
    // the graph and the size of the graph.

    struct s_dmalloc_entry *entry = secure_alloc(size, host_id, permissions,
                                                 test_mode, verbose);
    
    struct gapbs_entry *gapbs = (struct gapbs_entry *) malloc(
                                                sizeof(struct gapbs_entry));
    gapbs->entry = entry;

    // Now look at gapbs' header to figure out where are the addresses.

    return gapbs;
}

#endif // __GAPBS_MIDDLEWARE_HH__