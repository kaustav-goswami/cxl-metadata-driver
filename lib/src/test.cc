// Sometimes I feel like I don't even know C

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "dmalloc.h"

typedef struct internal internal_t;
typedef struct entry entry_t;

struct internal {
    int a;
    int b;
};

struct entry {
    int id;
    internal_t internal;
};

std::size_t index = 0;
entry_t *global_head = NULL;

std::size_t get_index() {
    return index;
}

bool increment_index() {
    index++;
    return true;
}

bool decrement_index() {
    if (index > 0) {
        index--;
        return true;
    }
    return false;
}

entry_t* get_head() {
    return global_head;
}

std::size_t allocate_new_entry(int *start_addr) {
    // returns the index of the new entry
    // allocate memory for the next entry
    entry_t *head = get_head();
    if (head == NULL) {
        // create the initial entry
        global_head = (entry_t *) &start_addr[get_index()];
    }
    else {
        // head is already allocated, extend the table!
        head = (entry_t *) &start_addr[get_index()];
    }
    increment_index();
    return get_index() - 1; // return the index of the new entry
}

void create_entry(std::size_t index, int id, internal_t internal) {
    // memory is already allocated, just set the values
    entry_t *head = get_head();
    head[index].id = id;
    head[index].internal = internal;
}

int main() {
    // This is a test function to check the structure of the code.
    // The purpose of this code is to test the structure of the code and make
    // sure that the code is working as expected.

    int* start = shmalloc(1, 0);

    munmap_memory(1, 1, 0); // reset the memory as the root user is done.

    printf("cleaning up the memory...\n");

    // Create an entry with the allocated index
    for (int i = 0; i < 10; i++) {
    // Allocate a new entry
        std::size_t new_index = allocate_new_entry(start);
        create_entry(new_index, i, (internal_t){i * 10, i * 20});
    }

    // Print the entry
    entry_t *head = get_head();

    for (std::size_t i = 0; i < get_index(); i++) {
        printf("Entry at index %zu: id = %d, internal.a = %d, internal.b = %d\n",
               i, head[i].id, head[i].internal.a, head[i].internal.b);
    }

    return 0;
}