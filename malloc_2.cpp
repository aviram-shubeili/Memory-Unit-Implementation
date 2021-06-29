#include <unistd.h>
#include <cstring>
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
    MallocMetaData* tail;
    size_t num_free_bytes;
    size_t num_free_blocks;
    size_t total_bytes;
    size_t total_blocks;
    bool isEmpty() const;
    bool isSingleBlock() const;
    MallocMetaData *allocateBlock(size_t size);
    MallocMetaData* searchFreeBlock(size_t size);
    void freeBlock(MallocMetaData* p);
};
/**
 * Search for a free block in list that is compatible or allocates and inserts a new block to the end of the list
 * @param size
 * @return nullptr if there isnt a compatible block and sbrk() fails to add another block. (or size is 0 / bigger than 1e8)
 */
MallocMetaData * BlockMetaDataList::allocateBlock(size_t size) {
    if(size == 0 or size > 1e8) {
        return nullptr;
    }
    /* Check to see if there is a compatible free block */
    MallocMetaData* block = searchFreeBlock(size);
    if(block != nullptr) {
        block->is_free = false;
        /* Update stats */
        num_free_blocks--;
        num_free_bytes -= block->size;

        return block;
    }

    /* Allocating a new block */
    void* addr = sbrk(sizeof(MallocMetaData) + size);
    if(addr == (void*)(-1)) {
        return nullptr;
    }
    block = (MallocMetaData*)(addr);

    /* Inserting new block to end of the list */
        // empty list
    if(isEmpty()) {
        head = tail = block;
        block->prev = nullptr;
        block->next = nullptr;
    }
        // one block in list
    else if(isSingleBlock()) {
        block->prev = tail;
        head->next = block;
        block->next = nullptr;
        tail = block;
    }
        // more than one block in list
    else {
        tail->next = block;
        block->prev = tail;
        block->next = nullptr;
        tail = block;
    }
    block->size = size;
    block->is_free = false;
    /* Added a new block to list -- update stats */
    total_blocks++;
    total_bytes += size;
    return block;
}

/**
 * Search for a free block with size >= from requested size
 * @param size
 * @return nullptr if not found
 */
MallocMetaData *BlockMetaDataList::searchFreeBlock(size_t size) {
    MallocMetaData* ptr = head;
    while(ptr) {
        if(ptr->is_free and ptr->size >= size) {
            return ptr;
        }
        ptr = ptr->next;
    }
    /* Didnt find any */
    return nullptr;
}

void BlockMetaDataList::freeBlock(MallocMetaData *p) {
    /*If already free -> do nothing */
    if(p->is_free) {
        return;
    }
    p->is_free = true;
    num_free_blocks++;
    num_free_bytes += p->size;
}

bool BlockMetaDataList::isEmpty() const {
    return head == nullptr;
}

bool BlockMetaDataList::isSingleBlock() const {
    return head == tail;
}

BlockMetaDataList meta_list;

void* smalloc(size_t size) {
    MallocMetaData* block_metadata = meta_list.allocateBlock(size);
    if(block_metadata == nullptr) {
        return nullptr;
    }
    char* p1 = (char*)(block_metadata);
    void* p = (void*)(p1 + sizeof(MallocMetaData));
    return p;
}

void* scalloc(size_t num, size_t size) {
    MallocMetaData* block_metadata = meta_list.allocateBlock(size * num);
    if(block_metadata == nullptr) {
        return nullptr;
    }
    char* p1 = (char*)(block_metadata);
    void* block = (void*)(p1 + sizeof(MallocMetaData));
    /* Making a zeroes block */
    memset(block,0,size*num);
    return block;
}


void sfree(void* p) {
    if(p == nullptr) {
        return;
    }
    /* Accessing p's metadata */
    char* p1 = (char*)(p);
    MallocMetaData* p_metadata =(MallocMetaData*)(p1 - sizeof(MallocMetaData));
    /* Marking p as free (may have already been free) */
    meta_list.freeBlock(p_metadata);
}

void* srealloc(void* oldp, size_t new_size) {
    if(oldp == nullptr) {
        return smalloc(new_size);
    }
    char* p1 = (char*)(oldp);
    MallocMetaData* oldp_metadata =(MallocMetaData*)(p1 - sizeof(MallocMetaData));
    /* Can we use the same block? */
    if(oldp_metadata->size >= new_size and new_size != 0) {
        return oldp;
    }
    /* Find/Allocate an other block */
    void* newp = smalloc(new_size);
    /* If allocation failed return NULL and dont free oldp */
    if (newp == nullptr) {
        return nullptr;
    }
    memcpy(newp, oldp, oldp_metadata->size);
    /* Free old */
    sfree(oldp);

    return newp;
}

size_t _num_free_blocks() {
    return meta_list.num_free_blocks;
}

size_t _num_free_bytes() {
    return meta_list.num_free_bytes;
}

size_t _num_allocated_blocks() {
    return meta_list.total_blocks;
}

size_t _num_allocated_bytes() {
    return meta_list.total_bytes;
}

size_t _size_meta_data() {
    return sizeof(MallocMetaData);
}

size_t _num_meta_data_bytes() {
    return meta_list.total_blocks * _size_meta_data();
}
