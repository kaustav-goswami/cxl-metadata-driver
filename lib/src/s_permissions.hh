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

______________________________________ .. _____________________________ .. ____
| is     | who    | participant | participant | proposed | count | entries    |
| locked | locked | count (N)   | host IDs    | update   |       |            |   
|________|________|_____________|_____ .. ____|__________|_______|_____ .. ___|
<--4B---><--4B---><-----4B-----><----N*4B----><--entry--><--4B--><-- entries ->

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "utility.hh"

// Typedefs are here. 
typedef uint64_t Addr;

// Define the state enum. This is used to manage the reading and writing the
// metadata of the shared memory (aka the head).
enum states {
    IDLE,
    READ,
    WRITE
};

// Define the structure of the permission table.
//
struct table_entry {
    // The context ID (aka PCID, set of PCID or a VMID)
    int domain_id;
    // The permission bit. This only needsto be either a read or a write. Papge
    // permissions already has an execute bit, which we can simply ignote in
    // the hardware.
    bool permission;
    // The hosts who share this memory segment.
    int shared_mask;
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
// Define the offsets
#define IS_LOCKED 0
#define WHO_LOCKED sizeof(int)
#define PARTICIPANT_COUNT (WHO_LOCKED + sizeof(int))
#define PARTICIPANT_HOST_IDS (PARTICIPANT_COUNT + (MAX_PARTICIPANT_COUNT * sizeof(int)))
#define PROPOSED_UPDATE (PARTICIPANT_HOST_IDS + sizeof(struct table_entry))
#define COUNT (PROPOSED_UPDATE + sizeof(int))

// we need a bunch of global variables that manages the memory
extern s_dmalloc_entry *global_addr_;   // Manages the start addresses*
extern bool verbose;                    // verbose is set by the parent
                                        // function
extern int* is_locked;                  // assigning is locked as a variable
extern int* who_locked;                 // similar
extern int* participant_count;
extern int* participant_host_ids;
extern struct table_entry* proposed_update;
extern int* count;
extern struct table_entry* permission_table;
extern int permission_table_count;
extern int permission_table_index;

// All the functions are declared here for better book-keeping!

// First we need to assign the variables to the flat memory region so that we
// can manage this memory better. This is the management structure defined in
// the beginning. Regardless whoami, the metadata always has read permissions
// to any new host.
void assign_all_global_variables(int* start_address);
// what's the current status of the lock?
int get_lock_status();
// We first need a lock writer.
bool write_lock(int action, int host_id);
// Then we need a data writer
bool write_proposed_entry(int host_id, struct table_entry entry);
// We need a voter!
void vote_entry(int host_id, int vote);
// The FAM needs to reset the vote counter after moving the entry from the
// proposed section to the permission table
bool reset_vote();
// We need a function to create a new entry and wait until it gets approved by
// everyone.
bool create_and_wait_to_get_access(int host_id, struct table_entry);
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
struct table_entry* get_proposed_entry();
void set_proposed_entry(struct table_entry* entry);
int get_count();
// Sets an integer to vote. Is in between 0 and 1.
void set_count(int my_count);
struct table_entry* get_permission_table(int host_id);

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

// All function definitions are here!

#endif // __S_PERMISSIONS_HH__
