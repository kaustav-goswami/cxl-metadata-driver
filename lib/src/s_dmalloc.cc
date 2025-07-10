#include "s_dmalloc.hh"

// --------------------- start of all utility functions -------------------- //

struct s_dmalloc_entry*
secure_alloc(size_t size, int host_id, int permissions, int test_mode,
                                                                bool verbose) {
    // Simple step here is to create a s_dmalloc_entry to store the start
    // addresses and the permissions.
    
    // Depending upon the test mode, we create dmalloc or shmalloc
    int *start_address;
    if (test_mode == 0)
        start_address = dmalloc(size, host_id);
    else if (test_mode == 1)
        start_address = shmalloc(size, host_id);
    else
        // Illegal operation
        fatal("Cannot create memory region without a valid test mode (%d)",
                test_mode);
    // Since this is the first time anyone is calling this function from this
    // host, make sure that the global variables are setup and the start
    // addresses aren't passed around functions.
    assign_all_global_variables(start_address);
    // make sure to define the data_start_address and make sure that the user
    // has no access to the data_start_address.
    int *data_start_address = NULL;
    // make sure that is_allowed is set to false
    bool is_allowed = false;
    // make sure that the participant count is set to 0 if I am the first host.
    if (get_participant_count() <= 0) {
        // This is the first host who is trying to create the head.
        set_participant_count(0);
    }
    // Now make sure that this host has permissions to access the memory.
    // First, if I am host 0, then I'll have permission if the participant host
    // count is 0.
    if (get_participant_count() == 0 && host_id == 0) {
        // I'll get permission as I am the only one in the system!
        is_allowed = true;
        // I need to create the head
        create_head(host_id, permissions, verbose);
    }
    else if (get_participant_count() > 0 && host_id == 0) {
        // I'm host 0 but my permission were revoked or I was kicked out
        // before. Therefore, we need to request permissions again to make sure
        // that I am legally allowed in!
        // TODO
        fatal("NotImplementedError!");
    }
    else if (get_participant_count() == 0 && host_id > 0) {
        // I am the first one here but I am not the primary host. This makes it
        // complicated as I don't know what happened before. I'll ignore if
        // someone prior created the table or not, I'll just go ahead and
        // re-create the head.
        is_allowed = true;
        create_head(host_id, permissions, verbose);
    }
    else if (get_participant_count() > 0 && host_id > 0) {
        // I am a new participant host. First I need to request permissions by
        // proposing a new entry. While others vote, I wait. If the voting goes
        // through, I'll enter my proposed entry into the permission table.
        // TODO
        fatal("NotImplementedError!");
    }
    else {
        // This is an unhandled exception! We'll crash the program here!
        fatal("Unhandled exception while creating the head! "
                "Info: participant count %d and host_id %d",
                get_participant_count(), host_id);
    }
    // The data segment of the flat memory will always start after 1 GiB. Why?
    if (is_allowed == true)
        data_start_address = start_address + TABLE_SIZE;
    
    // This is a variable stored in the local memory of the node. This is also
    // returned to the user making sure that the copy of the permission table
    // and the data start address is copied to the host as well.
    struct s_dmalloc_entry *global_addr_ = (struct s_dmalloc_entry *)
                                    malloc (sizeof(struct s_dmalloc_entry));
    // these variable are now global.
    global_addr_->start_address = start_address;
    global_addr_->data_start_address = data_start_address;
    global_addr_->permissions = permissions;

    return global_addr_;
}

void
create_head(int host_id, int permissions, bool verbose) {
    // We need to make sure that the global_addr_ is not null. Why?
    assert(global_addr_ != NULL);
    // This function only creates the head. Must have the host_id as 0 as the
    // first host who wants to create the sharing range must call this
    // function. However, exception can happen when everyone else left the
    // shared memory and a new shared memory needs to be created and someone
    // else other than host_id 0 is trying to create this range.
    if (host_id == 0 || (get_participant_count() == 0 && host_id > 0)) {
        // The head can be created here
        // Create the initial entry at the start of the metadata
        write_lock(IDLE, -1);
        // now lock the memory for me.
        while (write_lock(WRITE, host_id) != true) {
            // wait until the lock is acquired. This is done to handle
            // exceptions.
        }
        if (verbose)
            // print the status if the user wants to debug this.
            info_who_locked(host_id);
        // since i am the only sharer now, I set this to 1.
        set_participant_count(1);
        // set participant id at the right index. This is a flat table, indexed
        // by the host_id directly!
        set_participant_host_ids(host_id);
        // Need to create a proposed entry but I am the only user. So I'll
        // override the proposed entry and create an entry directly in the 
        // permission table?
        // XXX: Disable direct entry into the permission table.
        struct table_entry *entry =
                    (struct table_entry *) malloc (sizeof(struct table_entry));
        entry->domain_id = 0;
        entry->permission = 1;
        entry->shared_mask = 1;
        // TODO: This entry will go through. The FAM must create this entry and
        // update the index.
        set_proposed_entry(entry);

        // that's it! Now unlock the memory
        unlock(host_id);

        if (verbose)
            // print the status if the user wants to debug this.
            info_who_unlocked(host_id);
    }
    else {
        fatal("Cannot create head! There are others using the shared memory.");
    }
}

void
create_interrupt() {
    fatal("Interrupt is not implemented!");
}

int*
dmalloc(size_t size, int host_id) {
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

int*
hmalloc(size_t size, int host_id) {
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

int*
shmalloc(size_t size, int host_id) {
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

void
flush_x86_cache(int *_mmap_pointer, size_t size) {
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

void
munmap_memory(size_t size, int test_mode, int host_id) {
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

