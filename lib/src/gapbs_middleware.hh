#ifndef __GAPBS_MIDDLEWARE_HH__
#define __GAPBS_MIDDLEWARE_HH__

// The purpose of this file is to include the necessary headers for the
// GAPBS middleware. The end user does not know where are the graphs. So this
// files uses the s_dmalloc.hh to return gapbs specific pointers to a subset of
// the graph.

// The OS can only set or remove permissions. The hardware is responsible for
// enforcing the permissions.

// This is a pure library that is dependent on s_dmalloc.hh
#include "s_dmalloc.hh"

#define INT sizeof(int)
#define INT64_T sizeof(int64_t)
#define SIZE_T sizeof(size_t)

typedef struct gapbs_entry gapbs_alloc_t;
typedef struct graph_sizes_metadata metadata_t;
typedef struct pair pairs_t;

struct gapbs_entry {
    // The start address of the graph
    dmalloc_t* entry;

    // The metadata of the graph is organized as shown in : https://github.com
    // /darchr/shared-gapbs/blob/shared-disaggregated-v3/README-SDM.md

    // there is a pointer to the synch variable. These variables need to be
    // typecasted before it can be used in the actual program.
    int *synch_variable;    // int
    int64_t *num_nodes;         // int64_t
    size_t *index_x;           // size_t
    size_t *neigh_size;        // size_t
    int *neigh_array;       // DestID_
    int *column_size;       // size_t
    // the DestID_ x DestID for the indices is suepr complicated!
    int *index_arr;

    // The permissions of the graph
    bool permission;
};

// supports upto g 25
struct pair {
    int size;
    size_t neigh_sizes[25];

};

struct graph_sizes_metadata {
    // a structure that holds the metadata lengths
    size_t neigh;
};

void set_lookup_table(int graph_size) {
    // the sizes of the graphs need to be defined. For a given size, this
    // function returns the length of the metadata.
}

size_t get_neigh_size() {}

gapbs_alloc_t* gapbs_middleware(size_t size, int host_id, int permissions,
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
    // A pointer to the gapbs_entry structure that contains the start address
    // of the graph and the size of the graph.

    // With the final version of the driver, we need to use proper constructs
    // to be able to access the remote memory.
    // Create an array with my process ID.

    // The permission table is supposed to get updated automatically.

    // Now meta_start knows waht memory regions to access. This host will
    // request the permission table to access the regions 


    unsigned int process_ids[MAX_PROCESSES] = {(unsigned int) getpid()};
    int num_valid_processes = 1;
    
    // MPI processes for example needs to have this middleware.
    context_t *context = create_context(
                                    host_id, process_ids, num_valid_processes);
    
    // We don't know what is the memory range where the graph data is located.
    // This is not a trivial problem to handle. Also we don't know how many
    // ranges do we need.
    range_t *meta_range = (range_t *) malloc (sizeof(range_t));
    // idk where does this range start? the drive rneeds to figure this out!
    meta_range->start = NULL;
    meta_range->size = INT + INT64_T + SIZE_T + SIZE_T;

    // requesting the range from start to the meta_range size
    dmalloc_t *start = secure_alloc(meta_range->size, *context, permissions, test_mode, verbose);

    if (start->data_start_address == NULL) {
        // the driver didnt gave us access to the data region.
        fatal("Couldn't get permission to access the remtoe memory!");
    }

    // Now we have access to the data of the remote memory.
    // But we want more region in the remote memory.
    gapbs_alloc_t *gapbs = (gapbs_alloc_t *) malloc (sizeof(gapbs_alloc_t));

    // type convertion nightmare :(
    gapbs->synch_variable = (int *) &start->data_start_address[0];
    gapbs->num_nodes = (int64_t *) &start->data_start_address[INT];
    gapbs->index_x = (size_t *) &start->data_start_address[INT + INT64_T];
    gapbs->neigh_size = (size_t *) &start->data_start_address[INT + INT64_T + SIZE_T];

    // I don't have any further access! Request for more memory.

    // what do we need now?

    // start by asking the driver to give access to the neigh array. It's size
    // is defined.
    // create an entry for the neigh array
    range_t *range = (range_t *) malloc (sizeof(range_t));
    // The start of this range is where neigh_array starts. 
    range->start = start->data_start_address + meta_range->size;
    // what ever is the value at neigh size
    range->size = *gapbs->neigh_size * sizeof(DestID_);

    entry_t *entry = get_proposed_entry();
    // The context will stay the same. The host_id and the processes running on
    // the host hasn't changed.
    entry->domain.context[0] = *context;
    // requesting for more memory!
    entry->range = *range;
    // I just need to read these entries! Why?
    entry->permission = permissions;

    entry->is_valid = 1;

    if (write_proposed_entry(context->host_id, entry) == true) {
        // I wait until voting is done.
        while (get_count() <= get_participant_count() / 2) {
            // do nothing! FAM gives permission anyway!
        }
        info("got access to the neigh array!");
    }
    else {
        fatal("denied access to the neigh array!");
    }
    gapbs->neigh_array = (DestID_ *) &start->data_start_address[INT + INT64_T + SIZE_T + SIZE_T];

    // Now the data pointer can be further accessed to have the column size for
    // each index row. IDK how long this is yet. But we'll assume something.
    range->start = start->data_start_address + WHERE;
    range->size = HOW_MUCH;

    entry->range = *range;
    entry->is_valid = 1;


    if (write_proposed_entry(context->host_id, entry) == true) {
        // I wait until voting is done.
        while (get_count() <= get_participant_count() / 2) {
            // do nothing! FAM gives permission anyway!
        }
        info("got access to the index row array!");
    }
    else {
        fatal("denied access to the index row array!");
    }

    // similarly create pointers to each of the index arrays!!
    // TODO

    // Not easy!


    // Now look at gapbs' header to figure out where are the addresses.

    return gapbs;
}

#endif // __GAPBS_MIDDLEWARE_HH__