
#ifndef MEMORY_UNIT_IMPLEMENTATION_MALLOC_2_H
#define MEMORY_UNIT_IMPLEMENTATION_MALLOC_2_H

#endif //MEMORY_UNIT_IMPLEMENTATION_MALLOC_2_H
void* smalloc(size_t size);
void* scalloc(size_t num, size_t size);
void sfree(void* p) ;
void* srealloc(void* oldp, size_t new_size);
size_t _num_free_blocks();
size_t _num_free_bytes();
size_t _num_allocated_blocks();
size_t _num_allocated_bytes();
size_t _size_meta_data();
size_t _num_meta_data_bytes();