#ifndef UNTITLED13_HEAP_H
#define UNTITLED13_HEAP_H

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>


#define FENCE_SIZE 16
#define PAGE_SIZE 4096
#define CHUNK_SIZE (size + sizeof(struct memory_chunk_t) + FENCE_SIZE * 2)

enum pointer_type_t {
    pointer_null,
    pointer_heap_corrupted,
    pointer_control_block,
    pointer_inside_fences,
    pointer_inside_data_block,
    pointer_unallocated,
    pointer_valid
};

struct memory_manager_t {
    void *memory_start;
    size_t memory_size;
    size_t free_space;
    size_t number_of_chunks;
    struct memory_chunk_t *first_memory_chunk;
};

struct memory_chunk_t {
    struct memory_chunk_t *prev;
    struct memory_chunk_t *next;
    int free;
    size_t size;
    size_t possible_size;
    uint8_t checksum;
};

int heap_setup(void);

void heap_clean(void);

void *heap_malloc(size_t size);

void *heap_calloc(size_t number, size_t size);

void *heap_realloc(void *memblock, size_t count);

void heap_free(void *memblock);

size_t heap_get_largest_used_block_size(void);

enum pointer_type_t get_pointer_type(const void *const pointer);

int heap_validate(void);

void checksum_set(struct memory_chunk_t *chunk_t);

void fences_set(struct memory_chunk_t *chunk_t);


#endif //UNTITLED13_HEAP_H
