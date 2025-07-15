#include "s_permissions.hh"

// define all the global variables that are used to manage the shared memory
// region. These are all undefined in the header file.
dmalloc_t *global_addr_;
bool verbose = false;                    // verbose is set by the parent
                                 // function
int* is_locked;                  // assigning is locked as a variable
int* who_locked;                 // similar
int* participant_count;
int* participant_host_ids;
entry_t* proposed_update;
int* count;
entry_t* permission_table = NULL;
int permission_table_count;
int *permission_table_index;
volatile entry_t* monitor_region = NULL;
volatile uint8_t *shared_region = NULL;
volatile int *futex_flag = NULL;

// A FAM exclusive integer used to manage the domain ids.
int domains;

void increment_domains() {
    domains = domains + 1;
    if (verbose) {
        info("domains = %d", domains);
    }
}

int get_domains() {
    return domains;
}


void
assign_all_global_variables(int* start_address, bool this_verbose) {
    // We don't need the host_id tp assign the global variables. Instead, we
    // need the start address of the mmapped region
    is_locked = &start_address[IS_LOCKED];
    who_locked = &start_address[WHO_LOCKED];
    participant_count = &start_address[PARTICIPANT_COUNT];
    participant_host_ids = &start_address[PARTICIPANT_HOST_IDS];
    proposed_update = (entry_t*) &start_address[PROPOSED_UPDATE];
    count = &start_address[COUNT];
    permission_table_index = &start_address[INDEX_COUNT];
    // The participant hosts should know where the start is.
    permission_table = (entry_t *) &start_address[PERMISSION_TABLE];
    verbose = this_verbose;
    // Do we need the setter functions now?..
}

int
get_lock_status() {
    // return the value of the start value of the first integer.
    return *is_locked;
}

bool
write_lock(int action, int host_id) {
    // this is complicated!
    if (action == WRITE) {
        // is the current state IDLE?
        if (get_lock_status() == IDLE) {
            // lock it!
            set_is_locked(WRITE);
            set_who_locked(host_id);
            return true;
        }
        else {
            // cannot lock! someone is writing.
            return false;
        }
    }
    else if (action == IDLE) {
        // How do i unlock this?
        // make sure the current state is WRITE
        // assert(get_lock_status() == WRITE);
        if ((get_lock_status() == IDLE) ||
                (get_lock_status() == WRITE && get_who_locked() == host_id)) {
            // unlock it!
            set_who_locked(-1); // no one locked it.
            set_is_locked(IDLE); // set the state to IDLE
            return true;
        }
        // If the host_id is -1, then override the lock and reset it.
        else if (get_lock_status() == IDLE && host_id == -1) {
            // override the lock!
            set_who_locked(-1);
            set_is_locked(IDLE); // set the state to IDLE
            return true;
        }
        // if the lock is undefined
        else if ((get_lock_status() < IDLE || get_lock_status() > WRITE) && host_id == -1) {
            set_who_locked(-1);
            set_is_locked(IDLE); // set the state to IDLE
            return true;
        }
        else {
            // cannot unlock! someone is writing
            return false;
        }
    }
    else {
        // make sure this is never called!
        fatal("invalid action: %d by host %d", action, host_id);
        // unreachable code
        return false;
    }
    // make sure that this is never reached!
    assert(false && "unreachable code reached!");
    // unreachable code.
    return false;
}
    
// Then we need a data writer
bool
write_proposed_entry(int host_id, entry_t *entry) {
    // This is always written into a fixed region of the memory. This cannot be
    // exposed as it is to the end user. Make sure to update this somewhere and
    // make it like an API.
    // We need to write the values in entry to the proposed update
    proposed_update->domain = entry->domain;
    proposed_update->range = entry->range;
    proposed_update->permission = entry->permission;
    proposed_update->shared_mask = entry->shared_mask;
    proposed_update->is_valid = entry->is_valid;
    proposed_update->is_dirty = entry->is_dirty;
    if (verbose)
        info("Proposed entry written by host %d, pid %u at (%p, %lu) and domain %d",
            entry->domain.context[0].host_id,
            entry->domain.context[0].process_id[0],
            entry->range.start,
            entry->range.size,
            entry->domain.id);
    // entry will be killed later.
    return true;
}
// We need a voter!
void vote_entry(int host_id, int vote) {
    // check if this host can vote? The host id array is a flat table with set
    // and unset values.
    if (get_participant_host_ids(host_id) != 0)
        *count += 1;
    else
        // This host is not allowed to vote. create an interrupt?
        fatal("This host %d is not allowed to vote!", host_id);
}

// We need a function to create a new entry and wait until it gets approved by
// everyone.
bool create_and_wait_to_get_access(int host_id, entry_t *entry) {
    // This is the most complicated function IMHO
    printf("create and waid %d ---- ", get_permission_table_index());
    if (write_proposed_entry(host_id, entry) == true) {
        // wait until voting is done.
        while (*count <= get_participant_count() / 2) {
            // wait until voting is done. How????
            // TODO: Add a timeout condition.
        }
        printf(" create and waid %d \n", get_permission_table_index());
        return true;
    }
    else
        return false;
}

// Finally we need an unlock function to unlock the metadata.
bool unlock(int host_id) {
    if (get_is_locked() == WRITE && get_who_locked() == host_id) {
        // The section is locked and i am the one who locked it. unlock!
        set_is_locked(IDLE);
        set_who_locked(-1);
        return true;
    }
    else if (host_id == -1) {
        // override whoever locked it before
        set_is_locked(IDLE);
        set_who_locked(-1);
        return true;
    }
    else {
        // wait until whoever created this lock unlocks it.
        return false;
    }
}

void set_is_locked(int action) {
    // make sure that action is bounded!
    assert(action == IDLE || action == READ || action == WRITE);
    *is_locked = action;
}

int get_who_locked() {
    return *who_locked;
}

void set_who_locked(int host_id) {
    *who_locked = host_id;
}

int get_participant_count() {
    return *participant_count;
}
// This must be set in consensus
void set_participant_count(int count) {
    // make sure that participant count is always bounded!
    assert(count >= 0 && count <= MAX_PARTICIPANT_COUNT);
    *participant_count = count; 
}
// Returns the integer stored at the index offset
int get_participant_host_ids(size_t index) {
    // make sure that index is bounded!
    assert(index >= 0 && index <= MAX_PARTICIPANT_COUNT);
    // The flat table structure makes sure that the index of this host id is
    // already set.
    return (int) participant_host_ids[index];
}

void set_participant_host_ids(size_t index) {
    // index is the host_id
    participant_host_ids[index] = true;
}
entry_t* get_proposed_entry() {
    return proposed_update;
}

void set_valid_bit() {
    proposed_update->is_valid = false;
}

void set_proposed_entry(entry_t* entry) {
    assert(false && "call write_proposed_entry\n");
}
int get_count() {
    return *count;
}
// Sets an integer to vote. Is in between 0 and 1.
void set_count(int my_count) {
    if (my_count > 0)
        *count += 1;
}

void reset_count() {
    *count = 0;
}

// Returns a pointer to the start of the permission table. This can be null if
// this is the first entry to the table.
entry_t* get_permission_table(int host_id) {
    // a participant host wants to see the table but we need to check if the
    // host_id has access and the table is not null.
    if (host_id >= 0) {
        // I am a participant host. I need to check if the host_id has access
        // to the permission.
        if (participant_host_ids[host_id] == true) {
            // Finally check if the permission table is initialized.
            if (permission_table != NULL)
                // return the permission table.
                return permission_table;
            else {
                // The permission table is not initialized yet. This means that
                // the host_id is not allowed to access the permission table.
                info("The permission table is not initialized!");
                return NULL; // unreachable code
            }
        }
        else {
            // This host ID does not have permission to access the permission
            // table
            fatal("Host %d does not have permissions to read the permissions",
                    host_id);
            return NULL; // unreachable code
        }
    }
    // I can be the FAM and I have full control to the permission table.
    else {
        assert(host_id == FAM_ID);
        // This is the FAM. It can access the permission table. This can be
        // null but the move_ function handles this.
        return permission_table;
    }
}

// All the getter functions should work now.
int get_is_locked() {
    return *is_locked;
}

void allocate_table_entry() {
    entry_t *head = get_permission_table(FAM_ID);
    // the first enty is already allocated!
    if (get_permission_table_index() > 0)
        // allocate more memory
        head = (entry_t *) &global_addr_->start_address[PERMISSION_TABLE + 
                get_permission_table_index() * sizeof(entry_t)];
    
    // This index is initialized but do not increment the index yet! Why?
    if (verbose) {
        info("Permission table is initialized at %p with index %d",
                head, get_permission_table_index());
    }
}

void populate_table_entry(int host_id, entry_t proposal) {
    // This function populates the permission table with the given context and
    // permission.
    entry_t *head = get_permission_table(host_id);
    // head will never be null as the allocation will always be true.
    if (get_permission_table_index() == 0) {
        // This means that the permission table is not initialized yet. We need
        // to initialize it.
        // allocate_table_entry();
        // head = get_permission_table(FAM_ID);
    
        // Now we can populate the entry.
        proposal.domain.id = get_domains();
        increment_domains();
        head[get_permission_table_index()] = proposal;
        // set the proposal's domain id to 0 as this is undefined until now.
        // head[get_permission_table_index()].domain.id = 0;
        head[get_permission_table_index()].domain.valid_contexts = 1;

        if (verbose) {
            // information on this new entry is printed if verbose is turned on
            info("Populating entry for host %d with process %u "
                "at index %d for domain id %d",
            head[get_permission_table_index()].domain.context[0].host_id,
            head[get_permission_table_index()].domain.context[0].process_id[0],
            get_permission_table_index(),
            head[get_permission_table_index()].domain.id);
        }
        // increase the table index by 1.
        set_permission_table_index(host_id, get_permission_table_index() + 1);
    }
    else {
        // see if this entry can be merged? For a given range of memory, if
        // multiple hosts are trying to access the same memory, we can merge
        // the entries.
        bool flag = false;
        for (int i = 0; i < get_permission_table_index(); i++) {
            // check if the host_id and the process_id matches
            if (head[i].range.start == proposal.range.start &&
                    head[i].range.size == proposal.range.size &&
                    head[i].permission == proposal.permission) {
                // This means that the entry already exists. We can merge the
                // entries.
                if (verbose) {
                    // print this to the usr
                    info("Merging entry for host %d with process %u "
                            "into existing entry at index %d",
                            proposal.domain.context[0].host_id,
                            proposal.domain.context[0].process_id[0], i);
                }
                head[i].domain.context[head[i].domain.valid_contexts] = 
                    proposal.domain.context[0];
                head[i].domain.valid_contexts++;
                flag = true;
                break;
            }
        }
        if (flag == false) {
            // This means that the entry does not exist. We can add it to the
            // permission table.
            allocate_table_entry();
            proposal.domain.id = get_domains();
            increment_domains();
            head[get_permission_table_index()] = proposal;

            // Increment the index
            set_permission_table_index(
                                    host_id, get_permission_table_index() + 1);
            if (verbose) {
                info("Creating a new entry with a new domain %d", get_domains() - 1);
            }
            // The needs to be assigned a new domain. Domains just increase
            // right now.
            // head[get_permission_table_index()].domain.id = get_domains();
            // increment_domains();
        }
    }
    // print the permission table.
    if (verbose) {
        // only possible if im the FAM.
        print_permission_table(host_id);
    }
    // does not return anything.
}

bool remove_table_entry(int host_id, entry_t proposal) {
    // similar to the populate version of the function but returns a boolean
    // value depending upon whether an entry was found and deleted or not.

    // XXX: Entries cannot be per process. If a host says that it wants to
    // remove access of another host, then it only know that hosts' id not the
    // processes.
    // Waht if other processes of my own system is trying to be malicious?
    // then the requestor and the victim host must be the same!
    bool is_found = false;

    // iterate over the permission table. how??
    // the memory range must match. for a given range of memory, if the
    // proposed host has access to the memory, remove that host from accessing
    // the memory anymore.
    entry_t *head = get_permission_table(host_id);
    for (int i = 0 ; i < get_permission_table_index(); i++) {
        // compare the range.
        if (head[i].range.start == proposal.range.start &&
                head[i].range.size == proposal.range.size &&
                head[i].permission == proposal.permission) {
            // found the range, now compare if proposed->host_id has access or
            // not. The victim host will always be at 0th index of the domain
            // context.
            for (int j = 0 ; j < MAX_CONTEXT; j++) {
                if (head[i].domain.context[j].host_id ==
                                        proposal.domain.context[0].host_id) {
                    // Found the host!
                    // see I am the same host as the initiator?
                    if (proposal.initiator_host_id ==
                                        head[i].domain.context[j].host_id) {
                        // check what processes to remove
                        fatal("NotImplementedError! Same host different"
                            " processes!");
                    }
                    else {
                        // someone wants to remove this entry!
                        is_found = true;
                        // The FAM is lazy and simply sets the host_id to
                        // itself unless deployed in the wild
                        head[i].domain.context[j].host_id = FAM_ID;
                        warn("This version of the code does not rearrange the"
                                                                    " table");
                        // All the access rights of this host is completely
                        // removed! 
                    }
                }
            }
        }
        else 
            if (verbose) {
                info("The requested permission removal is not valid! "
                    "Either the victim host does not have access permissions "
                    "or the ranges are incorrect!");
            }
    }
    // depending upon the outcome return is_found!
    return is_found;
}

// Here are the functions needed by the FAM to move entries from the proposed
// section to the actual table. This is something we need to figure out in the
// long run.
bool move_proposed_entry(int host_id) {
    // make sure that this is the FAM and no one else. For actual
    // implementation, HOST IDs are appended at the CXL switch so hosts never
    // has control on what is their host_id. This is not a security threat.
    assert(host_id == FAM_ID);

    // The entry is ready in the proposed memory section. Just move it.
    // First lock the memory
    assert(is_locked != NULL);
    while (write_lock(WRITE, host_id) != true) {
        // wait until the lock is acquired
    }

    // Now, move the proposed update section to the actual table. Where is the
    // table at rn?
    entry_t *proposal = get_proposed_entry();
    // the proposal has a domain, which IDK yet. The proposal is based on the
    // range and the context. The doamin is assigned by the framework.


    if (verbose) {
        // what is the proposed entry? It is defiend in the proposal!
        // FIXME: check only for the valid number of processes!!!
        info("Proposed entry by host %d: pid: %u, permission: %d, "
                "shared_mask: %d, is_valid: %d\n",
                proposal->domain.context[0].host_id,
                proposal->domain.context[0].process_id[0],
                proposal->permission, proposal->shared_mask,
                proposal->is_valid);
    }
    // TODO: Ignore voting in this version. Fix it later.
    *count = MAX_PARTICIPANT_COUNT;
    populate_table_entry(host_id, *proposal);
    // make sure that this host can access the permission table.
    // set_participant_count(get_participant_count() + 1);
    participant_host_ids[host_id] = true;

    // reset the proposed entry
    set_valid_bit();
    // proposal->is_valid = false;



    // TODO: See if this request can be merged?

    // unlock the lock
    unlock(host_id);
    // print this to the user if they want. 
    // Notify the user about the successful update.
    if (verbose) {
        info("Host %d successfully updated the permission table.\n", host_id);
    }

    // make sure to notify the FAM that the update was successful.
    return true;
}

// The other function needed to remove an entry from the permission table.
bool remove_proposed_entry(int host_id) {
    // Again, make sure that this is the FAM and no one else.
    assert(host_id == FAM_ID);

    // The entry is ready in the proposed memory section. Just move it.
    // First lock the memory
    while (write_lock(WRITE, host_id) != true) {
        // wait until the lock is acquired
    }
    // The entry is ready in the update section.
    entry_t *proposal = get_proposed_entry();
    // TODO: Ignore voting in this version. Fix it later.
    entry_t *head = get_permission_table(host_id);

    // TODO: See if this request can be merged?
    // Now, search the host and the context_id to match an entry and remove the
    // entry.
    // for (size_t i = 0; i < get_permission_table_count(); i++) {
    //     if (*head[i]->domain_id == proposal->domain_id)
    // }
    // FIXME: Lazy implementation????
    // I am using a valid bit to do a lazy implementation for now.
    // proposal->is_valid = 0;

    // unlock the lock
    unlock(host_id);

    return true;
}

int get_permission_table_count() {
    return permission_table_count;
}

int get_permission_table_index() {
    return *permission_table_index;
}

void set_permission_table_count(int table_count) {
    // make sure that the table count is bounded!
    assert(table_count >= 0 && table_count <= MAX_PARTICIPANT_COUNT);
    permission_table_count = table_count;
}

void set_permission_table_index(int host_id, int table_index) {
    // make sure that the table index is bounded!
    assert(table_index >= 0 && table_index <= MAX_PARTICIPANT_COUNT);
    assert(host_id == FAM_ID);
    *permission_table_index = table_index;
}

// Here is the user interface for getting the get_is_locked function.
void print_lock_info() {
    info("is_locked: %d, who_locked: %d, participant_count: %d\n",
            get_is_locked(), get_who_locked(), get_participant_count());
}

void print_proposed_update(int host_id) {
    // This is a utility function to print the proposed update.
    // security feature: only the FAM can see the proposed update.
    if (host_id != FAM_ID) {
        fatal("Host %d is not allowed to see the proposed update!", host_id);
    }
    entry_t *entry = get_proposed_entry();
    info("Proposed entry by host %d: process_id: %d, permission: %d, "
            "shared_mask: %d, is_valid: %d\n",
            host_id, entry->domain.context[0].process_id[0],
            entry->permission,
            entry->shared_mask, entry->is_valid);
}

void print_vote_count(int host_id) {
    // This is a utility function to print the vote count.
    // security feature: only the FAM can see the vote count.
    if (host_id != FAM_ID) {
        fatal("Host %d is not allowed to see the vote count!", host_id);
    }
    info("Vote count: %d\n", get_count());
}

void print_single_entry(entry_t *entry) {
    
        // The entry can have multiple sub-entries!
        printf(" %d \t\t", entry->domain.id);

        for (int j = 0; j < entry->domain.valid_contexts; j++) {
            // print the host_id and process_id
            printf("[(%d, [", entry->domain.context[j].host_id);
            for (int k = 0; k < entry->domain.context[j].valid_processes; k++) {
                printf("%u, ", entry->domain.context[j].process_id[k]);
            }
            printf("]), ");
        }
        printf("] \t");

        printf("(%p, %lu) \t", entry->range.start, entry->range.size);
        printf(" %d \t %d \t %d \t %d \n",
               entry->permission, entry->shared_mask,
               entry->is_dirty, entry->is_valid);

}

void print_permission_table(int host_id) {
    // This is a utility function to print the permission table.
    // security feature: only the FAM can see the permission table.
    if (host_id != FAM_ID) {
        // if I am a participant host with valid permissions, I should be able
        // to see the permission table.
        // handle overflow!
        if (participant_host_ids[host_id] == false && host_id < get_participant_count()) {
            fatal("Host %d is not allowed to see the permission table!", host_id);
        }
    }
    printf("Permission table has %d entries:\n", get_permission_table_index());
    printf("----------------------------------------------------"
        "--------------------------------------------------\n");
    printf(" domain_id \t [(host_id, [pid])] \t (start, end) \t "
        "permission \t shared_mask \t is_dirty \t is_valid\n");
    printf("----------------------------------------------------"
        "--------------------------------------------------\n");

    entry_t *head = get_permission_table(host_id);
    for (int i = 0; i < get_permission_table_index(); i++) {
        // The entry can have multiple sub-entries!
        printf(" %d \t\t", head[i].domain.id);

        for (int j = 0; j < head[i].domain.valid_contexts; j++) {
            // print the host_id and process_id
            printf("[(%d, [", head[i].domain.context[j].host_id);
            for (int k = 0; k < head[i].domain.context[j].valid_processes;
                                                                        k++) {
                printf("%u, ", head[i].domain.context[j].process_id[k]);
            }
            printf("]), ");
        }
        printf("] \t");

        printf("(%p, %lu) \t", head[i].range.start, head[i].range.size);
        printf(" %d \t %d \t %d \t %d \n",
               head[i].permission, head[i].shared_mask,
               head[i].is_dirty, head[i].is_valid);
        printf("----------------------------------------------------"
            "--------------------------------------------------\n");
    }
}

context_t *create_context(int host_id, unsigned int *process_id,
        unsigned int valid_processes) {
    // This function creates a context for the user. This is used to create a
    // context for the user.
    if (host_id != FAM_ID) {
        // If I am not the FAM then I am bounded by the maximum participant.
        assert(host_id >= 0 && host_id < MAX_PARTICIPANT_COUNT);
    }
    // FIXME: We need another header specific for the FAM.
    // The context is initialized in the local memory before moving to the
    // permission table.
    context_t *context = (context_t *) malloc (sizeof(context_t));
    context->host_id = host_id;
    for (size_t i = 0 ; i < (size_t) valid_processes; i++) {
        context->process_id[i] = process_id[i];
    }
    context->valid_processes = valid_processes;
    return context;
}

// ------------------------- FAM starts here ------------------------------- //

void init_fam(int* start_address) {
    // This function initializes the FAM.
    // make sure the the initial region is mapped to null
    assert(monitor_region == NULL);
    set_permission_table_index(FAM_ID, 0);
    monitor_region = (entry_t *) &start_address[PROPOSED_UPDATE];
    // shared_region = (uint8_t *) start_address + PROPOSED_UPDATE;
    // *futex_flag = 0;
    assert(monitor_region != NULL);
}

void monitor_update_old(int host_id, int* start_address) {
    // This is the FAM's function that monitor and updates the permission table
    // and the lock information. This is called by the FAM to update the
    // permission table and the lock information.
    assert(host_id == FAM_ID);
    // This is an address!
    entry_t *old_entry = (entry_t *) &start_address[PROPOSED_UPDATE];
    while (true) {
        // monitor the proposed update section.
        monitor_and_wait(start_address + PROPOSED_UPDATE);
        if (old_entry != monitor_region) {
            // changes detected!
            info("changes detected in the proposed update section by host");
            // TODO: The voting is not implemented yet 
            if (get_count() > 0 || true) {
                // move this entry to the table
                if (move_proposed_entry(host_id) == true) {
                    info("Moved the proposed entry to the permission table");
                }
                else {
                    // FAM cannot be crashed at any point. If this happens, the
                    // entire memory needs to be wiped out to prevent memory
                    // leak!
                    fatal("Failed to move the proposed entry to the permission table");
                }
            }
        }
    }
}

void monitor_update(int host_id, int* start_address) {
    // This is the FAM's function that monitor and updates the permission table
    // and the lock information. This is called by the FAM to update the
    // permission table and the lock information.
    assert(host_id == FAM_ID);
    domains = 0;
    // This is an address!
    while (true) {
        // monitor the proposed update section.
        entry_t *old_entry = get_proposed_entry();
        printf("valid bit: %d \t entries %d\n", old_entry->is_valid, get_permission_table_index());
        if (old_entry->is_valid == 1) {
            // This means that the entry is  valid. This is a lazy
            // implementation.
            // FIXME: Lock is too slow
            // whoever locked the region is the one changing the entry.
            info("changes detected in the proposed update section by host %d",
                    get_who_locked());
            // move this entry to the permission table
            move_proposed_entry(host_id);
            // reset the proposed entry
            // old_entry->is_valid = 0; // reset the valid bit
            // if this is a new update, set the vote to high number
            // FIXME:
            // set_count(get_participant_count() / 2 + 1);
            set_count(1024);
            info("Moved the proposed entry to the permission table by host %d",
                    get_who_locked());
        }
        usleep(FAM_SLEEP); // sleep for a while to avoid busy waiting
    }
}

void unoptimized_monitor_and_wait(volatile void *addr) {
    // This is an unoptimized version of the monitor and wait function.
    // It uses the monitor and mwait instructions to wait for a change in the
    // memory location pointed by addr.
    fatal("NotImplementedError: unoptimized_monitor_and_wait is not implemented yet!");
}

void monitor_and_wait(volatile void *addr) {
 asm volatile (
 "mfence\n\t" // Ensure memory operations complete
 "monitor\n\t" // Set up monitoring on addr
 "mwait\n\t" // Wait until addr is written to
 :
 : "a"(addr), "c"(0), "d"(0)
 : "memory"
);
}
