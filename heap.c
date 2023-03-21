#include "heap.h"
#include "string.h"

struct memory_manager_t memory_manager;

int heap_setup(void) {
    memory_manager.memory_size = 0;
    memory_manager.number_of_chunks = 0;
    memory_manager.free_space = 0;
    memory_manager.memory_start = sbrk(0);
    memory_manager.first_memory_chunk = NULL;
    return 0;
}

void heap_clean(void) {
    struct memory_manager_t *debug = &memory_manager;
    if (debug) {};

    sbrk((long) -memory_manager.memory_size);
    memory_manager.first_memory_chunk = NULL;
    memory_manager.memory_start = NULL;
}

void *heap_malloc(size_t size) {
    if (size == 0 || heap_validate() != 0)
        return NULL;

    struct memory_chunk_t *curr_chunk = memory_manager.first_memory_chunk;
    struct memory_chunk_t *last_chunk = curr_chunk;
    struct memory_chunk_t *new_chunk = NULL;
    int alloc_new_chunk = 1;

    while (curr_chunk != NULL) {
        if (curr_chunk->free == 1 && size <= curr_chunk->possible_size) {
            new_chunk = curr_chunk;
            while (memory_manager.free_space < CHUNK_SIZE) {
                void *sbrk_res = sbrk(PAGE_SIZE);
                if (sbrk_res == (void *) -1) {
                    if (last_chunk != NULL) {
                        last_chunk->next = NULL;
                        checksum_set(last_chunk);
                    }
                    return NULL;
                }
                memory_manager.memory_size += PAGE_SIZE;
                memory_manager.free_space += PAGE_SIZE;
            }
            alloc_new_chunk = 0;
            break;
        }
        last_chunk = curr_chunk;
        curr_chunk = curr_chunk->next;
    }
    if (alloc_new_chunk) {
        if (memory_manager.number_of_chunks == 0) {
            curr_chunk = (struct memory_chunk_t *) memory_manager.memory_start;
            memory_manager.first_memory_chunk = curr_chunk;
            new_chunk = curr_chunk;
        } else {
            new_chunk = (void *) ((char *) last_chunk + sizeof(struct memory_chunk_t) + last_chunk->size +
                                  FENCE_SIZE * 2);
            last_chunk->next = new_chunk;
            checksum_set(last_chunk);
        }
        // Preparing space for a new element
        while (memory_manager.free_space < CHUNK_SIZE) {
            void *sbrk_res = sbrk(PAGE_SIZE);
            if (sbrk_res == (void *) -1) {
                if (last_chunk != NULL) {
                    last_chunk->next = NULL;
                    checksum_set(last_chunk);
                }
                return NULL;
            }
            memory_manager.memory_size += PAGE_SIZE;
            memory_manager.free_space += PAGE_SIZE;
        }

        // Adding a new element
        new_chunk->next = NULL;
        new_chunk->prev = last_chunk;
        new_chunk->possible_size = size;
    }
    new_chunk->size = size;
    new_chunk->free = 0;
    fences_set(new_chunk);
    memory_manager.free_space -= CHUNK_SIZE;
    memory_manager.number_of_chunks++;
    // Checksum setting
    checksum_set(new_chunk);

    return (void *) ((char *) new_chunk + sizeof(struct memory_chunk_t) + FENCE_SIZE);
}

enum pointer_type_t get_pointer_type(const void *const pointer) {
    if (pointer == NULL) return pointer_null;
    if (heap_validate() != 0) return pointer_heap_corrupted;
    if (memory_manager.number_of_chunks == 0) return pointer_unallocated;

    struct memory_chunk_t *curr_chunk = memory_manager.first_memory_chunk;

    while (curr_chunk != NULL) {
        if ((void *) ((char *) curr_chunk + sizeof(struct memory_chunk_t) + FENCE_SIZE) == pointer) {
            if (curr_chunk->free == 1) return pointer_unallocated;
            return pointer_valid;
        }
        if ((void *) ((char *) curr_chunk) <= pointer
            &&
            ((void *) ((char *) curr_chunk + sizeof(struct memory_chunk_t) + curr_chunk->size + 2 * FENCE_SIZE) >=
             pointer
            )) {
            if (curr_chunk->free == 1) return pointer_unallocated;
            if ((void *) ((char *) curr_chunk + sizeof(struct memory_chunk_t)) > pointer)
                return pointer_control_block;
            if ((void *) ((char *) curr_chunk + sizeof(struct memory_chunk_t) + FENCE_SIZE) >= pointer)
                return pointer_inside_fences;
            if ((void *) ((char *) curr_chunk + sizeof(struct memory_chunk_t) + FENCE_SIZE + curr_chunk->size) >
                pointer)
                return pointer_inside_data_block;
            if ((void *) ((char *) curr_chunk + sizeof(struct memory_chunk_t) + 2 * FENCE_SIZE + curr_chunk->size) >
                pointer)
                return pointer_inside_fences;
        }
        curr_chunk = curr_chunk->next;
    }
    return pointer_unallocated;
}

void *heap_calloc(size_t number, size_t size) {
    struct memory_chunk_t *result = heap_malloc(number * size);
    if (result == NULL) return NULL;
    memset((char *) result, 0, number * size);
    return result;
}

void *heap_realloc(void *memblock, size_t count) {
    if (heap_validate() != 0) return NULL;
    if (memory_manager.memory_start == NULL) return NULL;
    if (memblock == NULL) return heap_malloc(count);
    if (count == 0) {
        heap_free(memblock);
        return NULL;
    }

    struct memory_chunk_t *curr_chunk = memory_manager.first_memory_chunk;
    while (curr_chunk != NULL) {
        if ((void *) ((char *) curr_chunk + sizeof(struct memory_chunk_t) + FENCE_SIZE) == memblock) {
            break;
        }
        curr_chunk = curr_chunk->next;
    }

    if (curr_chunk == NULL) return NULL;


    if (count < curr_chunk->size) {
        curr_chunk->size = count;
        fences_set(curr_chunk);
        checksum_set(curr_chunk);
        return memblock;
    }

    if (count > curr_chunk->size) {
        if (curr_chunk->next == NULL) {
            while (memory_manager.free_space < count - curr_chunk->size) {
                void *sbrk_res = sbrk(PAGE_SIZE);
                if (sbrk_res == (void *) -1) return NULL;
                memory_manager.memory_size += PAGE_SIZE;
                memory_manager.free_space += PAGE_SIZE;
            }
            curr_chunk->size = count;
            memory_manager.free_space -= count;
            curr_chunk->possible_size = curr_chunk->size;
            fences_set(curr_chunk);
            checksum_set(curr_chunk);
            return memblock;
        }
        if (curr_chunk->next->free == 1) {
            int a = curr_chunk->next->possible_size + sizeof(struct memory_chunk_t) + 2 * FENCE_SIZE;
            int b = count - curr_chunk->possible_size;
            if (a >= b) {
                curr_chunk->next = curr_chunk->next->next;
                curr_chunk->size = count;
                curr_chunk->possible_size = curr_chunk->size;
                fences_set(curr_chunk);
                checksum_set(curr_chunk);
                return memblock;
            } else {
                struct memory_chunk_t *new_chunk = heap_malloc(count);
                while (memory_manager.free_space < count - curr_chunk->size) {
                    void *sbrk_res = sbrk(PAGE_SIZE);
                    if (sbrk_res == (void *) -1) return NULL;
                    memory_manager.memory_size += PAGE_SIZE;
                    memory_manager.free_space += PAGE_SIZE;
                }
                if (new_chunk == NULL) return NULL;
                memory_manager.free_space = 0;
                memcpy((void *) ((char *) new_chunk), (void *) ((char *) memblock), curr_chunk->size);
                heap_free(memblock);
                return new_chunk;
            }
        }

    }

    checksum_set(curr_chunk);
    return memblock;
}

void heap_free(void *memblock) {
    if (heap_validate() != 0) return;
    struct memory_chunk_t *curr_chunk = memory_manager.first_memory_chunk;
    if (curr_chunk == NULL) return;

    while (curr_chunk != NULL) {
        if ((void *) ((char *) curr_chunk + sizeof(struct memory_chunk_t) + FENCE_SIZE) == memblock) {
            curr_chunk->free = 1;
            memory_manager.number_of_chunks--;
            curr_chunk->size = curr_chunk->possible_size;
            checksum_set(curr_chunk);
            break;
        }
        curr_chunk = curr_chunk->next;
    }

    curr_chunk = memory_manager.first_memory_chunk;
    struct memory_chunk_t *next_chunk = curr_chunk->next;
    while (curr_chunk != NULL && next_chunk != NULL) {
        next_chunk = curr_chunk->next;
        if (next_chunk == NULL) break;
        if (curr_chunk->free == 1 && next_chunk->free == 1) {
            curr_chunk->size += next_chunk->size + 2 * FENCE_SIZE + sizeof(struct memory_chunk_t);
            curr_chunk->possible_size = curr_chunk->size;
            curr_chunk->next = next_chunk->next;
            if (next_chunk->next != NULL) {
                next_chunk->next->prev = curr_chunk;
                checksum_set(next_chunk->next);
            }
            fences_set(curr_chunk);
            checksum_set(curr_chunk);
            curr_chunk = memory_manager.first_memory_chunk;
            next_chunk = curr_chunk->next;

            continue;
        }
        curr_chunk = curr_chunk->next;
    }

    if (memory_manager.number_of_chunks == 0) {
        memory_manager.first_memory_chunk = NULL;
    }

}

size_t heap_get_largest_used_block_size(void) {
    if (heap_validate() != 0) return 0;
    size_t max = 0;
    struct memory_chunk_t *curr_chunk = memory_manager.first_memory_chunk;

    while (curr_chunk != NULL) {
        if (curr_chunk->size > max && curr_chunk->free == 0) {
            max = curr_chunk->size;
        }
        curr_chunk = curr_chunk->next;
    }
    return max;
}


int heap_validate(void) {
    if (memory_manager.memory_start == NULL)return 2;

    struct memory_chunk_t *curr_chunk = memory_manager.first_memory_chunk;
    int index = 1;
    int fence = -1;
    while (curr_chunk != NULL) {
        index++;
        if (curr_chunk == (void *) -1) return 2;
        // Checksum setting
        struct memory_chunk_t temp;
        memcpy(&temp, curr_chunk, sizeof(struct memory_chunk_t));
        checksum_set(&temp);
        if (temp.checksum != curr_chunk->checksum)
            return 3;
        if (curr_chunk->free == 0) {
            ////////FENCES CHECK//////////
            for (int i = 0; i < FENCE_SIZE; ++i)
                if (memcmp((char *) curr_chunk + sizeof(struct memory_chunk_t) + i, &fence, 1) != 0)
                    return 1;
            for (int i = 0; i < FENCE_SIZE; ++i)
                if (memcmp((char *) curr_chunk + sizeof(struct memory_chunk_t) + curr_chunk->size + FENCE_SIZE + i,
                           &fence, 1) != 0)
                    return 1;
            ////////FENCES CHECK//////////
        }

        curr_chunk = curr_chunk->next;
    }
    return 0;
}

void checksum_set(struct memory_chunk_t *chunk_t) {
    struct memory_chunk_t temp;
    memcpy(&temp, chunk_t, sizeof(struct memory_chunk_t));
    temp.checksum = 0;
    uint8_t *p = (uint8_t *) &temp;
    for (int i = 0; i < (int) sizeof(struct memory_chunk_t); ++i) {
        temp.checksum += *p++;
    }
    chunk_t->checksum = temp.checksum;
}

void fences_set(struct memory_chunk_t *chunk_t) {
    memset(((char *) chunk_t + sizeof(struct memory_chunk_t)), -1, FENCE_SIZE);
    memset(((char *) chunk_t + sizeof(struct memory_chunk_t) + chunk_t->size + FENCE_SIZE), -1, FENCE_SIZE);
}

