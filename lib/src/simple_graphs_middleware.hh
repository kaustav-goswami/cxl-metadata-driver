#ifndef __SIMPLE_GRAPHS_MIDDLEWARE_HH__
#define __SIMPLE_GRAPHS_MIDDLEWARE_HH__

// The purpose of this file is to include the necessary headers for the
// GAPBS middleware. The end user does not know where are the graphs. So this
// files uses the s_dmalloc.hh to return gapbs specific pointers to a subset of
// the graph.

// The OS can only set or remove permissions. The hardware is responsible for
// enforcing the permissions.

// This is a pure library that is dependent on s_dmalloc.hh
#include "s_dmalloc.hh"

// This middleware needs to understand the structure of the graphs software.
// Rest of the data structures will be taken care of by the application itself

enum METADATA {
    VERTEX,
    EDGE,
    ROWP_SIZE,
    COLI_SIZE,
    WEIGHT_SIZE,
    SYNC,
    // There needs to be a variable defining the total number of groups
    GROUPS,
    // After the sync variable, there will be an array with the pointers to the
    // actual row pointers. In the physical memory, checks will be placed to
    // determine if a given node and access rights have permission to access
    // the memory
    PERMISSION_ARRAY
};

#define INT sizeof(int)

typedef struct graph_entry graph_alloc_t;

struct graph_entry {
    // there needs to be a single pointer to the secure memory
    dmalloc_t *_mem_ptr;
    // there needs to be a single pointer to the data
    int *_data_ptr;
    int _row_ptr_start;
    int _row_ptr_end;
    int _col_idx_start;
    int _col_idx_end;
};

// supply a graph name to querry if this graph is supported
bool is_supported(const char *graph_name, int access_group);

// The graph can be allocated via the given structure.
graph_alloc_t *graph_middleware(int host_id, int access_group, int group_elements);


#endif // __GAPBS_MIDDLEWARE_HH__
