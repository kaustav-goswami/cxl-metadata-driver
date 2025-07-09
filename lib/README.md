# Space-Control Library

This is the supplementary OS-side changes to enable space-control in a full system environment.
The goal is to have the minimal changes to the user programs to assign permissions.
This makes the design better than mondrian etc. where each malloc needs to be assigned with some permissions per domain.

## Architecture

Here is a simple explanation of the library.
```
Input: the user requests a pointer to the shared memory region with its ID and permissions
Output: A valid pointer to a shared memory region.
```
The memory management is done by the secured dmalloc library implementation.
`s_dmalloc(host_id, size, permissions)` returns the following structure.
```c
struct addr {
    // A pointer to the start of the entire region
    int* start_address;
    // A pointer to the data segment of the region. Is NULL if the permission assignemnt fails
    int* data_start_address;
    // Host ID
    int host_id;
    // FIXME:
    // Requested permissions: 0 -> no 1 -> read only 2 -> write only 3 -> r/w 
    int permissions;
};
```

The implementation is done by the following files:
```
s_dmalloc.hh    -> Stores all the allocation stuff
s_permissions.hh  -> Stores all the permission stuff
```

The `start_address` will not be NULL if the mmap is successful.
The `data_start_address` can be NULL if the user doesn't get permission to read or write into the shared memory region.

