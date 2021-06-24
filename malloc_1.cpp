#include <unistd.h>

/**
 * This is a Naive malloc - it doesnt do anything smart.
 * @param size
 * @return
 */
void* smalloc(size_t size) {
    if(size == 0 or size > 1e8) {
        return nullptr;
    }
    /* increasing heap area */
    void* data_address = sbrk(size);
    /* If sbrk() fails we return null */
    if(data_address == (void*)(-1)) {
        return nullptr;
    }
    /* Giving user a pointer to the newly allocated space. */
    return data_address;
}