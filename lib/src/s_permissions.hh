#ifndef __S_PERMISSIONS_HH__
#define __S_PERMISSIONS_HH__

/* This promgram manages a flat memory range in a distributed manner. */

/* The goal of the project is to create a a flat memory range in the remote
memory that is managed by the opearting systems of each host. Standard shared
memory protocols are used to write into this memory range. Here are the key
variables stored in this memory:

1. is_locked (4B) -- The current status of the region. This right now can be
                    in reading or writing mode.
                    TODO: To make sure that this is cache line aligned, this
                    variable is forced to be 64B.
2. who_locked (4B) -- The ID of the host who locked this memory. Defaults to
                    -1. TODO: Fix these things to use unsigned in the future.
3. particiant host count:N (4B) -- The number of particiant hosts sharing the
                    remote memory range. This is used to setup the permission
                    table.
4. participant host IDs (N * sizeof(int)) -- int of all the host IDs.
5. update proposal entry (sizeof(struct entry)) -- The suggested proposal by a
                    participant host to either UPDATE or REVOKE permissions of
                    a domain or a host.
6. count (4B)    -- Voting count. If assignment is done, then all votes are
                    counted. The number of votes required to create an entry in
                    the permission table, the entry is added into the
                    permission table. The count >= (N / 2) + 1. If the proposal
                    is to revoke permission, the vote of the victim host, whose
                    rights are taken away, will not be counted and the
                    count >= (N / 2).
5. permission table (variable) -- depending upon the number of particiant hosts
                                the table might vary. The table is indexed by
                                the device's physical address. If necessary, a
                                separate translation can be done by the CXL to
                                figure out this translation.

                                each entry has {
                                    unsigned int domain id; (max 2^7 domains)
                                    uint2_t permissions; (2 bits)
                                    int shared_mask;    (idk this rn)
                                };

Writing to the metadata is done by the host that has the write lock. This is
the critical section of the code. The host that has the write lock before
making changes to the table.

// FIXME: Number of entries in the permission table is not defined.
______________________________________ .. _____________________________________ .. ____
| is     | who    | participant | participant | proposed | vote  | table | entries    |
| locked | locked | count (N)   | host IDs    | update   | count | count |            |   
|________|________|_____________|_____ .. ____|__________|_______|_______|_____ .. ___|
<--4B---><--4B---><-----4B-----><----N*4B----><--entry--><--4B--><-- entries ->

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include "utility.hh"

// Typedefs are here. 
typedef uint64_t Addr;
typedef struct table_entry entry_t;
typedef struct context context_t;
typedef struct range range_t;
typedef struct domain domain_t;

// typedefs are needed for the struct. _t postfix is valid in this case as this
// is a still a driver code.
typedef struct s_dmalloc_entry dmalloc_t;

// Define the state enum. This is used to manage the reading and writing the
// metadata of the shared memory (aka the head).
enum states {
    IDLE,
    READ,
    WRITE
};

// A domain is defined as a combination of host_id and the process_id. Can also
// the cr3.
struct context {
    // The host ID of the host that owns this domain. Contexts can be merged as
    // multiple hosts can share the same range of memory. In that case, there
    // can be at most 1 table entry with a start, size,
    int host_id;
    // The process ID of the process that owns this domain. In this version,
    // we only support up to 8 processes per host. A VM ID can also be used
    // here, but this is not implemented yet.
    unsigned int process_id[8];
    // The number of valid processes must be defined! *sign* "C"
    unsigned int valid_processes;
};

// Multiple contexts should be able to be merged into a single domain.
struct domain {
    int id;     // a monotonic ID of the domain
    // XXX: This is a hardcoded value. This is the maximum number of
    // contexts that can be merged into a single domain.
    context_t context[8];
    unsigned int valid_contexts; // the number of valid contexts
};

struct range {
    // The start address of the range.
    int* start;
    size_t size;
};

// Define the structure of the permission table.
//
struct table_entry {
    // The context ID (aka PCID, set of PCID or a VMID)
    domain_t domain;
    range_t range;
    // The permission bit. This only needs to be either a read or a write. Page
    // permissions already has an execute bit, which we can simply ignore in
    // the hardware.
    bool permission;
    // The hosts who share this memory segment. 
    // TODO: Marked for deletion.
    int shared_mask;
    // Here are a couple of more bits to assign dirty and valid bits
    bool is_dirty;
    bool is_valid;
};

// Define the secured structure of the pointer to the mmaped regions
struct s_dmalloc_entry {
    int* start_address;
    int *data_start_address;
    int permissions;
};

// Hardcoded sections. Max number of participant hosts is 1024
// FIXME:
#define MAX_PARTICIPANT_COUNT 1024
// The head is hardcoded to 1G of memory.
#define TABLE_SIZE 0x40000000
// FAM needs to have a fixed ID
#define FAM_ID -2
// Define the offsets
#define IS_LOCKED 0
#define WHO_LOCKED sizeof(int)
#define PARTICIPANT_COUNT (WHO_LOCKED + sizeof(int))
#define PARTICIPANT_HOST_IDS (PARTICIPANT_COUNT + (MAX_PARTICIPANT_COUNT * sizeof(int)))
#define PROPOSED_UPDATE (PARTICIPANT_HOST_IDS + sizeof(entry_t))
#define COUNT (PROPOSED_UPDATE + sizeof(int))
#define INDEX_COUNT (COUNT + sizeof(int))

// The FAM needs to have a fixed size of the permission table.
#define UPDATE_RANGE (COUNT - PROPOSED_UPDATE)

// The FAM needs to sleep for a while to avoid busy waiting. This in
// microseconds.
#define FAM_SLEEP 1000000


// we need a bunch of global variables that manages the memory
extern dmalloc_t *global_addr_;   // Manages the start addresses*
extern bool verbose;                    // verbose is set by the parent
                                        // function
extern int* is_locked;                  // assigning is locked as a variable
extern int* who_locked;                 // similar
extern int* participant_count;
extern int* participant_host_ids;
extern entry_t* proposed_update;
extern int* count;
// This is a flat table of the permission entries.
extern entry_t* permission_table;
extern int permission_table_count;  // TOTAL entries
extern int *permission_table_index;

extern int domains;

// All the functions are declared here for better book-keeping!

// First we need to assign the variables to the flat memory region so that we
// can manage this memory better. This is the management structure defined in
// the beginning. Regardless whoami, the metadata always has read permissions
// to any new host.
void assign_all_global_variables(int* start_address, bool this_verbose);
// what's the current status of the lock?
int get_lock_status();
// We first need a lock writer.
bool write_lock(int action, int host_id);
// Then we need a data writer
bool write_proposed_entry(int host_id, entry_t *entry);
// We need a voter!
void vote_entry(int host_id, int vote);
// The FAM needs to reset the vote counter after moving the entry from the
// proposed section to the permission table
bool reset_vote();
// We need a function to create a new entry and wait until it gets approved by
// everyone.
bool create_and_wait_to_get_access(int host_id, entry_t *entry);
// Finally we need an unlock function to unlock the metadata.
bool unlock(int host_id);
// Here are the utility setter and getter functions for the flat memory range.
int get_is_locked();
void set_is_locked(int action);
int get_who_locked();
void set_who_locked(int host_id);
int get_participant_count();
// This must be set in consensus
void set_participant_count(int participant_count);
// Returns the integer stored at the index offset
int get_participant_host_ids(size_t index);
void set_participant_host_ids(size_t index);
entry_t* get_proposed_entry();
void set_proposed_entry(entry_t* entry);
int get_count();
// Sets an integer to vote. Is in between 0 and 1.
void set_count(int my_count);
// returns a pointer to the start of the permission table.
entry_t* get_permission_table(int host_id);
entry_t* create_new_permission_entry();

// A user-level API is needed to define the number of processes that can share
// the memory. This is used to create a context for the user.
context_t* create_context(int host_id, unsigned int* process_id,
                          unsigned int valid_processes);

// How is the main permission table managed? Ideally this needs to be managed
// by the hardware. IDK how but the secure trusted hardware needs to get
// triggered when the votes are more than the required number of votes. If we
// assume that the FAM node does the actual writes, then we can use the
// folowing functions to move or remove proposed entries into the actual table.
bool move_proposed_entry(int host_id);
bool remove_proposed_entry(int host_id);
// We need a couple of setter and getter for the FAM also
int get_permission_table_count();
int get_permission_table_index();
void set_permission_table_count(int table_count);
void set_permission_table_index(int table_index);

// Here are couple of more utility functions that are used by the user to get
// memory information with a more explainable way.
void print_lock_info();
void print_proposed_update(int host_id);
void print_vote_count(int host_id);
void print_permission_table(int host_id);

// FAM specific functions.
extern volatile entry_t* monitor_region;

extern volatile uint8_t *shared_region;
extern volatile int *futex_flag;

void init_fam(int* start_address);
void monitor_update(int host_id, int* start_address);
void monitor_and_wait(volatile void *addr);

// All function definitions are here!

#endif // __S_PERMISSIONS_HH__
