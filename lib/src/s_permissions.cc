#include "s_permissions.hh"

void
assign_all_global_variables(int* start_address) {
    // We don't need the host_id tp assign the global variables. Instead, we
    // need the start address of the mmapped region
    int* is_locked = &start_address[IS_LOCKED];
    int* who_locked = &start_address[WHO_LOCKED];
    int* participant_count = &start_address[PARTICIPANT_COUNT];
    int* participant_host_ids = &start_address[PARTICIPANT_HOST_IDS];
    struct table_entry* proposed_update = 
                        (struct table_entry*) &start_address[PROPOSED_UPDATE];
    int* count = &start_address[COUNT];
    struct table_entry* permission_table =
                    (struct table_entry*) &start_address[COUNT + sizeof(int)];

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
    // This is always written into a fixed region of the memory
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
bool create_and_wait_to_get_access(int host_id, struct table_entry entry) {
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

// Here are the utility setter and getter functions for the flat memory range.
int get_is_locked() {
    // call get_lock_status
    // TODO: Create a warn function!
    return get_lock_status();
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

struct table_entry* get_permission_table(int host_id);

// All the getter functions should work now.
int get_is_locked() {
    return *is_locked;
}
