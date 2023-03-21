# Memory-Allocator-C

A memory manager project developed for the "Operating Systems" classes which contains implementations of functions such as: malloc, calloc, realloc.

# USAGE
```
#include "heap.h"

int main() {
    int status = heap_setup();
    if(status != 0)
        return 1;

    int *ptr = heap_malloc(510 * sizeof(int));
    if(ptr == NULL)
        return 2;

    heap_free(ptr);
    heap_clean();
    return 0;
}
```
