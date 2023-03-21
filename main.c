#include <stdio.h>
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




