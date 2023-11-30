/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"
#include "helper.h"



void *sf_malloc(size_t size) {
    if(size == 0) return NULL;
    size_t block_size = size + 16;
    while(block_size % 16 != 0 || block_size < M) block_size++;
    int start_block_index = find_class(block_size);
    if(sf_mem_start() == sf_mem_end()){
        max_aggregate_payload = 0;
        current_aggregate_payload = 0;
        if(initalize_heap()){
            sf_errno = ENOMEM;
            return NULL;
        }
    }
    sf_block *allocated_block_ptr = find_block_firstfit(block_size,size,start_block_index);
    if(allocated_block_ptr != NULL){
        current_aggregate_payload += size;
        max_aggregate_payload = max(max_aggregate_payload,current_aggregate_payload);
        return allocated_block_ptr->body.payload;
    }
    else{ sf_errno = ENOMEM;}
    return NULL;
}

void sf_free(void *pp) {
    if(isValid_pointer(pp)){
        sf_block * block = (sf_block*)((sf_header*)pp - 2);
        int payload = payload_size_of(block->header);
        sf_block *free_block = coalesc(block);
        current_aggregate_payload -= payload;
        max_aggregate_payload = max(max_aggregate_payload,current_aggregate_payload);
        add_to_freelist(free_block);
    }else{
        abort();
    }
}

void *sf_realloc(void *pp, size_t rsize) {
    // if realloc to a bigger however no malloc max utl payload += (rsize - oldsize)
    if(isValid_pointer(pp) == 0){
        sf_errno = EINVAL;
        return NULL;
    }
    if(rsize == 0){
        sf_free(pp);
        return NULL;
    }
    sf_block * old_block = (sf_block*)((sf_header*)pp - 2);
    int block_payload = payload_size_of(old_block->header);
    int old_block_size = block_size_of(old_block->header);
    if(block_payload == rsize){
        return pp;
    }else if(block_payload > rsize){
        size_t block_size = rsize + 16;
        while(block_size % 16 != 0 || block_size < M) block_size++;
        int rem = old_block_size - block_size;
        if(rem < M){
            old_block->header &= 0x00000000ffffffff;
            old_block->header |= (rsize << 32);
            void *payload = old_block->body.payload;
            memcpy(payload,old_block->body.payload,rsize);
            sf_block *next_block = (sf_block*)((char *)old_block + block_size_of(old_block->header));
            next_block->prev_footer = old_block->header;
            return payload;
        }else{
            sf_block* free_block = (sf_block *)((char *)old_block + block_size);
            // old block
            old_block->header &= 0x000000000000000f;
            old_block->header |= (block_size);
            old_block->header |= (rsize << 32);
            void *payload = old_block->body.payload;
            memcpy(payload,old_block->body.payload,rsize);
            // free block
            free_block->header = 0;
            free_block->header |= (1<<2);
            free_block->header |= (rem);
            // rest
            free_block->prev_footer = old_block->header;
            sf_block* next_block = (sf_block *)((char *)free_block + rem);
            next_block->prev_footer = free_block->header;
            sf_block * new_free_block = coalesc(free_block);
            add_to_freelist(new_free_block);
            return payload;
        }
        current_aggregate_payload -= (block_payload-rsize);
        max_aggregate_payload = max(max_aggregate_payload,current_aggregate_payload);
    }else{
        size_t block_size = rsize + 16;
        while(block_size % 16 != 0 || block_size < M) block_size++;
        if(old_block_size == block_size){
            old_block->header &= 0x00000000ffffffff;
            old_block->header |= (rsize << 32);
            void *payload = old_block->body.payload;
            memcpy(payload,old_block->body.payload,rsize);
            sf_block *next_block = (sf_block*)((char *)old_block + block_size_of(old_block->header));
            next_block->prev_footer = old_block->header;
            current_aggregate_payload += (rsize-block_payload);
            max_aggregate_payload = max(max_aggregate_payload,current_aggregate_payload);
            return payload;
        }else{
            void *payload = sf_malloc(rsize);
            if(payload == NULL) return NULL;
            memcpy(payload,old_block->body.payload,block_payload);
            sf_free(pp);
            return payload;
        }
    }
}

double sf_fragmentation() {
    if(sf_mem_start() == sf_mem_end() || num_of_pages == 0){
        return 0.0;
    }
    double total_internal_fragmentation = 0;
    double total_allocated = 0;
    sf_block* first_block = (sf_block *)((char *)sf_mem_start() + 32);
    sf_block* block = first_block;
    while(block != (sf_block*)((sf_header*)sf_mem_end() - 2)){
        size_t block_size = block_size_of(block->header);
        if(is_allocated(block->header)){
            total_internal_fragmentation += (double)payload_size_of(block->header);
            total_allocated += block_size;
        }
        block = (sf_block*)((char *)block + block_size);
    }
    if(total_allocated == 0) return 0.0;
    else return (total_internal_fragmentation/total_allocated);
}

double sf_utilization() {
    if(sf_mem_start() == sf_mem_end() || num_of_pages == 0){
        return 0.0;
    }
    return (max_aggregate_payload/(num_of_pages * PAGE_SZ));
}

// HELPER FUNCTIONS

int initalize_heap(){
    sf_block* prologue = (sf_block*)sf_mem_grow();
    if(prologue == NULL){
        return 1;
    }
    num_of_pages = 1;
    prologue->header = 0;
    prologue->header |= (1<<3);
    prologue->header |= (1<<5);
    sf_block* epilouge = (sf_block*)(((sf_header*)sf_mem_end())-2);
    epilouge->header = (1<<3);
    sf_block* first_block = (sf_block *)((char *)prologue + 32);
    first_block->header = 0;
    first_block->header |= (1 << 2);
    first_block->header |= (PAGE_SZ - 48);
    first_block->prev_footer = prologue->header;
    epilouge->prev_footer = first_block->header;
    init_freelist(first_block);
    return 0;
}

bool isValid_pointer(void *pp){
    if(pp == NULL || (size_t)pp % 16){
        return false;
    }
    // check that third is first block
    // check that the fourth is right
    // first block of the heap is the address of the header of the block after the prologue
    // start of a block means header
    // last block is not the epilouge
    // footer of the block is either on or after the epilouge header (any of footer to overlap epilogue)
    sf_block * block = (sf_block*)((sf_header*)pp - 2);
    int block_size = block_size_of(block->header);
    sf_block *next_block = (sf_block*)((char *)block + block_size);
    // printf("h: %p\n",((sf_header*)sf_mem_start() + 5));
    // printf("d: %p\n",&block->header);
    // printf("e: %p\n",&next_block->prev_footer);
    // printf("f: %p\n",(sf_footer*)((sf_header*)sf_mem_end() - 1));
    if(block_size < 32
        || block_size % 16 != 0
        || (&block->header < ((sf_header*)sf_mem_start() + 5))
        || (&next_block->prev_footer >= (sf_footer*)((sf_header*)sf_mem_end() - 1))
        || (is_allocated(block->header) == 0)
        || ((prev_alloc_of(block->header) == 0) && (is_allocated(block->prev_footer) != 0))
        ){
        return false;
    }
    return true;
}

// Block to add to freelist post-coalescing
sf_block* coalesc(sf_block *block){
    int block_size = block_size_of(block->header);
    int prev_block_size = block_size_of(block->prev_footer);
    sf_block *prev_block = (sf_block*)((char *)block - prev_block_size);
    sf_block *next_block = (sf_block*)((char *)block + block_size);
    int next_block_size = block_size_of(next_block->header);
    int prev_alloc = prev_alloc_of(block->header);
    int next_alloc = is_allocated(next_block->header);
    // case 1: neither is free
    if(prev_alloc && next_alloc){
        block->header = 0;
        if(prev_alloc) block->header |= (1<<2);
        block->header |= block_size;
        next_block->prev_footer = block->header;
        next_block->header &= ~(1 << 2);
        if(block_size_of(next_block->header) != 0){
            sf_block *temp_next_block = (sf_block*)((char *)next_block + next_block_size);
            temp_next_block->prev_footer = next_block->header;
        }
        return block;
    }
    // case 2: both are free
    else if(prev_alloc == 0 && next_alloc == 0){
        int new_block_size = block_size + prev_block_size + next_block_size;
        int temp_prev_alloc = prev_alloc_of(prev_block->header);
        // prev_block
        remove_from_freelist(prev_block);
        prev_block->header = 0;
        if(temp_prev_alloc) prev_block->header |= (1<<2);
        prev_block->header |= new_block_size;
        // curr block
        block->header = 0;
        // next block
        sf_block *temp_next_block = (sf_block*)((char *)next_block + block_size_of(next_block->header));
        remove_from_freelist(next_block);
        next_block->header = 0;
        temp_next_block->prev_footer = prev_block->header;
        temp_next_block->header &= ~(1 << 2);
        if(block_size_of(temp_next_block->header) != 0){
            sf_block *temp_next_next_block = (sf_block*)((char *)temp_next_block + block_size_of(temp_next_block->header));
            temp_next_next_block->prev_footer = temp_next_block->header;
        }
        return prev_block;
    }
    // case 3: one of them is free
    else{
        if(prev_alloc == 0){ // only prev is free
            int new_block_size = block_size + prev_block_size;
            int temp_prev_alloc = prev_alloc_of(prev_block->header);
            // prev_block
            remove_from_freelist(prev_block);
            prev_block->header = 0;
            if(temp_prev_alloc) prev_block->header |= (1<<2);
            prev_block->header |= new_block_size;
            // curr block
            block->header = 0;
            next_block->prev_footer = prev_block->header;
            next_block->header &= ~(1 << 2);
            if(block_size_of(next_block->header) != 0){
                sf_block *temp_next_block = (sf_block*)((char *)next_block + block_size_of(next_block->header));
                temp_next_block->prev_footer = next_block->header;
            }
            return prev_block;
        }else{ // only next is free
            sf_block *temp_next_block = (sf_block*)((char *)next_block + block_size_of(next_block->header));
            int new_block_size = block_size + next_block_size;
            remove_from_freelist(next_block);
            next_block->header = 0;
            block->header = 0;
            if(prev_alloc) block->header |= (1<<2);
            block->header |= new_block_size;
            temp_next_block->prev_footer = block->header;
            temp_next_block->header &= ~(1 << 2);
            if(block_size_of(temp_next_block->header) != 0){
                sf_block *temp_next_next_block = (sf_block*)((char *)temp_next_block + block_size_of(temp_next_block->header));
                temp_next_next_block->prev_footer = temp_next_block->header;
            }
            return block;
        }
    }
}

void remove_from_freelist(sf_block* block){
    sf_block *prev = block->body.links.prev;
    sf_block *next = block->body.links.next;
    prev->body.links.next = next;
    next->body.links.prev = prev;
}
void add_to_freelist(sf_block* block){
    size_t block_size = block_size_of(block->header);
    int index = find_class(block_size);
    if((sf_block*)((char *)block + block_size) == (sf_block*)((sf_header*)sf_mem_end() - 2)){
        index = NUM_FREE_LISTS-1;
    }
    sf_block *next = sf_free_list_heads[index].body.links.next;
    next->body.links.prev = block;
    sf_free_list_heads[index].body.links.next = block;
    block->body.links.next = next;
    block->body.links.prev = &sf_free_list_heads[index];
}
void init_freelist(sf_block *block){
    for(int i = 0; i<NUM_FREE_LISTS; i++){
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }
    sf_free_list_heads[NUM_FREE_LISTS-1].body.links.next = block;
    sf_free_list_heads[NUM_FREE_LISTS-1].body.links.prev = block;
    block->body.links.prev = &sf_free_list_heads[NUM_FREE_LISTS-1];
    block->body.links.next = &sf_free_list_heads[NUM_FREE_LISTS-1];

}
int extend_heap(){
    sf_block* prev_block = (sf_block*)(((sf_header*)sf_mem_end())-2);
    int prev_alloc = prev_alloc_of(prev_block->header);
    int total_size = 0;
    int free_from_list = 0;
    if(prev_alloc == 0){
        int prev_block_size = block_size_of(prev_block->prev_footer);
        prev_block->header = 0;
        prev_block = ((sf_block*)((char *)prev_block - prev_block_size));
        total_size += prev_block_size;
        free_from_list = 1;

    }
    sf_block* new_page = sf_mem_grow();
    if(new_page == NULL){
        return 1;
    }
    num_of_pages += 1;
    total_size += PAGE_SZ;
    if(free_from_list){remove_from_freelist(prev_block);}
    int temp_prev_alloc = prev_alloc_of(prev_block->header);
    prev_block->header = 0;
    if(temp_prev_alloc) prev_block->header |= (1<<2);
    prev_block->header |= total_size;
    sf_block* epilouge = (sf_block*)(((sf_header*)sf_mem_end())-2);
    epilouge->header = (1<<3);
    epilouge->prev_footer = prev_block->header;
    add_to_freelist(prev_block);
    return 0;
}
sf_block *find_free_block(size_t block_size, int index){
    sf_block* curr;
    for(int i = index; i < NUM_FREE_LISTS; i++){
        curr = &sf_free_list_heads[i];
        sf_block* temp = curr;
        do{
            if(block_size_of(temp->header) < block_size){
                temp = temp->body.links.next;
            }else{
                return temp;
            }
        }while(temp != curr);
    }
    return NULL;

}
sf_block *find_block_firstfit(size_t block_size,size_t payload_size, int start_block_index){
    sf_block *curr = find_free_block(block_size, start_block_index);
    if(curr == NULL){
        if(extend_heap()) return NULL;
        return find_block_firstfit(block_size,payload_size, start_block_index);
    }else{
        size_t curr_block_size = block_size_of(curr->header);
        size_t rem = curr_block_size - block_size;
        int prev_alloc = prev_alloc_of(curr->header);
        if(rem < M){
            remove_from_freelist(curr);
            curr->header = 0;
            curr->header |= (1<<3);
            if(prev_alloc) curr->header |= (1<<2);
            curr->header |= (block_size + rem);
            curr->header |= (payload_size << 32);
            sf_block* next_block = (sf_block *)((char *)curr + (block_size + rem));
            next_block->prev_footer = curr->header;
            next_block->header |= (1<<2);
            if(block_size_of(next_block->header) != 0){
                sf_block *temp_next_block = (sf_block*)((char *)next_block + block_size_of(next_block->header));
                temp_next_block->prev_footer = next_block->header;
            }
        }else{
            sf_block* free_block = (sf_block *)((char *)curr + block_size);
            free_block->header = 0;
            free_block->header |= (1<<2);
            free_block->header |= (rem);
            remove_from_freelist(curr);
            curr->header = 0;
            curr->header |= (1<<3);
            if(prev_alloc) curr->header |= (1<<2);
            curr->header |= (block_size);
            curr->header |= (payload_size << 32);
            free_block->prev_footer = curr->header;
            sf_block* next_block = (sf_block *)((char *)free_block + rem);
            next_block->prev_footer = free_block->header;
            add_to_freelist(free_block);
        }
    }
    return curr;
}
double max(double a, double b){
    if(a > b){
        return a;
    }else{
        return b;
    }
}
bool is_allocated(sf_header header){
    return (header & (1 << 3));
}
int block_size_of(sf_header header){
    return (header & 0x00000000fffffff0);
}
int payload_size_of(sf_header header){
    return (header >> 32);
}
bool prev_alloc_of(sf_header header){
    return (header & (1 << 2));
}
int is_aligned(sf_header header){
    return ((header & 0xf) != 0);
}
int find_class(size_t block_size){
    int fib[] = {1,1};
    for(int i = 0; i < NUM_FREE_LISTS-2;i++){
        if (block_size <= fib[1]* M){
            return i;
        }else{
            int temp = fib[1];
            fib[1] = fib[0] + fib[1];
            fib[0] = temp;
        }
    }
    return NUM_FREE_LISTS-2;
}
