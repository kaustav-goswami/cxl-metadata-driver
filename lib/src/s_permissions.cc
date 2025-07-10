#include "s_permissions.hh"

// define all the global variables that are used to manage the shared memory
// region. These are all undefined in the header file.
struct s_dmalloc_entry *global_addr_;
bool verbose = false;                    // verbose is set by the parent
                                 // function
int* is_locked;                  // assigning is locked as a variable
int* who_locked;                 // similar
int* participant_count;
int* participant_host_ids;
struct table_entry* proposed_update;
int* count;
struct table_entry* permission_table;
int permission_table_count;
int permission_table_index;


void
assign_all_global_variables(int* start_address, bool this_verbose) {
    // We don't need the host_id tp assign the global variables. Instead, we
    // need the start address of the mmapped region
    is_locked = &start_address[IS_LOCKED];
    who_locked = &start_address[WHO_LOCKED];
    participant_count = &start_address[PARTICIPANT_COUNT];
    participant_host_ids = &start_address[PARTICIPANT_HOST_IDS];
    proposed_update = 
                        (struct table_entry*) &start_address[PROPOSED_UPDATE];
    count = &start_address[COUNT];
    permission_table =
                    (struct table_entry*) &start_address[COUNT + sizeof(int)];
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
write_proposed_entry(int host_id, struct table_entry *entry) {
    // This is always written into a fixed region of the memory. This cannot be
    // exposed as it is to the end user. Make sure to update this somewhere and
    // make it like an API.
    proposed_update = entry;
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
bool create_and_wait_to_get_access(int host_id, struct table_entry *entry) {
    // This is the most complicated function IMHO
    if (write_proposed_entry(host_id, entry) == true) {
        // wait until voting is done.
        while (*count <= get_participant_count() / 2) {
            // wait until voting is done. How????
            // TODO: Add a timeout condition.
        }
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
struct table_entry* get_proposed_entry() {
    return proposed_update;
}

void set_proposed_entry(struct table_entry* entry) {
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

int reset_count() {
    *count = 0;
}

struct table_entry* get_permission_table(int host_id) {
    if (participant_host_ids[host_id] == true)
        return permission_table;
    else {
        // This host ID does not have permission to access the permission table
        fatal("Host %d does not have permissions to read the permissions",
                host_id);
        return NULL; // unreachable code
    }
}

// All the getter functions should work now.
int get_is_locked() {
    return *is_locked;
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
    while (write_lock(WRITE, host_id) != true) {
        // wait until the lock is acquired
    }

    // Now, move the proposed update section to the actual table. Where is the
    // table at rn?
    struct table_entry *proposal = get_proposed_entry();
    // TODO: Ignore voting in this version. Fix it later.
    struct table_entry *head = get_permission_table(host_id);

    // TODO: See if this request can be merged?
    head[get_permission_table_index()] = *proposal;
    // increment the counter of the index.
    set_permission_table_index(get_permission_table_index() + 1);

    // unlock the lock
    unlock(host_id);
    // print this to the user if they want. NVM

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
    // The enty is readu in the update section.
    struct table_entry *proposal = get_proposed_entry();
    // TODO: Ignore voting in this version. Fix it later.
    struct table_entry *head = get_permission_table(host_id);

    // TODO: See if this request can be merged?
    // Now, serach the host and the context_id to match an entry and remove the
    // entry.
    // for (size_t i = 0; i < get_permission_table_count(); i++) {
    //     if (*head[i]->domain_id == proposal->domain_id)
    // }
    // FIXME: Lazy implementation????
    // I am using a valid bit to do a lazy implementation for now.
    proposal->is_valid = 0;

    // unlock the lock
    unlock(host_id);

    return true;
}

int get_permission_table_count() {
    return permission_table_count;
}

int get_permission_table_index() {
    return permission_table_index;
}

void set_permission_table_count(int table_count) {
    // make sure that the table count is bounded!
    assert(table_count >= 0 && table_count <= MAX_PARTICIPANT_COUNT);
    permission_table_count = table_count;
}

void set_permission_table_index(int table_index) {
    // make sure that the table index is bounded!
    assert(table_index >= 0 && table_index <= MAX_PARTICIPANT_COUNT);
    permission_table_index = table_index;
}
