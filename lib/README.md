# Space-Control Library

This is the supplementary OS-side changes to enable space-control in a full system environment.
The goal is to have the minimal changes to the user programs to assign permissions.
This makes the design better than mondrian etc. where each malloc needs to be assigned with some permissions per domain.

## Architecture

Here is a simple explanation of the library.
```
Input: the user initializes the shared memory region onto it's own virtual address space and then requests *physical address ranges* to access parts of the shared memory.
Output: A valid pointer to a shared memory region in the OS and the hardware enforces physical address' access control.
```


The user initializes the memory via:
`secure_init(size, host_id, permission)` which returns the following structure.
```c
struct addr {
    // A pointer to the start of the entire region
    int* start_address;
    // A pointer to the data segment of the region. Is NULL if the permission assignemnt fails
    int* data_start_address;
    // FIXME:
    // Requested permissions: 0 -> no 1 -> read only 2 -> write only 3 -> r/w 
    int permissions;
};
```

Then the user request to read/write parts of the shared memory via:
`create_entry(start, end, permission, host_id, process_id)`
and then proposing this update into the permission table via
`write_proposed_entry(host_id, entry, create/remove)`

The FAM is responsible for allowing new hosts, which can be replaced by a consensus mechanism
`vote_entry(host_id, vote)`

The implementation is done by the following files (clean separation):
```
s_dmalloc.hh    -> Stores all the initialization and allocation stuff
s_permissions.hh  -> Stores all the permission stuff
```

The `start_address` will not be NULL if the mmap is successful.
The `data_start_address` can be NULL if the user doesn't get permission to read or write into the shared memory region. This is unlikely as the OS will be oblivious to the actual permissions.

