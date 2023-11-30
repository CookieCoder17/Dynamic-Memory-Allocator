#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "sfmm.h"
#define M 32
sf_block *find_block_firstfit(size_t block_size, size_t payload_size, int start_block_index);
int find_class(size_t block_size);
int initalize_heap();
bool is_allocated(sf_header header);
int block_size_of(sf_header header);
int payload_size_of(sf_header header);
int extend_heap();
void init_freelist(sf_block *block);
void add_to_freelist(sf_block* block);
void remove_from_freelist(sf_block* block);
int is_aligned(sf_header header);
bool prev_alloc_of(sf_header header);
bool isValid_pointer(void *pp);
sf_block* coalesc(sf_block *block);
double max(double a, double b);
int num_of_pages = 0;
double max_aggregate_payload;
double current_aggregate_payload;