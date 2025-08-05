#include "simple_graphs_middleware.hh"

bool is_supported(const char *graph_name, int access_group) {
    fatal("NotImplementedError");
    return true;
}

graph_alloc_t *graph_middleware(int host_id, int access_group,
                                                        int group_elements) {
    // image that the graph is already allocated and partioned.

    // ask for accessing the metadata.
    graph_alloc_t *secure_graph = 
                            (graph_alloc_t *) malloc (sizeof(graph_alloc_t));

    // create a context for this user. keep it simple for now. just one process
    unsigned int processes[8] = {getpid(), 0, 0, 0, 0, 0, 0, 0};
    context_t *context = create_context(host_id, processes, 1);

    bool permission;
    // We need read permissions if we are host > 0
    if (host_id == 0)
        permission = 0b1;
    else
        permission = 0b0;
    
    // let's keep test mode as true
    bool test_mode = true;
    bool verbose = true;
    // We need to somehow know the size. initially, the entry must be for the
    // metadata. then we proceed to append the permission table.
    size_t size = sizeof(METADATA) * sizeof(int);
    dmalloc_t *s_ptr = secure_alloc(size, *context, permission, test_mode,
                                                                    verbose);
    // Depending upon whether I am reading or writing the graph, these
    // assignments change
    if (host_id == 0) {
        // I don't really need to take care of anything. I have full rw
        // permissions on the remote memory
        // create full access on the graph.
        range_t new_range;
        new_range.pstart = size;
        new_range.vstart = s_ptr->data_start_address;
        new_range.size = 0x2000000000;
        request_extension(s_ptr, host_id, new_range, *context, permission, verbose);
        // just assign the data_pointer to the graph.
    } 
    else {
        // once the allocation is complete, the middleware still needs to
        // assign and get more memory access.

        // then based on the access group, ask for permissions per memory range
        // assume that 1% of the graph is assigned to host id 1.
        // need to know the group id
        int *row_ptr_size = &s_ptr->data_start_address[EDGE];
        int *col_idx_size = &s_ptr->data_start_address[ROWP_SIZE];
        int *permission_array_size = 
                                &s_ptr->data_start_address[PERMISSION_ARRAY];

        int *group_id = &s_ptr->data_start_address[SYNC];

        // We need to request the permission array first to read (important).
        range_t new_range;
        // size is at metadata. the start needs to be at the size
        new_range.pstart = size;
        // the virtual address start will be simply offsetted.
        new_range.vstart = s_ptr->data_start_address + size;
        // the new size is size + the permission array. so only supply the
        // permission size.
        new_range.size = *permission_array_size * sizeof(int);
        request_extension(s_ptr, host_id, new_range, *context, permission, verbose);

        // next we need to access the right elements from the row_ptr array.
        new_range.pstart = new_range.size;
        new_range.pstart = size + *group_id * group_elements * sizeof(int); 
        new_range.size = *group_id * group_elements * sizeof(int); 

        request_extension(s_ptr, host_id, new_range, *context, permission, verbose);

        // for now allow the entire col idx to access the memory.
        new_range.pstart = size + *row_ptr_size * sizeof(int);
        new_range.size = *col_idx_size * sizeof(int);
        request_extension(s_ptr, host_id, new_range, *context, permission, verbose);
    }

    secure_graph->_mem_ptr = s_ptr;
    secure_graph->_data_ptr = (int *) (s_ptr + 0x40000000);

    return secure_graph;

}