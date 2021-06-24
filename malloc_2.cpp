#include <unistd.h>

/**
 * This is a metadata used to manage the allocated blocks in a linked list
 */
struct MallocMetaData    {
    size_t size;
    bool is_free;
    MallocMetaData* next;
    MallocMetaData* prev;
};
/**
 * This is a class that handles the meta data list of blocks
 */
class BlockMetaDataList {
public:
    MallocMetaData* head;
    size_t num_free_bytes;
    size_t num_free_blocks;
    size_t num_occupied_bytes;
    size_t num_occupied_blocks;
    bool isEmpty();
};

BlockMetaDataList meta_list;

void* smalloc(size_t size) {
    if(meta_list.isEmpty()) {

    }
}



void* scalloc(size_t num, size_t size) {}


void sfree(void* p) {}

void* srealloc(void* oldp, size_t new_size) {}



