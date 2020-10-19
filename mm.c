/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Half_Babubhaiya",
    /* First member's full name */
    "Sagar Tyagi",
    /* First member's email address */
    "imsagartyagi@cse.iitb.ac.in",
    /* Second member's full name (leave blank if none) */
    "Debasish Das",
    /* Second member's email address (leave blank if none) */
    "debasishdas@cse.iitb.ac.in"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define HEADER_LENGTH                8  /*Header length in Bytes*/
#define FOOTER_LENGTH                8  /*Footer length in Bytes*/
#define MIN_BLOCK_SIZE               ALIGN(HEADER_LENGTH + sizeof(struct free_block_header) + FOOTER_LENGTH)
#define free_block_header_ptr        struct free_block_header*
#define free_block_header_size       ALIGN(sizeof(struct free_block_header))

/*These two defination will give the Header and Footer access by having the address of block*/
#define Header(block_ptr)            *((int*)block_ptr)
#define Footer(block_ptr)            *((int*)((char*)block_ptr + (*((int*)block_ptr) & -2) - FOOTER_LENGTH))

/*Get payload ptr from block ptr and vide versa*/
#define Payload_ptr(block_ptr)		 (char*)block_ptr + HEADER_LENGTH
#define Block_ptr(payload_ptr)		 (char*)payload_ptr - HEADER_LENGTH
 
/*Using a circular doubly linked list to store freelist*/
struct free_block_header
{ 	int size;
	struct free_block_header* prev;
	struct free_block_header* next;
};

/* 
 * mm_init - initialize the malloc package.
 */

/*Global count for debugging*/
int count = 0;

void *init_mem_sbrk_break = NULL;
free_block_header_ptr head = NULL;			/*this will contain the head of doubly-linked list*/


/*Temporary function for printing the heap*/
void print_heap_condition()
{
	// Here we are iterating the heap implicitly i.e, both allocated blocks and free blocks
	// with the help of size feild in the header.
	free_block_header_ptr implicit_iterator = mem_heap_lo();
	while (implicit_iterator < (free_block_header_ptr)mem_heap_hi())
	{
		printf("Address -- %p  ",implicit_iterator);
		if(Header(implicit_iterator) & 1) printf("Allocated ");
		else printf("Free Block--  ");
		printf("Size of Block %d  ", Header(implicit_iterator) & -2) ;
		printf("\n");
		implicit_iterator = (free_block_header_ptr)((char *)implicit_iterator + (Header(implicit_iterator) & -2));
	}	
	printf("\n\n");
}

void print_list(free_block_header_ptr head)
{
	if(head == NULL) return;
	free_block_header_ptr iterator = head;
	while (iterator != NULL)
	{
		printf("Address -- %p size -- %d   \n", iterator, iterator ->size);
		iterator = iterator ->next;
	}
}
/*Delete the block from the freelist pointed by ptr*/
void delete_block_from_list(free_block_header_ptr block_ptr)
{
	if (block_ptr ->next == NULL && block_ptr ->prev == NULL)
	{
		head = NULL;
		return;
	}
	if(head == block_ptr)
	{
		head = head ->next;
		head ->prev = NULL;
		return;
	}
	if(block_ptr ->next == NULL)
	{
		block_ptr ->prev ->next = NULL;
		return;
	}
	block_ptr ->prev ->next = block_ptr->next;
	block_ptr ->next ->prev = block_ptr ->prev;
}

/*Insert the block into the freelist pointed by ptr*/
void insert_block_in_list(free_block_header_ptr block_ptr)
{
	/*If the linked list is empty*/
	if(head == NULL)
	{
		/*Here the size is already setted in Header field, when we were allocating the blocks*/
		block_ptr ->prev = NULL;
		block_ptr ->next = NULL;
		head = block_ptr;
		return;
	}
	block_ptr ->next = head;
	block_ptr ->prev = NULL;
	block_ptr ->next ->prev = block_ptr;
	head = block_ptr;
}

/*this function will give the smallest size greater than requirement (if present, otherwise NULL)*/
void * best_fit_in_doubly_linked_list(int size)
{
	/*If the linked list is empty*/
	if(head == NULL)
	{
		return NULL;
	}
	free_block_header_ptr iterator = head;
	free_block_header_ptr current_best =  NULL;
	while (iterator != NULL) /*as it is a circular double linked list*/
	{
		if (iterator ->size >= size)
		{
			if (current_best == NULL)
			{
				current_best = iterator;
			}
			else if(current_best -> size > iterator ->size)
			{
				current_best  = iterator;
			}
		}
		iterator = iterator ->next;
	}
	return current_best;
}

/*This function is capable to coaelesce from both side*/
void* coaelesce_next_and_prev_free_block(void * block_ptr)
{
	void * next_block_ptr = ((char*)block_ptr + (Header(block_ptr) & -2) > (char*)mem_heap_hi()) ? NULL : (char*)block_ptr + (Header(block_ptr) & -2);
	void * prev_block_ptr = ((char*)block_ptr - (*((int *)((char*)block_ptr - FOOTER_LENGTH)) & -2)) < (char *)mem_heap_lo()?
							NULL : ((char*)block_ptr - (*((int *)((char*)block_ptr - FOOTER_LENGTH)) & -2));
	
	/*In case of overflow is_next_allocated and is_prev_allocated is set as 1*/
	int is_next_allocated = next_block_ptr == NULL ? 1 : Header(next_block_ptr) & 1;
	int is_prev_allocated = prev_block_ptr == NULL ? 1 : Header(prev_block_ptr) & 1;

	if(!is_prev_allocated && !is_next_allocated)		/*both the blocks are free*/
	{
		/*Delete both the block to maintain consistency*/
		delete_block_from_list(prev_block_ptr);
		delete_block_from_list(next_block_ptr); 

		Header(prev_block_ptr) = Header(prev_block_ptr) + (Header(block_ptr) & -2) + Header(next_block_ptr);
		Footer(prev_block_ptr) = Header(prev_block_ptr);
		return prev_block_ptr;	
	} 
	else if(!is_prev_allocated && is_next_allocated)
	{
		/*Delete the prev block from freelist to maintain consistency*/
		delete_block_from_list(prev_block_ptr);

		Header(prev_block_ptr) = Header(prev_block_ptr) + (Header(block_ptr) & -2);
		Footer(prev_block_ptr) = Header(prev_block_ptr);
		return prev_block_ptr;
	}
	else if(is_prev_allocated && !is_next_allocated)
	{
		/*Delete the next block from freelist to maintain consistency*/
		delete_block_from_list(next_block_ptr);

		Header(block_ptr) = (Header(block_ptr) & -2) + Header(next_block_ptr);
		Footer(block_ptr) = Header(block_ptr);
		return block_ptr;
	}
	else
	{
		return block_ptr;
	}
}

int mm_init(void)
{
	
	//This function is called every time before each test run of the trace.
	//It should reset the entire state of your malloc or the consecutive trace runs will give wrong answer.	
	

	/* 
	 * This function should initialize and reset any data structures used to represent the starting state(empty heap)
	 * 
	 * This function will be called multiple time in the driver code "mdriver.c"
	 */
	mem_reset_brk();
	head = NULL;
    return 0;		//Returns 0 on successfull initialization.
}

//---------------------------------------------------------------------------------------------------------------
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{	
	/* 
	 * This function should keep track of the allocated memory blocks.
	 * The block allocation should minimize the number of holes (chucks of unusable memory) in the heap memory.
	 * The previously freed memory blocks should be reused.
	 * If no appropriate free block is available then the increase the heap  size using 'mem_sbrk(size)'.
	 * Try to keep the heap size as small as possible.
	 */
	/*
		Our allocated block will look like
		-----------------------------------
		|Header	    |  Payload | Footer    |
		|(stores    |  of the  | (stores   |
		|size of    |  block.  |  size of  |
		|Allocated  |          |  allocated|
		|Block)	    |          |  block    |
		-----------------------------------     
		
		Header and footer is required for both side coealescing.
	*/
	int size_include_payload = ALIGN(HEADER_LENGTH + size + FOOTER_LENGTH);
	size_include_payload = (size_include_payload < MIN_BLOCK_SIZE)? MIN_BLOCK_SIZE : size_include_payload;  /*To save the overflow condition when making
																											block in freelist*/

	// Before allocating size find in the freelist.
	void* block_ptr = best_fit_in_doubly_linked_list(size_include_payload);     /* This function will give you the best size block in O(n) time.*/
	
	if(block_ptr == NULL)  /* That means we didn't get anything from the freelist*/
	{
		block_ptr = mem_sbrk(size_include_payload);
		if((long)block_ptr == -1)
		{
			return NULL; /*Not enough space in the memory*/
		}
		/*Set the header size and the footer size and mark them as allocated*/
		 Header(block_ptr) = size_include_payload | 1;
		 Footer(block_ptr) = size_include_payload | 1;
	}
	else
	{
		/*Here we need to do the splitting of the block we got from best_fit_in_doubly_linked_list*/
		if((Header(block_ptr) - size_include_payload) >= MIN_BLOCK_SIZE )
		{
			/*Here we are tolerating some extent of internal fragmentation*/
			int old_block_size = Header(block_ptr);
			Header(block_ptr) = size_include_payload | 1;
			Footer(block_ptr) = size_include_payload | 1;


			void * new_free_block_ptr = (char*)block_ptr + (Header(block_ptr) & -2);
			Header(new_free_block_ptr) = old_block_size - size_include_payload;
			Footer(new_free_block_ptr) = old_block_size - size_include_payload;

			/*Now Delete the old block from free list*/			
			delete_block_from_list(block_ptr);

			/*Add the new free block into the free list*/
			insert_block_in_list(new_free_block_ptr);
		}
		else
		{
			/*Mark the block allocated and remove it from the freelist.*/

			Header(block_ptr) = Header(block_ptr) | 1;
			Footer(block_ptr) = Footer(block_ptr) | 1;

			delete_block_from_list(block_ptr);
		}
	}
	// Uncomment this code to see the heap after every malloc request.

	// printf("\n---------------------Malloc Malloc Request %d------------------\n",++count + 4);
	// print_heap_condition();
	// printf("\n---------------------Print Freelist------------------\n");
	// print_list(head);
	// printf("\n");
	// sleep(1);
	return Payload_ptr(block_ptr);
}


void mm_free(void *ptr)
{
	/* 
	 * Searches the previously allocated node for memory block with base address ptr.
	 * 
	 * It should also perform coalesceing on both ends i.e. if the consecutive memory blocks are 
	 * free(not allocated) then they should be combined into a single block.
	 * 
	 * It should also keep track of all the free memory blocks.
	 * If the freed block is at the end of the heap then you can also decrease the heap size 
	 * using 'mem_sbrk(-size)'.
	 */
	/*
		Our free block will look something like this:
		-----------------------------------------
		|Header	    | Payload		 | Footer    |
		|(stores    | of the block.  | (stores   |
		|size of    | contains the   |  size of  |
		|Allocated  | next, prev  ptr|  allocated|
		|Block)	    | size           |  block    |
		----------------------------------------- 	
    */
	void* block_ptr = Block_ptr(ptr);
	block_ptr = coaelesce_next_and_prev_free_block(block_ptr);

	/*Mark this block as Free Block*/
	Header(block_ptr) = Header(block_ptr) & -2;
	Footer(block_ptr) = Footer(block_ptr) & -2;

	/*Now add this free block to tree -- O(1) operation*/
	insert_block_in_list(block_ptr);
	
	// Uncomment this code to see the heap after every Free Request.. 

	// printf("\n---------------------Printing Free Request %d------------------\n",++count + 4);
	// print_heap_condition();
	// printf("\n---------------------Print Freelist------------------\n");
	// print_list(head);
	// sleep(1);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{	
	// If ptr is null, it will work simply as malloc.
	size = ALIGN(size); // make the size multiple of 8;

	if(ptr == NULL)
		return mm_malloc(size);
	

	if(size == 0)
	{
		mm_free(ptr);
		return NULL;
	}
	free_block_header_ptr old_block = (free_block_header_ptr)((char *)ptr - ALIGN(sizeof(free_block_header_ptr)));
	int old_block_payload_size = old_block ->size - (ALIGN(sizeof(free_block_header_ptr)) + FOOTER_LENGTH); 

	/*If the size is same as old block*/
	if(old_block_payload_size == size)
	   return ptr;

	void* new_block = mm_malloc(size);

	/*The copy size should be minimum of old_payload_size and */
	int copy_size = (old_block_payload_size < size)? old_block_payload_size : size;
	memcpy(new_block, ptr, copy_size);
	mm_free(ptr);
	return new_block;

	mm_free(ptr);
	return new_block;
}














