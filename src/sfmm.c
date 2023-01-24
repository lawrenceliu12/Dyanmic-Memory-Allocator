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

sf_size_t aggregate_payload = 0;
sf_size_t max_aggregate_payload = 0;

int get_index = 0;
sf_block *old_epilogue = NULL;
int error = 0;

int initialized = 0;

/*
 *  All methods in the entire sfmm.c 
 */
void compare_payload();
sf_size_t round_to_16(sf_size_t size);
void set_payload_size(sf_block *block, uint64_t payload);
sf_size_t get_payload_size(sf_header header);
sf_size_t get_blocksize_from_header(sf_header header);
sf_header set_blocksize_from_header(sf_header header, sf_size_t size);
void flush(int index);
int free_list_index(sf_size_t size);
int valid_quick_list(sf_size_t size);
int quick_list_index(sf_size_t size);
void insert_to_quick_list(sf_block *block, sf_size_t size_of_block);
sf_header update_quick_list_header(sf_header header);
void add_to_free_list(sf_block *firstblock, sf_size_t size);
void remove_from_free_list(sf_block *block);
sf_block *find_next_free_block (sf_size_t size, sf_block *block);
void write_footer(sf_block *block);
void initialize_heap();
int is_prev_allocated(sf_header header);
sf_header set_allocated_bit_to_0(sf_header header);
sf_header set_prev_allocated_bit_to_0 (sf_header header);
sf_block *go_next_block(sf_block *block);
sf_block *coalesce (sf_block *block);
sf_block *create_new_page_expand_heap();
void *sf_malloc(sf_size_t size);
int get_quick_list_max_length();
int check_validity (void *pp);
void sf_free(void *pp);
sf_block *realloc_split(sf_block *realloc_block, sf_size_t rsize, sf_size_t padded_rsize);
void *sf_realloc(void *pp, sf_size_t rsize);
double sf_internal_fragmentation();
double sf_peak_utilization();

/*
* Start of methods in sfmm.c
*/
void compare_payload(){
    if (aggregate_payload > max_aggregate_payload){
        max_aggregate_payload = aggregate_payload;
    }
}

sf_size_t round_to_16(sf_size_t size){
    if (size % 16 == 0){
        //debug ("%i multiple of 16: ", size);
        return size;
    }
    else{
        int result = size / 16;
        result += 1;

        debug("Result: %i", result);

        size = result * 16;
        //debug ("Size: %i", size);
        return size;
    }
}

void set_payload_size(sf_block *block, uint64_t payload){
   //// sf_header current_payload = ((uint64_t) block -> header >> 32) ^ MAGIC;         //MAY NEED THIS AS A FUTURE METHOD WITHOUT PAYLOAD SIZE. REMINDER!!
   // current_payload = payload;
    block -> header = ((block -> header ^ MAGIC) << 32) ^ MAGIC;
    block -> header = ((block -> header ^ MAGIC) >> 32) ^ MAGIC;
    block -> header = (((payload << 32) | (block -> header ^ MAGIC))) ^ MAGIC;
    // sf_show_block(block);
}

sf_size_t get_payload_size(sf_header header){
    return (((header ^ MAGIC) >> 32) & 0xffffffff);
}

sf_size_t get_blocksize_from_header(sf_header header){
    uint64_t checker = 0xFFFFFFF0;
    return ((header ^ MAGIC) & (checker));
}

sf_header set_blocksize_from_header(sf_header header, sf_size_t size){
    uint64_t checker = 0xFFFFFFF0;
    return (((header ^ MAGIC) & (~checker)) | size);
}

void flush(int index){
    int count = 0;
    // sf_block *block = NULL;
    while (sf_quick_lists[index].length != 0){
            // sf_block *quick_list_block = sf_quick_lists[quick_index].first -> body.links.next;       //relook at this?
        sf_block *quick_list_block = sf_quick_lists[index].first;
        
        sf_block *next_quick_list_block = (((void*) quick_list_block) + get_blocksize_from_header(quick_list_block -> header));
        next_quick_list_block -> header = (set_prev_allocated_bit_to_0 (next_quick_list_block -> header)) ^ MAGIC;
        int if_next_geo_allocated_bit = ((next_quick_list_block -> header) ^ MAGIC) & THIS_BLOCK_ALLOCATED; 
        if (!if_next_geo_allocated_bit){
            write_footer(next_quick_list_block);
        }

        sf_quick_lists[index].first = quick_list_block -> body.links.next;                     //Sets old head to the next node.
        sf_quick_lists[index].length = sf_quick_lists[index].length - 1;                    //changes length for update

            // quick_list_block -> header = update_quick_list_header(quick_list_block -> header);
        quick_list_block -> header = (quick_list_block -> header & (~IN_QUICK_LIST));

        //  sf_show_heap();
        
        // if (count != QUICK_LIST_MAX - 1){
        //     quick_list_block -> header = (set_prev_allocated_bit_to_0(quick_list_block -> header) ^ MAGIC);
        //     write_footer(quick_list_block);
        // }

        sf_size_t size = get_blocksize_from_header(quick_list_block -> header);

        set_payload_size(quick_list_block, 0);
        write_footer(quick_list_block);
        
        quick_list_block -> header = (set_allocated_bit_to_0(quick_list_block -> header) ^ MAGIC);
        write_footer(quick_list_block);
        //now free, look at previous

        // uint64_t prev_size = get_blocksize_from_header(quick_list_block -> prev_footer);
        // sf_block *previous_quick_list_block = (((void *) quick_list_block) - prev_size);

        // sf_show_block(previous_quick_list_block);

        // quick_list_block -> header = (set_prev_allocated_bit_to_0(quick_list_block -> header) ^ MAGIC);
        // write_footer(quick_list_block);

        add_to_free_list(quick_list_block, size);
        sf_show_heap();
        // sf_show_block(quick_list_block);

        sf_block *block = quick_list_block;

        sf_block *next_block = (((void *) block) + get_blocksize_from_header(block -> header));
        int if_next_allocated_bit = ((next_block -> header) ^ MAGIC) & THIS_BLOCK_ALLOCATED; 

        if (!if_next_allocated_bit){
            block = coalesce(next_block);
        }

        block = coalesce (block);
        
        // if (count > QUICK_LIST_MAX - 4){
        //     block = coalesce(block);            
        // }
        count++;
    }
}

int free_list_index(sf_size_t size){
    // debug("%i", size);
    if (size == 32){
        get_index = 0;
        return 0;
    }
    else{
        int counter = 0;
        while (size > 32){
            if (counter == NUM_FREE_LISTS - 1){
                return NUM_FREE_LISTS - 1;
            }
            size = size / 2;
            counter++;
            // debug ("size %i", size);
            // debug ("counter %i", counter);
            if (size <= 32){
            get_index = (counter);
                // debug("%i", get_index);
                return counter;
            }
        }
        get_index = NUM_FREE_LISTS - 1;
        return NUM_FREE_LISTS - 1;
    }
}

int valid_quick_list(sf_size_t size){
    if (size < 32 || size > 176){
        return -1;
    }
    return size % 16;
}

int quick_list_index(sf_size_t size){
    return ((size - 32) / 16);
}

void insert_to_quick_list(sf_block *block, sf_size_t size_of_block){
    if (valid_quick_list(size_of_block) != -1){
        int index = quick_list_index(size_of_block);
        if (sf_quick_lists[index].length == QUICK_LIST_MAX){
            flush(index);
        }
        block -> body.links.next = sf_quick_lists[index].first;
        sf_quick_lists[index].first = block;
        sf_quick_lists[index].length++;
        
        set_payload_size(block, 0);
        block -> header = (((block -> header) ^ MAGIC) | IN_QUICK_LIST) ^ MAGIC;
    }
}

sf_header update_quick_list_header(sf_header header){
    header &= ~(IN_QUICK_LIST);
    return header ^ MAGIC;
}

void add_to_free_list(sf_block *firstblock, sf_size_t size){
    sf_block* first_block_pointer = (sf_block*)(firstblock);
    sf_block* next_node_pointer = sf_free_list_heads[free_list_index(size)].body.links.next;
    // if (next_node_pointer == NULL){
    //     sf_free_list_heads[free_list_index(size)].body.links.next = &sf_free_list_heads[free_list_index(size)];
    //     sf_free_list_heads[free_list_index(size)].body.links.prev = &sf_free_list_heads[free_list_index(size)];

    // }
    // sf_show_block(first_block_pointer);

    sf_free_list_heads[free_list_index(size)].body.links.next = first_block_pointer;
    first_block_pointer -> body.links.prev = &sf_free_list_heads[free_list_index(size)];

    first_block_pointer -> body.links.next = next_node_pointer;
    next_node_pointer -> body.links.prev = first_block_pointer;

    sf_show_heap();

    sf_show_heap(); 

    // int size_block = get_blocksize_from_header(firstblock -> header);
    // int index = free_list_index(size_block);

    // sf_block *head = &sf_free_list_heads[index];
    // sf_block *block_temp = head -> body.links.next;

    // firstblock -> body.links.next = block_temp;
    // firstblock -> body.links.prev = head;

    // head -> body.links.next = firstblock;
    // block_temp -> body.links.prev = firstblock;


}

void remove_from_free_list(sf_block *block){        //MAY CHANGE TO VOID *?
    sf_block *previous_block = block -> body.links.prev;
    sf_block *current_block = block -> body.links.next;

    previous_block -> body.links.next = current_block;
    current_block -> body.links.prev = previous_block;
    // return current_block -> body.payload;

    // block -> body.links.next -> body.links.prev = block -> body.links.prev;
    // block -> body.links.prev -> body.links.next = block -> body.links.next;

    // block -> body.links.next = NULL;
    // block -> body.links.prev = NULL;
}

sf_block *find_next_free_block (sf_size_t size, sf_block *block){
    if (block == NULL){
        return NULL;
    }
    int original_index = free_list_index(size);
    int block_index = free_list_index(get_blocksize_from_header(block -> header));

    if (block_index < original_index){
        return NULL;
    }
    else if (block_index >= original_index){
        int blocksize = get_blocksize_from_header(block -> header);
        if (blocksize < size){
            return NULL;
        }
        return block;
    }
    return NULL;
}

void write_footer(sf_block *block){
    int size = get_blocksize_from_header((block -> header));
    // debug("%i", size);
    sf_block *footer = ((void *) block) + size;
    footer -> prev_footer = (((block -> header) ^ MAGIC) ^ MAGIC);                              //GET BLOCK TO END OF FOOTER
    // debug("%p ???? %p\n", block,  footer);
    // memcpy(footer, block, 8);
}

// sf_block *create_epilogue(){
//     sf_block *epilogue = sf_mem_end() - 8;
//     epilogue = ((void*) epilogue) + PAGE_SZ;
//     epilogue -> header = (0 + THIS_BLOCK_ALLOCATED);

//     write_footer(epilogue);

//     // sf_show_blocks();

//     return epilogue;
// }

void initialize_heap(){
    debug("initializing the heap. sf_mem_start() == sf_mem_end()");

    for (int i = 0; i < NUM_FREE_LISTS; i++){
       sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
       sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }

    if (sf_mem_grow() == NULL){
        error = 1;
    }

    //Split into prologue, general first block that is in the free list, and an epilogue block

    // void* mem_start = sf_mem_start();
    // void* mem_end = sf_mem_end();

    int size = PAGE_SZ - (6 * 8);

    sf_block *prologue = (sf_block*) (sf_mem_start());
    prologue -> header = (32 + THIS_BLOCK_ALLOCATED) ^ MAGIC;

    sf_block *firstblock = (sf_block*) (sf_mem_start() + (4 * 8));
    // firstblock -> header = (PREV_BLOCK_ALLOCATED | size);
    firstblock -> header = (size + PREV_BLOCK_ALLOCATED) ^ MAGIC;

    sf_block *epilogue = (sf_block *) (sf_mem_end() - (2 * 8));
    epilogue -> header = (0 + THIS_BLOCK_ALLOCATED) ^ MAGIC;

    old_epilogue = epilogue;

    //footer of a free block should be the same of the header of that free block. should make it easier to coalesce
    sf_footer *footer = (sf_footer *) epilogue;
    *footer = firstblock -> header;

    add_to_free_list(firstblock, size);

    sf_show_heap();
}

int is_prev_allocated(sf_header header){
    uint64_t checker = (header ^ MAGIC) & PREV_BLOCK_ALLOCATED;
    checker = checker >> 1;
    // debug("%li", checker);
    return checker;
}

sf_header set_allocated_bit_to_0(sf_header header){
    return (header ^ MAGIC) & (~THIS_BLOCK_ALLOCATED);
}

sf_header set_prev_allocated_bit_to_0 (sf_header header){
    return (header ^ MAGIC) & (~PREV_BLOCK_ALLOCATED);
}

sf_block *go_next_block(sf_block *block){
    uint64_t size_to_go_next = get_blocksize_from_header(block -> header);
    return ((void*) block) + size_to_go_next;
}

sf_block *coalesce (sf_block *block){
    //add the two sizes change the header of first and footer of second
    //TODO: try to get to the previous block so you can get the size of that

    // sf_show_block(block);
    // int asdgi = get_blocksize_from_header(block -> prev_footer);
    // debug("%i", asdgi);
    // sf_block *prev = ((void *) block) - asdgi;
    // sf_show_block(prev);
    // if (prev);

    // if (block == NULL){
    //     return NULL;
    // }

    int prev_block_allocated = is_prev_allocated(block -> header);
    if (prev_block_allocated == 0){         //Previous block is not allocated
        int prev_block_size = get_blocksize_from_header(block -> prev_footer);
        int new_size = get_blocksize_from_header(block -> header) + prev_block_size;

        // remove_from_free_list (block);
        // sf_show_heap();

        // sf_block *previous_block = ((void *) block) - prev_block_size; 
        // // block = ((void *) block) - prev_block_size;                                 //Block is at previous block

        // previous_block -> header = (set_blocksize_from_header(previous_block -> header, new_size));
        // write_footer(previous_block);

        // remove_from_free_list (block);

        // add_to_free_list(block, new_size);

        // return block;

        sf_block *previous_block = ((void *) block) - prev_block_size;

        previous_block -> header = (set_blocksize_from_header(previous_block -> header, new_size)) ^ MAGIC;         //MAGIC?

        remove_from_free_list(block);
        write_footer(previous_block);

        remove_from_free_list(previous_block);
        // sf_show_block(previous_block);
        // printf("\n\n");

        // before_max_block = previous_block;
        // // sf_show_block(before_max_block);
        // printf("\n\n\n");

        add_to_free_list(previous_block, new_size);
        return previous_block;
    }
    return NULL;
}

sf_block *create_new_page_expand_heap(){
    if (sf_mem_grow() == NULL){
        sf_errno = ENOMEM;
        return NULL;
    }
    sf_show_heap();

    //old epilogue pointer (from previous heap) points to new header
    sf_block *new_header = old_epilogue;

    if (new_header == NULL){
        return NULL;
    }

    sf_block *new_epilogue = (sf_block*) (sf_mem_end() - (2 * 8));
    new_epilogue -> header = 0;                                         //get rid of garbage values
    new_epilogue -> header = ((0 + THIS_BLOCK_ALLOCATED) ^ MAGIC);

    // *new_epilogue = *old_epilogue;               //old epilogue is ata the bottom

    //set the allocation bit to 0
    //set the blocksize to 1024
    //set the in_quick_list bit to 0
    //set payload size to 0
    //make sure you're copying the header to the footer.
    //place into free list 
    //ideally, coalesce (coalesce removes from free list).
    //figure out your previous allocation bit.

    new_header -> header = (((new_header -> header) ^ MAGIC) & (~THIS_BLOCK_ALLOCATED)) ^ MAGIC; //get rid of allocation bit to 0.
    new_header -> header = (((new_header -> header) ^ MAGIC) & (~IN_QUICK_LIST)) ^ MAGIC;        //get rid of in quick list bit
    new_header -> header = (set_blocksize_from_header((new_header -> header), PAGE_SZ)) ^ MAGIC;
    // sf_show_block(new_header);
    set_payload_size(new_header, 0);
    write_footer(new_header);
    add_to_free_list(new_header, PAGE_SZ);
    // sf_show_block(new_header);
    sf_show_heap();

    sf_block *coalesced_block = coalesce(new_header);

    if (coalesced_block == NULL){
        old_epilogue = sf_mem_end() - (2 * 8);
        coalesced_block = create_new_page_expand_heap();
    }
    return coalesced_block;
}



void *sf_malloc(sf_size_t size) {
    if (size == 0){
        return NULL;
    }

    if (sf_mem_start() == sf_mem_end()){
        initialize_heap();
        sf_show_heap();
        debug("Done initializing heap");
        if (error == 1){
            goto error_time;
        }
    }

    int payloadSize = size;

    size += 8; //include for header
    size = round_to_16(size);

    if (size <= 32){
        size = 32;
    }

    //START OF QUICK LIST
    if (valid_quick_list(size) == 0){
        int quick_index = quick_list_index(size); //get index of quick list
        
        if (sf_quick_lists[quick_index].length > 0){
            // sf_block *quick_list_block = sf_quick_lists[quick_index].first -> body.links.next;       //relook at this?
            sf_block *quick_list_block = sf_quick_lists[quick_index].first;
            sf_quick_lists[quick_index].first = quick_list_block -> body.links.next;                     //Sets old head to the next node.
            sf_quick_lists[quick_index].length = sf_quick_lists[quick_index].length - 1;                //changes length for update

            // quick_list_block -> header = update_quick_list_header(quick_list_block -> header);
            quick_list_block -> header = (quick_list_block -> header & (~IN_QUICK_LIST));

            aggregate_payload += (double) payloadSize;
            compare_payload();
            return quick_list_block -> body.payload;
        }
    }

    //START OF FREE LIST
    //Iterate through free list after initializing heap, starting from the closest one through finding index.
    for (int i = get_index; i < NUM_FREE_LISTS; i++){
        sf_block *block_head = &sf_free_list_heads[i];
        sf_block *current_block = block_head -> body.links.next;

        //Search through this linked list until the sentinel/head block is essentially reached once again.
        while (current_block != block_head){
            int block_header_size = get_blocksize_from_header(current_block -> header); //return 976
            // debug ("block header size: %i", block_header_size);
            if (block_header_size >= size){            //block_header_size = 976, size = 32
                //remove from free list, and return the address
                //check if you need to split it, split if necessary
                //once its split, then you allocate by setting payload size to the original argument size and you set the allocation bit to 1.
                //return address of that block's payload (&address -> body -> payload)

                //REMOVE FROM FREE LIST -> RETURN TO PAYLOAD
                // char *payload = remove_from_free_list(current_block); //TODO: RETURN ADDRESS?  
                remove_from_free_list(current_block);

                //CHECK IF YOU NEED TO SPLIT IT, SPLIT IF NECESSARY
                //difference between the original chunk and the chunk you're trying to get.
                int chunk_I_want = 32;
                int new_size = block_header_size;

                sf_block *split_pointer = NULL;
                if (block_header_size - size >= chunk_I_want){          //Changed, relook
                    //Change current block to become new size I want
                    int current_block_size = size;
                    new_size = block_header_size - size;      

                    //calculate the address of the free block by adding the size to the address of the current block
                    sf_block *split_block = ((void*) (current_block) + 8); 

                    //set the new free block to the difference between the two values.
                    split_block -> header = (block_header_size - current_block_size) ^ MAGIC;
                    current_block -> header = (current_block_size) ^ MAGIC;
                    write_footer(split_block);
                    write_footer(current_block);

                    split_pointer = split_block;

                    // add_to_free_list(split_block, block_header_size - size);
                    // sf_show_block(current_block);
                    // debug("\n");

                    // sf_show_block(split_block);
                }
                else{
                    sf_block *next_block = ((void *) current_block) + get_blocksize_from_header(current_block -> header);
                    next_block -> header = (((next_block -> header) ^ MAGIC) | PREV_BLOCK_ALLOCATED) ^ MAGIC;
                    // sf_show_block(next_block);

                    set_payload_size(current_block, payloadSize);
                    current_block -> header = (((current_block -> header) ^ MAGIC) | THIS_BLOCK_ALLOCATED) ^ MAGIC;
                    write_footer(current_block);
                    
                    aggregate_payload += (double) payloadSize;
                    compare_payload();
                    return current_block -> body.payload;
                }
                //Finished split, allocate by setting payload size to the original argument size and set the allocation bit to 1.
                //Allocating by setting payload size to original argument size.
                // sf_header current_payload = (((uint64_t) ((current_block -> header) ^ MAGIC)) >> 32) ^ MAGIC;         //MAY NEED THIS AS A FUTURE METHOD WITHOUT PAYLOAD SIZE. REMINDER!!
                // current_payload = payloadSize;
                // current_block -> header = (((current_payload << 32)) | ((current_block -> header) ^ MAGIC)) ^ MAGIC;
                set_payload_size(current_block, payloadSize);
                current_block -> header = (((current_block -> header) ^ MAGIC) | THIS_BLOCK_ALLOCATED) ^ MAGIC;
                current_block -> header = (((current_block -> header) ^ MAGIC) | PREV_BLOCK_ALLOCATED) ^ MAGIC;

                uint64_t size_of_block = get_blocksize_from_header(current_block -> header);

                sf_block *next_block = ((void*) (current_block) + size_of_block);
                // sf_show_block(next_block);

                if (split_pointer != NULL){
                    next_block -> header = (((split_pointer -> header) ^ MAGIC) ^ MAGIC);
                    next_block -> header = (((next_block -> header) ^ MAGIC) | PREV_BLOCK_ALLOCATED) ^ MAGIC;
                    add_to_free_list(next_block, new_size);
                }
                else{
                    next_block -> header = 0 ^ MAGIC;
                    next_block -> prev_footer = 0 ^ MAGIC;
                }
                write_footer(current_block);
                write_footer(next_block);
              
                // add_to_free_list(next_block, new_size);

                // sf_show_block(current_block);
                // debug("\n");
                // sf_show_block(next_block);
                // debug("\n");
                sf_show_heap();
                sf_show_heap();

                aggregate_payload += (double) payloadSize;
                compare_payload();
                return current_block -> body.payload;
            }
            current_block = current_block -> body.links.next; //Goto next node
        }
    }

    uint64_t final_size = PAGE_SZ;
    sf_block *return_coalesced_blocks = NULL;
    // if (final_size < size){
    // while (final_size <= size){
    while ((return_coalesced_blocks = (find_next_free_block(size, return_coalesced_blocks))) == NULL){
        // debug("final size: %li", final_size);
        sf_show_heap();
        return_coalesced_blocks = create_new_page_expand_heap();
        sf_show_heap();
        if (return_coalesced_blocks == NULL){
            // sf_show_block(before_max_block);
            
            // add_to_end_free_list(before_max_block);
            sf_show_heap();

            goto error_time;
        }
        // add_to_free_list(return_coalesced_blocks, get_blocksize_from_header(return_coalesced_blocks -> header));
        sf_show_heap();
        final_size = get_blocksize_from_header(return_coalesced_blocks -> header);
        old_epilogue = sf_mem_end() - (2 * 8);
    }
    // }
    // else{
    //     return_coalesced_blocks = create_new_page_expand_heap();
    //      if (return_coalesced_blocks == NULL){
    //         // sf_show_block(before_max_block);
    //         add_to_end_free_list(before_max_block);
    //         sf_show_heap();
    //         goto error_time;
    //     }
    //     final_size = get_blocksize_from_header(return_coalesced_blocks -> header);
    //     old_epilogue = sf_mem_end() - (2 * 8);
    // }

    // sf_block *return_coalesced_blocks = create_new_page_expand_heap();

    // if (return_coalesced_blocks != NULL){
    //     return return_coalesced_blocks -> body.payload;
    // }

    // final_size = get_blocksize_from_header(return_coalesced_blocks -> header);

    // old_epilogue = sf_mem_end() - 8;
    // while (final_size <= size){
    //     return_coalesced_blocks = create_new_page_expand_heap();
    //     final_size = get_blocksize_from_header(return_coalesced_blocks -> header);
    //     old_epilogue = sf_mem_end() - 8;
    // }
    // debug("Size: %i", size);
    // debug("Final size: %li", final_size);

    final_size = get_blocksize_from_header(return_coalesced_blocks -> header);
    old_epilogue = sf_mem_end() - (2 * 8);

    remove_from_free_list(return_coalesced_blocks);
    sf_show_heap();

    if (final_size >= size){
                //CHECK IF YOU NEED TO SPLIT IT, SPLIT IF NECESSARY
                //difference between the original chunk and the chunk you're trying to get.
                int chunk_I_want = 32;
                // int new_size = final_size;

                sf_block *split_pointer = NULL;
                if (final_size - size >= chunk_I_want){         //CHANGED RELOOK
                    //Change current block to become new size I want
                    int current_block_size = size;
                    // new_size = final_size - size;            

                    //calculate the address of the free block by adding the size to the address of the current block
                    sf_block *split_block = ((void*) (return_coalesced_blocks) + 8); 

                    //set the new free block to the difference between the two values.
                    split_block -> header = (final_size - current_block_size) ^ MAGIC;
                    return_coalesced_blocks -> header = (current_block_size) ^ MAGIC;
                    write_footer(split_block);
                    write_footer(return_coalesced_blocks);

                    // sf_show_block(split_block);

                    split_pointer = split_block;

                    // add_to_free_list(split_block, block_header_size - size);
                    // sf_show_block(current_block);
                    // debug("\n");

                    // sf_show_block(split_block);
                }
                else{
                    sf_block *next_block = ((void *) return_coalesced_blocks) + get_blocksize_from_header(return_coalesced_blocks -> header);
                    next_block -> header = (((next_block -> header) ^ MAGIC) | PREV_BLOCK_ALLOCATED) ^ MAGIC;
                    // sf_show_block(next_block);

                    set_payload_size(return_coalesced_blocks, payloadSize);
                    return_coalesced_blocks -> header = (((return_coalesced_blocks -> header) ^ MAGIC) | THIS_BLOCK_ALLOCATED) ^ MAGIC;
                    write_footer(return_coalesced_blocks);

                    aggregate_payload += (double) payloadSize;
                    compare_payload();
                    return return_coalesced_blocks -> body.payload;
                }
                // else{
                //     // new_size = final_size - size;
                //     return_coalesced_blocks -> header = (size) ^ MAGIC;
                //     write_footer(return_coalesced_blocks);

                //     split_pointer = return_coalesced_blocks;
                // }
                //Finished split, allocate by setting payload size to the original argument size and set the allocation bit to 1.
                //Allocating by setting payload size to original argument size.
                sf_header current_payload = (((uint64_t) ((return_coalesced_blocks -> header) ^ MAGIC)) >> 32) ^ MAGIC;         //MAY NEED THIS AS A FUTURE METHOD WITHOUT PAYLOAD SIZE. REMINDER!!
                current_payload = payloadSize;
                return_coalesced_blocks -> header = (((current_payload << 32)) | ((return_coalesced_blocks -> header) ^ MAGIC)) ^ MAGIC;
                return_coalesced_blocks -> header = (((return_coalesced_blocks -> header) ^ MAGIC) | THIS_BLOCK_ALLOCATED) ^ MAGIC;
                return_coalesced_blocks -> header = (((return_coalesced_blocks -> header) ^ MAGIC) | PREV_BLOCK_ALLOCATED) ^ MAGIC;

                uint64_t size_of_block = get_blocksize_from_header(return_coalesced_blocks -> header);

                sf_block *next_block = ((void*) (return_coalesced_blocks) + size_of_block);

                if (split_pointer != NULL){
                    next_block -> header = (((split_pointer -> header) ^ MAGIC) ^ MAGIC);
                    next_block -> header = (((next_block -> header) ^ MAGIC) | PREV_BLOCK_ALLOCATED) ^ MAGIC;
                    add_to_free_list(next_block, get_blocksize_from_header(next_block -> header));
                }
                else{
                    next_block -> header = 0 ^ MAGIC;
                    next_block -> prev_footer = 0 ^ MAGIC;
                }
                write_footer(return_coalesced_blocks);
                sf_show_heap();
                write_footer(next_block);
                sf_show_heap();
              
                // add_to_free_list(next_block, new_size);

                // sf_show_block(current_block);
                // debug("\n");
                // sf_show_block(next_block);
                // debug("\n");
                sf_show_heap();

                // next_block -> header = (((next_block -> header) ^ MAGIC)| PREV_BLOCK_ALLOCATED) ^ MAGIC;
                // sf_show_block(next_block);

                aggregate_payload += payloadSize;
                compare_payload();
                return return_coalesced_blocks -> body.payload;
    }

    error_time:
    sf_errno = ENOMEM;
    // debug("%d", sf_errno);
    return NULL;
}

int get_quick_list_max_length(){
    int start = 32;
    for (int i = 0; i < NUM_QUICK_LISTS; i++){
        start += 16;
    }
    // debug("%i", start);

    return start;
}

int check_validity (void *pp){
     if (pp == NULL){            //If pointer is null, you must abort the function
        return 1;
    }

    if (pp - 16 == 0){          //Check if the pointer is 16 bit aligned.
        return 1;
    }

    sf_block *pointer = pp - 16;
    sf_header pointer_header = (pointer -> header);         //shouldnt be magiced?? it doesnt work without magic
    // sf_show_block(pointer);
    uint64_t pointer_size = (get_blocksize_from_header(pointer_header));
    uint64_t pointer_allocated_bit = ((pointer -> header ^ MAGIC)) & THIS_BLOCK_ALLOCATED;
    pointer_allocated_bit = pointer_allocated_bit >> 2;
    // debug("pointer allocated bit: %li", pointer_allocated_bit);
    uint64_t previous_pointer_allocated_bit = is_prev_allocated(pointer_header);
    // debug("previous allocated bit: %li", previous_pointer_allocated_bit);
    // debug("Size %li", pointer_size);
    // sf_show_block(pointer);

    //if prev_alloc is 0, then you can get the size
    if (!previous_pointer_allocated_bit){
        uint64_t previous_size = get_blocksize_from_header(pointer -> prev_footer);
        sf_block *previous_block = ((void *) pointer) - previous_size;
        // sf_show_block(previous_block);

        uint64_t prev_block_allocated = ((previous_block -> header ^ MAGIC)) & THIS_BLOCK_ALLOCATED;
        // debug("%li", prev_block_allocated);

        if (prev_block_allocated == 1 && previous_pointer_allocated_bit == 0){
            return 1;
        }
    }

    if (pointer_size < 32 
        || pointer_size % 16 != 0 
        || sf_mem_start() + 32 > pp - 16 
        || sf_mem_end() - 16 <= pp - 16 
        || pointer_allocated_bit == 0){      //RELOOK LAST CONDITION
        return 1;
    }
    return 0;
}

void sf_free(void *pp) {
    if (check_validity(pp)){
        abort();
    }

    sf_block *pointer = pp - 16;

    aggregate_payload -= (double) get_payload_size(pointer -> header);
    compare_payload();

    sf_header pointer_header = pointer -> header;
    uint64_t pointer_size = get_blocksize_from_header(pointer_header);
    uint64_t previous_pointer_allocated_bit = is_prev_allocated(pointer_header);

    int quick_list_max_length = get_quick_list_max_length();
    if (pointer_size <= quick_list_max_length){
        insert_to_quick_list(pointer, pointer_size);
        sf_show_heap();
    }
    else{
        // sf_show_block(pointer);
        pointer_header = (set_allocated_bit_to_0(pointer_header)) ^ MAGIC;             //OR SHOULD I USE pointer -> header?
        pointer -> header = pointer_header;
        // sf_show_block(pointer);
        write_footer(pointer);
        // sf_show_block(pointer);
        set_payload_size(pointer, 0);
        write_footer(pointer);
        // sf_show_block(pointer);
        add_to_free_list(pointer, pointer_size);

        sf_block *next_block = pp - 16 + pointer_size;

        //Change prev-alloc bit
        next_block -> header = (set_prev_allocated_bit_to_0(next_block -> header)) ^ MAGIC;
        int if_next_allocated_bit = ((next_block -> header) ^ MAGIC) & THIS_BLOCK_ALLOCATED; 
        if (!if_next_allocated_bit){
            write_footer(next_block);
        }
        // sf_show_block(pointer);
        // sf_show_block(next_block);

        // debug("next block allocated: %i", if_next_allocated_bit);
        // debug("previous block allocated: %li", previous_pointer_allocated_bit);
        sf_show_heap();
        if (!if_next_allocated_bit){                //If next block does not have an allocated bit
            pointer = coalesce(next_block);
            // int get_new_size = get_blocksize_from_header(pointer -> header);
            // add_to_free_list(pointer, get_new_size);
        }
        if (!previous_pointer_allocated_bit){
            pointer = coalesce(pointer);
            // int get_new_size = get_blocksize_from_header(pointer -> header);
            // add_to_free_list(pointer, get_new_size);
        }
    }
}

sf_block *realloc_split(sf_block *realloc_block, sf_size_t rsize, sf_size_t padded_rsize){
    uint64_t realloc_size = get_blocksize_from_header(realloc_block -> header);
    // sf_block *free_block = ((void *) realloc_block) + realloc_size;

    // sf_show_block(free_block);

    int chunk = 32;
    // uint64_t block_size = get_blocksize_from_header(free_block -> header);
    uint64_t new_size = realloc_size - padded_rsize;

    if (new_size < 0){
        return NULL;
    }

    if (new_size >= chunk){
        // remove_from_free_list(realloc_block);
        sf_block *split_block = ((void*) (realloc_block) + 8); 

        //set the new free block to the difference between the two values.
        split_block -> header = (realloc_size - padded_rsize) ^ MAGIC;
        realloc_block -> header = (padded_rsize) ^ MAGIC;
        write_footer(split_block);
        write_footer(realloc_block);

        // printf("\n\n\n");
        // sf_show_block(split_block);
        // printf("\n\n\n");
        // sf_show_block(free_block);
        // printf("\n\n\n");
        
        set_payload_size(realloc_block, rsize);
        realloc_block -> header = ((realloc_block -> header ^ MAGIC) | THIS_BLOCK_ALLOCATED) ^ MAGIC;
        realloc_block -> header = (((realloc_block -> header) ^ MAGIC) | PREV_BLOCK_ALLOCATED) ^ MAGIC;

        sf_block *next_block = ((void*) (realloc_block) + padded_rsize);

        if (split_block != NULL){
            next_block -> header = (((split_block -> header) ^ MAGIC) ^ MAGIC);
            next_block -> header = (((next_block -> header) ^ MAGIC) | PREV_BLOCK_ALLOCATED) ^ MAGIC;
            add_to_free_list(next_block, new_size);
        }
        else{
            next_block -> header = 0 ^ MAGIC;
            next_block -> prev_footer = 0 ^ MAGIC;
        }
        write_footer(realloc_block);
        write_footer(next_block);

        sf_show_heap();

        uint64_t next_block_size = get_blocksize_from_header(next_block -> header);
        sf_block *next_next_block = ((void *) next_block) + next_block_size;

        // sf_show_block(next_next_block);

        if (next_next_block == (sf_mem_end() - 16)){
            return next_block;
        }
        next_next_block -> header = (set_prev_allocated_bit_to_0(next_next_block -> header)) ^ MAGIC;
        write_footer(next_next_block);

        next_block = coalesce(next_next_block);
        // add_to_free_list(next_block, next_block_size);

        return next_block;        
    }
    else{
        set_payload_size(realloc_block, rsize);
        write_footer(realloc_block);
        return realloc_block;
    }
    return NULL;
}

void *sf_realloc(void *pp, sf_size_t rsize) {
    if (check_validity(pp)){
        sf_errno = EINVAL;
        return NULL;
    }
    if (!check_validity(pp) && rsize == 0){
        sf_free(pp);
        return NULL;
    }

    //Need to align rsize  
    sf_size_t new_rsize = rsize + 8;
    new_rsize = round_to_16 (new_rsize);

    if (new_rsize < 32){
        new_rsize = 32;
    }

    sf_block *realloc_block = pp - 16;
    uint64_t realloc_block_size = get_blocksize_from_header(realloc_block -> header);
    uint64_t realloc_header_size = sizeof(realloc_block -> header);

    if (rsize > realloc_block_size - realloc_header_size){           //Case 1: Reallocating to a Larger Size
        void *malloc_pointer = sf_malloc(rsize);
        sf_show_heap();
        if (malloc_pointer == NULL){                                   //Check validity of malloc
            return NULL;
        }
        sf_block *malloc_block = malloc_pointer - 16;
        uint64_t payload_size = get_payload_size(malloc_block -> header);
        memcpy(&(malloc_block -> body.payload), &(realloc_block -> body.payload), payload_size);
        sf_free(pp);
        return malloc_block -> body.payload;
    }
    else{  
        sf_block *block = realloc_split(realloc_block, rsize, new_rsize);
        if (block == NULL){
            return NULL;
        }
        return block -> body.payload;
    }
    return NULL;
}

double sf_internal_fragmentation() {
    if (sf_mem_end() == sf_mem_start()){
        return 0;
    }

    double block_size = 0.0;
    double payload_size = 0.0;

    sf_block *first_block = sf_mem_start() + 32;
    sf_header header = (first_block -> header);

    uint64_t alloc_bit = (header ^ MAGIC) & (THIS_BLOCK_ALLOCATED);

    uint64_t in_quick_list = (header ^ MAGIC) & (IN_QUICK_LIST);

    while(first_block != (sf_mem_end() - 16)){
        if ((alloc_bit) && (!in_quick_list)){
            block_size += get_blocksize_from_header(header);
            payload_size += get_payload_size(header);
        }
        first_block = go_next_block(first_block);
        header = (first_block -> header);
        alloc_bit = (header ^ MAGIC) & (THIS_BLOCK_ALLOCATED);
        in_quick_list = (header ^ MAGIC) & (IN_QUICK_LIST);
    }

    return (double) payload_size/block_size;
}

double sf_peak_utilization() {
    if (sf_mem_end() == sf_mem_start()){
        return 0;
    }

    double size_of_heap = (sf_mem_end() - sf_mem_start());

    return max_aggregate_payload / size_of_heap;
}
