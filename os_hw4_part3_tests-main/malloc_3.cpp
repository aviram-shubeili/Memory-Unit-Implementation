#include <unistd.h>
#include <cstring>
#include <cassert>
#include <iostream>
#include <sys/mman.h>

#define BIN_NUM 128
#define MMAP_BIN 128
#define COMBINED_LIST (-1)
#define WILDERNESS combined_list_tail

/**
 * This is a metadata used to manage the allocated blocks in a linked list
 */
struct MallocMetaData    {
    size_t size;
    bool is_free;
    MallocMetaData* next;
    MallocMetaData* prev;
    MallocMetaData* next_in_bin;
    MallocMetaData* prev_in_bin;
};
/**
 * This is a class that handles the meta data list of blocks
 */
class BlockMetaDataList {
public:
    /* BIN_NUM bins + 1 extra bin for mmapped  */
//    MallocMetaData* bins[BIN_NUM + 1];
    MallocMetaData* head[BIN_NUM + 1];
    MallocMetaData* tail[BIN_NUM + 1];
    MallocMetaData* combined_list_head;
    MallocMetaData* combined_list_tail;
    size_t num_free_bytes;
    size_t num_free_blocks;
    size_t total_bytes;
    size_t total_blocks;
    bool isEmpty(int i) const;
    bool isSingleBlock(int i) const;
    int getBinIndex(size_t size);
    MallocMetaData* allocateBlock(size_t size);
    MallocMetaData* searchFreeBlock(size_t size);
    void insertBlockToBinList(MallocMetaData* block);
    void removeFromBinList(MallocMetaData *block);
    void removeFromCombinedList(MallocMetaData *block);
    void insertToCombinedList(MallocMetaData* block);
    void insertAfterToCombinedList(MallocMetaData* left, MallocMetaData* new_block);
    void freeBlock(MallocMetaData* p);
    void occupyBlock(MallocMetaData* p);
    void mergeFreeBlocks(MallocMetaData* middle);
    void mergeLeftAndFree(MallocMetaData* left, MallocMetaData* middle);
    void mergeRightAndFree(MallocMetaData* middle, MallocMetaData* right);
    MallocMetaData *splitBlock(MallocMetaData *block, size_t first_block_size);
    MallocMetaData * expandAndOccupyWilderness(size_t size);

    void mergeRight(MallocMetaData *middle, MallocMetaData *right);

    void mergeLeft(MallocMetaData *left, MallocMetaData *middle);
};
/**
 * Search for a free block in list that is compatible or allocates and inserts a new block to the end of the list
 * @param size
 * @return nullptr if there is not a compatible block and sbrk() fails to add another block. (or size is 0 / bigger than 1e8)
 */
MallocMetaData* BlockMetaDataList::allocateBlock(size_t size) {
    if(size == 0 or size > 1e8) {
        return nullptr;
    }
    if(getBinIndex(size) == MMAP_BIN) {
        void* addr = mmap(NULL,sizeof(MallocMetaData) + size, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(addr == MAP_FAILED ) {
            return nullptr;
        }
        MallocMetaData* block = (MallocMetaData*)(addr);
        block->size = size;
        block->is_free = false;
        total_blocks++;
        total_bytes += size;
        return block;
    }
    /* **************** Small block **************** */
    /* **************** Small block **************** */

    /* A. Check to see if there is a compatible free block */
    MallocMetaData* block = searchFreeBlock(size);
    if(block != nullptr) {
        occupyBlock(block);
        MallocMetaData* splitted_block = splitBlock(block, size);
        return splitted_block;
    }
    /* B. Trying to expand wilderness */
    if(WILDERNESS and WILDERNESS->is_free) {
        return expandAndOccupyWilderness(size);
    }
    /* C. Allocating a new block */
    void* addr = sbrk(sizeof(MallocMetaData) + size);
    if(addr == MAP_FAILED) {
        return nullptr;
    }
    block = (MallocMetaData*)(addr);
    block->size = size;
    block->is_free = false;
    insertToCombinedList(block);
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
    int min_bin = getBinIndex(size);
    if(min_bin == MMAP_BIN) {
        // should never get here
        assert(false);
    }
    for (int i = min_bin; i < BIN_NUM ; ++i) {
        MallocMetaData* ptr = head[i];
        while(ptr) {
            if(/* // FixMe bin for free only ptr->is_free and */ ptr->size >= size) {
                return ptr;
            }
            ptr = ptr->next_in_bin;
        }
    }
    /* Didnt find any */
    return nullptr;
}

void BlockMetaDataList::freeBlock(MallocMetaData *p) {
    if(p->is_free) {
        return;
    }

//    std::cout << "Freeing p at size: " <<  p->size << "\n";
    int i = getBinIndex(p->size);
    if(i == MMAP_BIN) {
        munmap(p, sizeof(MallocMetaData) + p->size);
        return;
    }
    p->is_free = true;
    num_free_blocks++;
    num_free_bytes += p->size;
    insertBlockToBinList(p); // FixMe bin for free only
    mergeFreeBlocks(p);
}

bool BlockMetaDataList::isEmpty(int i) const {
    if(i == COMBINED_LIST) {
        return combined_list_head == nullptr;
    }
    return head[i] == nullptr;
}

bool BlockMetaDataList::isSingleBlock(int i) const {
    if(i == COMBINED_LIST) {
        return combined_list_head == combined_list_tail;
    }
    return head[i] == tail[i];
}

void BlockMetaDataList::insertBlockToBinList(MallocMetaData *block) {

    int i = getBinIndex(block->size);
    // should never get here
    assert(i != MMAP_BIN);
// empty list
    if(isEmpty(i)) {
        head[i] = tail[i] = block;
        block->prev_in_bin = nullptr;
        block->next_in_bin = nullptr;
    }
        // one block in list
    else if(isSingleBlock(i)) {
        /*  Insert at the tail */
        if(head[i]->size <= block->size) {
            block->prev_in_bin = tail[i];
            head[i]->next_in_bin = block;
            block->next_in_bin = nullptr;
            tail[i] = block;
        }
        else {
            /* Insert at the head  */
            block->prev_in_bin = nullptr;
            head[i]->prev_in_bin = block;
            block->next_in_bin = head[i];
            head[i] = block;
        }
    }
        // more than one block in list
    else {
        /* Find the first block smaller in size than me (or nullptr if none exist) */
        MallocMetaData* insert_before_me = head[i];
        while(insert_before_me and insert_before_me->size < block->size) {
            insert_before_me = insert_before_me->next;
        }
        /*Inserting in the middle of the list */
        if(insert_before_me) {

            if(insert_before_me == head[i]) {
                /* Insert at the head  */
                block->prev_in_bin = nullptr;
                head[i]->prev_in_bin = block;
                block->next_in_bin = head[i];
                head[i] = block;
            }
            /* READ THE FOLLOWING COMMENTS WITH THE VOICE AND TUNE OF JOEY IN "The One in Vegas: Part2 */
            /* Link: */ https://youtu.be/6OZoP2iWpnU?t=45 */

            // your prev is my prev
            insert_before_me->prev_in_bin->next_in_bin = block;
            // and i am his next
            block->prev_in_bin = insert_before_me->prev_in_bin;
            // oh wait you're my next
            block->next_in_bin = insert_before_me;
            // oh wait im your prev
            insert_before_me->prev_in_bin = block;
        }
            /*Inserting to tail */
        else {
            block->prev_in_bin = tail[i];
            tail[i]->next_in_bin = block;
            block->next_in_bin = nullptr;
            tail[i] = block;
        }

    }

}

int BlockMetaDataList::getBinIndex(size_t size) {
    int index = (int)(size / 1e3);
    /* https://piazza.com/class/kmeyq2ecrv940z?cid=584  */
    if(size == 128e3) {
        return 127;
    }
    if(index >= MMAP_BIN) {
        index = MMAP_BIN;
    }
    return index;
}

void BlockMetaDataList::insertToCombinedList(MallocMetaData *block) {

    /* Inserting new block to end of the list */
    // empty list
    if(isEmpty(COMBINED_LIST)) {
        combined_list_head = combined_list_tail = block;
        block->prev = nullptr;
        block->next = nullptr;
    }
        // one block in list
    else if(isSingleBlock(COMBINED_LIST)) {
        block->prev = combined_list_tail;
        combined_list_head->next = block;
        block->next = nullptr;
        combined_list_tail = block;
    }
        // more than one block in list
    else {
        combined_list_tail->next = block;
        block->prev = combined_list_tail;
        block->next = nullptr;
        combined_list_tail = block;
    }
    // TODO maybe update stats here?
}

MallocMetaData * BlockMetaDataList::expandAndOccupyWilderness(size_t size) {
    size_t expansion_size = size - WILDERNESS->size;
    void* addr = sbrk(expansion_size);
    if(addr == (void*)(-1)) { // sbrk fails
        return nullptr;
    }
    total_bytes += expansion_size;
    WILDERNESS->size = size;
    occupyBlock(WILDERNESS);
    return WILDERNESS;
}

void BlockMetaDataList::mergeFreeBlocks(MallocMetaData *middle) {
    MallocMetaData* prev = middle->prev;
    MallocMetaData* next = middle->next;
    if(prev and prev->is_free) {
        if (next and next->is_free) {
            mergeRight(middle, next);
            mergeLeft(prev, middle);
            // TODO is right and left replace code below?
            /*
            removeFromBinList(prev);
            removeFromBinList(middle);
            removeFromBinList(next);
            removeFromCombinedList(middle);
            removeFromCombinedList(next);
            //prev will remain as the new block
            prev->size += middle->size;
            prev->size += next->size;
            // TODO stats?
            total_blocks -=2;
            insertBlockToBinList(prev);
            */
            return;
        }
        else {
            mergeLeft(prev, middle);
            return;
        }
    }
    else if(next and next->is_free) {
        mergeRight(middle, next);
        return;
    }
    else {
        return;
    }

}

void BlockMetaDataList::removeFromBinList(MallocMetaData *block) {
    /* if block  is not free its already  not in  bin's list */
    if(not block->is_free) {
        return;
    }
    int i = getBinIndex(block->size);
    // block is the only one in list
    if(isSingleBlock(i)) {
        /* block is not in bin list */
        if(head[i] != block) {
            return;
        }
        head[i] = tail[i] = nullptr;
        return;
    }
        /* Removing the head  */
    else if(block == head[i]) {
        head[i] = block->next_in_bin;
        head[i]->prev_in_bin = nullptr;
        block->next_in_bin = nullptr;
    }
        /* Removing the tail  */
    else if(block == tail[i]) {
        tail[i] = block->prev_in_bin;
        tail[i]->next_in_bin = nullptr;
        block->prev_in_bin = nullptr;
    }
        /* Removing the middle  */
    else {
        block->prev_in_bin->next_in_bin = block->next_in_bin;
        block->next_in_bin->prev_in_bin = block->prev_in_bin;
        block->prev_in_bin = nullptr;
        block->next_in_bin = nullptr;
    }
}

void BlockMetaDataList::removeFromCombinedList(MallocMetaData *block) {

    // block is the only one in list
    if(isSingleBlock(COMBINED_LIST)) {
        combined_list_head = combined_list_tail = nullptr;
        return;
    }
        /* Removing the head  */
    else if(block == combined_list_head) {
        combined_list_head = block->next;
        combined_list_head->prev = nullptr;
        block->next = nullptr;
    }
        /* Removing the tail  */
    else if(block == combined_list_tail) {
        combined_list_tail = block->prev;
        combined_list_tail->next = nullptr;
        block->prev = nullptr;
    }
        /* Removing the middle  */
    else {
        block->prev->next = block->next;
        block->next->prev = block->prev;
        block->prev = nullptr;
        block->next = nullptr;
    }
}

MallocMetaData *BlockMetaDataList::splitBlock(MallocMetaData *block, size_t first_block_size) {
    assert(not block->is_free);
    /* There isn't enough room to split */
    if(block->size <= first_block_size + sizeof(MallocMetaData)) {
        return block;
    }

    /* Skipping first block meta data and size and setting the new block meta data */
    char* ptr = (char*)(block + sizeof(MallocMetaData) + first_block_size);
    MallocMetaData* second_block = (MallocMetaData*)(ptr);
    second_block->size = block->size - first_block_size - sizeof(MallocMetaData);
//    second_block->is_free = true;

    /* update size */
    block->size = first_block_size;

    /* Insert both new blocks to the bin list */
    freeBlock(second_block);

    /* Insert the second block after the first block in the combined list (sorted by addresses) */
    insertAfterToCombinedList(block, second_block);

    // Making sure we arent leaving any 2 adjacent free blocks
    if(second_block->next and second_block->next->is_free) {
        mergeRight(second_block, second_block->next);
    }

    return block;
}

void BlockMetaDataList::insertAfterToCombinedList(MallocMetaData *left, MallocMetaData *new_block) {
    /* Inserting to tail */
    if(left == combined_list_tail) {
        new_block->prev = combined_list_tail;
        left->next = new_block;
        new_block->next = nullptr;
        combined_list_tail = new_block;
    }
        /* Inserting in the middle */
    else {
        new_block->next = left->next;
        new_block->next->prev = new_block;
        left->next = new_block;
        new_block->prev = left;
    }
}

void BlockMetaDataList::mergeLeftAndFree(MallocMetaData *left, MallocMetaData *middle) {
    int i = getBinIndex(middle->size);

    freeBlock(middle);

//    removeFromBinList(left);
//    removeFromBinList(middle);
    removeFromCombinedList(middle);
    /*prev will remain as the new block */
    left->size += middle->size + sizeof(MallocMetaData);

//    insertBlockToBinList(left);
}
void BlockMetaDataList::mergeLeft(MallocMetaData *left, MallocMetaData *middle) {
    assert(middle->is_free);

    //    removeFromBinList(left);
//    removeFromBinList(middle);
    removeFromCombinedList(middle);
    /*prev will remain as the new block */
    left->size += middle->size + sizeof(MallocMetaData);

//    insertBlockToBinList(left);
}
void BlockMetaDataList::mergeRight(MallocMetaData *middle, MallocMetaData *right) {
    assert(middle->is_free);
//    freeBlock(middle);
//    removeFromBinList(middle);
//    removeFromBinList(right);
    removeFromCombinedList(right);
    /*middle will remain as the new block */
    middle->size += right->size + sizeof(MallocMetaData);

//    insertBlockToBinList(middle);
}

void BlockMetaDataList::mergeRightAndFree(MallocMetaData *middle, MallocMetaData *right) {
    freeBlock(middle);
//    removeFromBinList(middle);
//    removeFromBinList(right);
    removeFromCombinedList(right);
    /*middle will remain as the new block */
    middle->size += right->size + sizeof(MallocMetaData);

//    insertBlockToBinList(middle);
}

void BlockMetaDataList::occupyBlock(MallocMetaData *p) {
    assert(p->is_free);
    int i = getBinIndex(p->size);
    // should never get here
    assert(i != MMAP_BIN);
    p->is_free = false;
    num_free_blocks--;
    num_free_bytes -= p->size;
    removeFromBinList(p);
}

BlockMetaDataList meta_list;
BlockMetaDataList mmap_meta_list;

void* smalloc(size_t size) {

    MallocMetaData* block_metadata = meta_list.allocateBlock(size);
    if(block_metadata == nullptr) {
        return nullptr;
    }
    char* p1 = (char*)(block_metadata);
    void* block = (void*)(p1 + sizeof(MallocMetaData));
    return block;
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
    if (oldp == nullptr) {
        return smalloc(new_size);
    }
    if (new_size == 0) {
        return oldp;
    }
    char *p1 = (char *) (oldp);
    MallocMetaData *oldp_metadata = (MallocMetaData *) (p1 - sizeof(MallocMetaData));
    MallocMetaData *newp_metadata;
    size_t old_size = oldp_metadata->size;

    if (meta_list.getBinIndex(oldp_metadata->size) != MMAP_BIN) {
        {
            /* A. trying to use same block */
            if (oldp_metadata->size >= new_size) {
                newp_metadata = meta_list.splitBlock(oldp_metadata, new_size);
                char *p2 = (char *) (newp_metadata);
                return (void *) (p2 + sizeof(MallocMetaData));
            }
            size_t left_size = 0;
            size_t right_size = 0;
            /* B. trying to mergeLeft */
            if (oldp_metadata->prev and oldp_metadata->prev->is_free) {
                left_size = oldp_metadata->prev->size;

                /*              Block size after merge will be left size + old size +
                 *                      + size of metadata (currently 2 metadata for 2 blocks) */
                if (new_size <= oldp_metadata->size + left_size + sizeof(MallocMetaData)) {
                    newp_metadata = oldp_metadata->prev;

                    oldp_metadata->is_free = true; // for asserting reasons
                    meta_list.mergeLeft(oldp_metadata->prev, oldp_metadata);

                    meta_list.occupyBlock(newp_metadata);
                    /* Split the remaining (if there are)*/
                    newp_metadata = meta_list.splitBlock(newp_metadata, new_size);
                    char *p2 = (char *) (newp_metadata);
                    void* newp =  (void *) (p2 + sizeof(MallocMetaData));
                    memcpy(newp, oldp, old_size);
                    return newp;
                }
            }
            /* C. trying to mergeRightAndFree */
            if (oldp_metadata->next and oldp_metadata->next->is_free) {
                right_size = oldp_metadata->next->size;

                if (new_size <= oldp_metadata->size + right_size + sizeof(MallocMetaData)) {
                    meta_list.mergeRightAndFree(oldp_metadata, oldp_metadata->next);
                    meta_list.occupyBlock(oldp_metadata);
                    newp_metadata = oldp_metadata;
                    /* Split the remaining (if there are)*/
                    newp_metadata = meta_list.splitBlock(newp_metadata, new_size);
                    char *p2 = (char *) (newp_metadata);
                    void* newp =  (void *) (p2 + sizeof(MallocMetaData));
                    memcpy(newp, oldp, old_size);
                    return newp;
                }
            }
            /* D. trying to mergeBoth */
            if (left_size != 0 and   // means there is a left
                right_size != 0 and  // means there is a right
                new_size <= left_size + oldp_metadata->size + right_size + 2 * sizeof(MallocMetaData)) {

                meta_list.mergeRightAndFree(oldp_metadata, oldp_metadata->next);
                meta_list.occupyBlock(oldp_metadata);
                newp_metadata = oldp_metadata->prev;
                meta_list.mergeLeft(oldp_metadata->prev, oldp_metadata);
                meta_list.occupyBlock(newp_metadata);
                /* Split the remaining (if there are)*/
                newp_metadata = meta_list.splitBlock(newp_metadata, new_size);
                char *p2 = (char *) (newp_metadata);
                void* newp =  (void *) (p2 + sizeof(MallocMetaData));
                memcpy(newp, oldp, old_size);
                return newp;
            }
        }


    }
    /*
     * IF ARRIVED HERE:
     * 1. NEW_SIZE REQUIRES AN MMAP ALLOCATION
     * 2. THERE ISNT A FREE BLOCK IN BIN LIST ELIGIBLE FOR NEW SIZE --> MAKE A NEW ONE.
     *  IN BOTH CASES SMALLOC WILL DO THE RIGHT THING.
     */

/* E+F: Find/Allocate an other block */
    void *newp = smalloc(new_size);
/* If allocation failed return NULL and dont free oldp */
    if (newp == nullptr) {
        return nullptr;
    }
    memcpy(newp, oldp, old_size);

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

