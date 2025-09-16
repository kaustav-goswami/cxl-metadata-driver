#ifndef __S_PERMISSIONS_HH__
#define __S_PERMISSIONS_HH__

/* This program manages a flat memory range in a distributed manner. */

/*
In this optimized version of the implementation, we use a 64B cache line
to store the metadata of the shared memory region.

* this is an entry *
process id (53b) -- The process ID % 53.

hosts' bit index (64 bits) -- binary of the hosts sharing the memory. Can
                                be upto 61 bits/hosts.

start_address (64 bits) -- 
end_address (64 bits) -- 

* THIS IS FLIPPED *
_________________________________________________________________
| start | end | valid bit |permissions | process_id | host_mask |
|______ |_____|___________|____________|____________|___________|
<-64b--><64b--><-----1b---><-----2b-----><---125b---><---256b--->

For similicity of this implementation, we are using regular numbers. I don't
want to do bit manipulation


___________________________________________________________________________
| start | end | valid bit | permissions | process_id | host_mask | unused |
|______ |_____|___________|_ ___________|____________|___________|________|
<-64b--><64b--><----8b---><-----32b-----><-----64b---><---64b---><-216b-->

unused bits -> 32 Bytes! We can add another 256 hosts or contexts!



-- 8B --

-- 24B --

update proposal entry (64 start + 64 end + 64) -- The suggested proposal by a
                participant host to either UPDATE or REVOKE permissions of
                a domain or a host.
% vote count -- lg(64) = 8 bits

% -- 64B -- %
%% Permission table follows
*/

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
__________________________________________________________________________________ .. ____
| is     | who    | participant | proposed | init. | vote  | table | unused | permission |
| locked | locked | count (N)   | update   | host  | count | count | space  | table      |   
|________|________|_____________|__________|_______|_______|_______|________|_____ .. ___|
<--4B---><--4B---><-----4B-----><----64B---><--4B--><--4B--><--4B--><--40B--><-- entries ->


Assuming the total remote mmemory size could be up to 1 TB, 16 GiB of memory
needs to be reserved for ACM.

For a more practical use-case, we'll reserve 1 GiB of the initial memory for
storing the permission table. This can maintain up to 16.7M permission entries.

We assume that the total number of hosts to be 256 - 1 (FAM) and each host can
have up to 128 processes sharing the remote memory.

* Limitations *
1. Since the testing infrastructure is purely shm based, even if addresses are
mapped to the same physical address, different virtual addresses are created
across different process.
populate_table_entry cannot merge synonyms in the OS-based testing cases.
The hardware version via gem5 *may not* have this problem.

-> While this does not downgrade any of the security issues, this does create
multiple entries in the permission table.

2. When a host tries to directly mmap into the shared memory range, the system
level version of the code (aka this driver) cannot prevent this. This has to
be tested and verified with dedicated hardware implemented via gem5.

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <math.h>
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

// Hardcoded sections. Max number of participant hosts is 1024
// FIXME:
#define MAX_PARTICIPANT_COUNT 64

#define UNUSED_SIZE 27

#define MAX_CONTEXT 64
#define MAX_PROCESSES 64
// The head is hardcoded to 1G of memory. This can store 16.7M entries. The
// first 128 Bytes is reserved for management.
// All hosts can read and write into this section. For unauthorized/hogging
// issues, it is assumed that the FAM kicks out any hoarders.
#define TABLE_SIZE 0x40000000
#define MAX_TABLE_ENTRIES 16777214

// FAM needs to have a fixed ID. Keep this as 0 or max. This is important.
#define FAM_ID MAX_PARTICIPANT_COUNT - 1
// there are no offsets here, just binary bits. The entry size if 64 BYTES!
// 64 bits per uint8_t
// #define IS_LOCKED   0b1000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000
// #define WHO_LOCKED  0b0111_1111_1000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000
// #define PERMISSIONS 0b0000_0000_0110_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000
// #define PID         0b0000_0000_0001_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111
// #define HOST_MASK   0b1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111
// #define START       0b1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111
// #define END         0b1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111
// #define UPDATE      0b1000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000
// #define VOTE_COUNT  0b0111_1111_1110_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000
// using 49 bits as of now.

// #define START       0b1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111
// #define END         0b1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111
// #define PERMISSIONS 0b1100_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000_0000
// #define PID         0b0011_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111
// #define HOST_MASK   0b1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111

#define PID_MASK        0b0011111111111111111111111111111111111111111111111111
#define PERMISSION_MASK 0b1100000000000000000000000000000000000000000000000000
#define FULL_MASK       0b1111111111111111111111111111111111111111111111111111

#define START_ADDRESS 0

#define IS_LOCKED 0
#define WHO_LOCKED (IS_LOCKED + sizeof(int))
#define PARTICIPANT_COUNT (WHO_LOCKED + sizeof(int))
// #define PARTICIPANT_HOST_IDS (PARTICIPANT_COUNT + sizeof(int))
#define PROPOSED_UPDATE (PARTICIPANT_COUNT + sizeof(int))
#define INITIATOR (PROPOSED_UPDATE + sizeof(entry_t))

#define COUNT (PROPOSED_UPDATE + sizeof(int))
#define INDEX_COUNT (COUNT + sizeof(int))
#define PERMISSION_TABLE (INDEX_COUNT + sizeof(int))

// The FAM needs to have a fixed size of the permission table.
#define UPDATE_RANGE (COUNT - PROPOSED_UPDATE)

// The FAM needs to sleep for a while to avoid busy waiting. This in
// microseconds.
#define FAM_SLEEP 1000000

// Define the state enum. This is used to manage the reading and writing the
// metadata of the shared memory (aka the head).
enum states {
    IDLE,
    READ,
    WRITE
};

// In this version, we don;t need to explicitly define a context.

/* Obsolete version of the code 
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
    unsigned int process_id[MAX_PROCESSES];
    // The number of valid processes must be defined! *sign* "C"
    unsigned int valid_processes;
};

// Multiple contexts should be able to be merged into a single domain.
struct domain {
    int id;     // a monotonic ID of the domain
    // XXX: This is a hardcoded value. This is the maximum number of
    // contexts that can be merged into a single domain.
    context_t context[MAX_CONTEXT];
    unsigned int valid_contexts; // the number of valid contexts
};

struct range {
    // The start address of the range in virtual addressing.
    int* vstart;
    // Physical address
    Addr pstart;
    // this size needs to be bytes.
    size_t size;
};
*/

// Define the structure of the permission table. this is the optimized version
// of the permission table, which needs to be bit manipulated. This can be upto
// 64 Bytes AKA a cache line size.

// _________________________________________________________________
// | start | end | valid bit |permissions | process_id | host_mask |
// |______ |_____|___________|____________|____________|___________|
// <-64b--><64b--><-----1b---><-----2b-----><---125b---><---256b--->
// <--------------------------- 64 Bytes -------------------------->

struct table_entry {
    Addr start;             // 64 bits
    Addr end;               // 64 bits

    bool is_valid;                 // 8 bits
    int permission;             // 32 bits

    uint64_t pid_mask;          // 64 bits
    uint64_t host_mask;         // 64 bits

    uint8_t unused[UNUSED_SIZE];         // 216 bits
};

// Define the secured structure of the pointer to the mmaped regions. This
// structure deals purely in virtual addresses.
struct s_dmalloc_entry {
    int* start_address;
    // Metadata is located before the permission table.
    int *data_start_address;
    int permissions;
};


// we need a bunch of global variables that manages the memory
extern dmalloc_t *global_addr_;   // Manages the start addresses*
extern bool verbose;                    // verbose is set by the parent
                                        // function
extern int* is_locked;                  // assigning is locked as a variable
extern int* who_locked;                 // similar
extern int* participant_count;
// extern int* participant_host_ids;
extern entry_t* proposed_update;
extern int* initiator;
extern int* count;
// This is a flat table of the permission entries.
extern entry_t* permission_table;
extern int permission_table_count;  // TOTAL entries
extern int *permission_table_index;

extern int domains;

// All the functions are declared here for better book-keeping!

// -------------------- initialization ------------------------------------- //
// First we need to assign the variables to the flat memory region so that we
// can manage this memory better. This is the management structure defined in
// the beginning. Regardless whoami, the metadata always has read permissions
// to any new host.
void assign_all_global_variables(int* start_address, bool this_verbose);

// -------------------------------- lock ----------------------------------- //
// what's the current status of the lock?
int get_lock_status();
// We first need a lock writer.
bool write_lock(int action, int host_id);
// Finally we need an unlock function to unlock the metadata.
bool unlock(int host_id);
// Here are the utility setter and getter functions for the flat memory range.
int get_is_locked();
void set_is_locked(int action);
int get_who_locked();
void set_who_locked(int host_id);

// ------------------------------ entry management ------------------------- //
// Then we need a data writer. this function writes a given entry to the
// `proposed_update` section
bool write_proposed_entry(int host_id, entry_t *entry);
// Returns the proposed entry section.
entry_t* get_proposed_entry();
// void set_proposed_entry(entry_t* entry);

// ----------------------------- table management -------------------------- //
// gets the permission table's head. I DONT UNDERSTAND THIS FUNCTION.
void allocate_table();
// We need a couple of setter and getter for the FAM also
void populate_table_entry(int host_id, entry_t proposal);

// --------------------------- consensus mechanism ------------------------- //
// We need a voter!
void vote_entry(int host_id, int vote);
// The FAM needs to reset the vote counter after moving the entry from the
// proposed section to the permission table
bool reset_vote();
// TODO: Figure out why are there two functions doing the same thing?
void reset_count();
// We need a function to create a new entry and wait until it gets approved by
// everyone.
// bool create_and_wait_to_get_access(int host_id, entry_t *entry);
int get_participant_count();
// This must be set in consensus
void set_participant_count(int participant_count);
// Returns the integer stored at the index offset
// int get_participant_host_ids(size_t index);

// void set_participant_host_ids(size_t index);

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
entry_t *create_entry(Addr start, Addr end, int permission, int host_id, unsigned int process_id)

// How is the main permission table managed? Ideally this needs to be managed
// by the hardware. IDK how but the secure trusted hardware needs to get
// triggered when the votes are more than the required number of votes. If we
// assume that the FAM node does the actual writes, then we can use the
// folowing functions to move or remove proposed entries into the actual table.
bool move_proposed_entry(int host_id);
// TODO: Marked for deletion
bool remove_proposed_entry(int host_id);
bool remove_table_entry(int host_id, entry_t proposal);

int get_permission_table_count();
int get_permission_table_index();
void set_permission_table_count(int table_count);
void set_permission_table_index(int host_id, int table_index);

// Here are couple of more utility functions that are used by the user to get
// memory information with a more explainable way.
void print_lock_info();
void print_proposed_update(int host_id);
void print_vote_count(int host_id);
void print_permission_table(int host_id);
void print_single_entry(entry_t* entry);

// FAM specific functions.
extern volatile entry_t* monitor_region;

extern volatile uint8_t *shared_region;
extern volatile int *futex_flag;

void init_fam(int* start_address);
void monitor_update(int host_id, int* start_address);
void monitor_and_wait(volatile void *addr);

// All function definitions are here!
// For the cache line version, there are a couple of bitoperation functions

// inline void set_start_address(size_t start) {

// }

#endif // __S_PERMISSIONS_HH__
