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
        start_address = smalloc(size, host_id);
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
    }
    else {
        // This is an unhandled exception! We'll crash the program here!
        fatal("Unhandled exception while creating the head! \
                Info: participant count %d and host_id %d",
                get_participant_count(), host_id);
    }
    // The data segment of the flat memory will always start after 1 GiB. Why?
    if (is_allowed == true)
        data_start_address = start_address + TABLE_SIZE;
    
    // This is a variable stored in the local memory of the node. This is also
    // returned to the user making sure that the copy of the permission table
    // and the data start address is copied to the host as well.
    global_addr_ = (struct table_entry *) malloc (sizeof(struct table_entry));
    global_addr_->start_address = start_address;
    global_addr_->data_start_address = data_start_address;
    global_addr_->permissions = permissions;

    return global_addr_;
}

void
create_head(int host_id, int permissions, bool verbose) {
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
        while (write_lock(WRITE, host_id)
                                                                    != true) {
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
