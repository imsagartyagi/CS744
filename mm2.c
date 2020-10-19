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
 
struct free_block_header
{ 	int size;
	short height;   // using 2 bytes for height and 2 byte for count of same size block.
	short count;
	struct free_block_header* left;
	struct free_block_header* right;
};

/* 
 * mm_init - initialize the malloc package.
 */

/*Global count for debugging*/
int count = 0;

void *init_mem_sbrk_break = NULL;
free_block_header_ptr tree_root = NULL;			/*this will contain the root of tree*/



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

/*This function gives the difference between left and right subtree*/
int get_left_right_height_difference(free_block_header_ptr root)
{
	if(root == NULL) return 0;
	int left_height = root ->left == NULL ? 0 : root ->left ->height;
	int right_height = root ->right == NULL ? 0 : root -> right ->height;
	return left_height - right_height;
}

/*Print Inorder traversal of freelist*/
void print_inorder(free_block_header_ptr root)
{
	if(root == NULL) return;
	print_inorder(root ->left);
	int left_size = root ->left == NULL ? -1 : root ->left ->size;
	int right_size = root ->right == NULL ? -1 : root ->right ->size;
	printf("s-%d,c-%d,h-%d,l-%d,r-%d,hd-%d   \n", root ->size, root->count, root->height, left_size,right_size,get_left_right_height_difference(root));
	print_inorder(root ->right);
}

/*initialize the free_block_header */
free_block_header_ptr find_successor_and_delete_it(free_block_header_ptr root)
{
	/*This function will find the successor and delete it, this can have two cases:
	  1. successor can have 0 childs 
	  2. successor can have 1 right child (because a successo can,t have left child)
	*/
    /*initially both of them is pointing to same root*/
	free_block_header_ptr successor_parent = root;
	free_block_header_ptr successor = root ->right;

	/*now loop until there is no left child of the successor*/
	while (successor ->left != NULL)
	{
		successor_parent = successor;
		successor = successor ->left;
	}
	/*Now at this point we have the successor*/
	/* ----> Unlink it from its parent and child*/
	free_block_header_ptr successor_right_child = successor ->right;
	successor ->right = NULL;
	if(successor_parent ->right == successor)
		successor_parent ->right = successor_right_child;
	else if(successor_parent ->left == successor)
		successor_parent ->left = successor_right_child;
	
	return successor;
}		
int get_max_height(free_block_header_ptr left, free_block_header_ptr right)
{
	int left_height = left == NULL ? 0 : left ->height;
	int right_height = right == NULL ? 0 : right ->height;
	return (left_height > right_height)? left_height : right_height;
}

/*standard code to perform left rotation*/
void * left_rotate(free_block_header_ptr root)
{
	/*Rotation*/
	free_block_header_ptr temp_var_1 = root ->right ->left;
	free_block_header_ptr temp_var_2 = root ->right; /*we need to save it because it's height is changed and we need access to it.
														and also this will be the new node*/
	root ->right ->left  = root;
	root ->right  = temp_var_1;

	/*Now update the heights*/
	root -> height = get_max_height(root ->left, root ->right) + 1;
	temp_var_2 ->height = get_max_height(temp_var_2 ->left, temp_var_2 ->right) + 1;

	return temp_var_2;
}

/*standard code to perform right rotation*/
void* right_rotate(free_block_header_ptr root)
{
	free_block_header_ptr temp_var_1 = root ->left ->right;
	free_block_header_ptr temp_var_2 = root -> left;

	root ->left ->right = root;
	root ->left = temp_var_1;

	/*Now update the heights*/
	root -> height = get_max_height(root ->left, root ->right) + 1;
	temp_var_2 ->height = get_max_height(temp_var_2 ->left, temp_var_2 ->right) + 1;

	return temp_var_2;
}


/*Delete the block from the freelist(Tree) pointed by ptr*/
/*Deleting Node by just manipulating pointers
 if which_child == 0, root is the left child of parent, for which_child == 1 --> right child of parent, for which_child == -1 --> root == parent.
*/
void * delete_block_from_tree(free_block_header_ptr root,free_block_header_ptr block_ptr, free_block_header_ptr parent, int which_child)
{
	if(root == NULL)
		return root;
	
	if(root ->size > block_ptr ->size)
	{
		root ->left = delete_block_from_tree(root ->left, block_ptr,root,0);
	}
	else if(root ->size < block_ptr ->size)
	{
		root ->right = delete_block_from_tree(root ->right, block_ptr,root,1);
	}
	else
	{
		/*I got the Match*/
		block_ptr ->count = block_ptr ->count - 1; /*If there are more than one blocks of same size are present, then decrease the count and return*/
		if(block_ptr ->count > 0)
			return root;
		
		/*Else delete that block from freelist*/
		if(root ->left == NULL && root ->right == NULL)
		{
			/*root has no child*/
			return NULL;
		}
		else if(root ->left == NULL && root -> right != NULL)
		{
			/*root has only right child*/
			if(which_child == -1) /* -1 indicates root == parent*/
			{
				parent = root ->right;
				root = parent;
			}
			else if (which_child == 0)
			{
				parent ->left = root ->right;
				root = parent ->left;
			}
			else if(which_child == 1)
			{
				parent ->right = root ->right;
				root = parent ->right;
			}	
		}
		else if(root ->left != NULL && root ->right == NULL)
		{
			/*root has only left child*/
			if(which_child == -1) /* -1 indicates root == parent*/
			{
				parent = root ->left;
				root = parent;
			}
			else if(which_child == 0)
			{
				parent ->left = root ->left;
				root = parent ->left;
			}
			else if(which_child == 1)
			{
				parent ->right = root ->left;
				root = parent ->right;
			}
		}
		else
	    {
			/*root has both the child*/
			free_block_header_ptr successor = find_successor_and_delete_it(root);
			
			if(which_child == -1)  /* -1 indicates root == parent*/
				parent = successor; 

			else if(which_child == 0)
				parent ->left = successor;

			else  parent ->right = successor;

			successor ->left = root ->left;
			successor ->right = root ->right;
			root = successor;
		}
	}

	/*Now update the height of node here and perform rotation if required.*/
	root ->height = get_max_height(root ->left, root ->right) + 1;
	int height_difference = get_left_right_height_difference(root);

	/*Here arises 4 standard cases of AVL tree i.e, LL, RR, LR, RL*/
    if (height_difference >= 2 && get_left_right_height_difference(root->left) >= 0) /*LL*/
        return right_rotate(root); 
   
    else if (height_difference >= 2 && get_left_right_height_difference(root->left) < 0) /*LR*/
    { 
        root->left =  left_rotate(root->left); 
        return right_rotate(root); 
    } 

    else if (height_difference <= -2 && get_left_right_height_difference(root->right) <= 0) /*RR*/
        return left_rotate(root); 
  
    else if (height_difference <= -2 && get_left_right_height_difference(root->right) > 0) /*RL*/
    { 
        root->right = right_rotate(root->right); 
        return left_rotate(root); 
    } 	
	return root;
}

/*Insert the block into the freelist(Tree) pointed by ptr*/

/*This is standard insertion AVL TREE CODE*/
/*Reference Geeks-For-Geeks and CLSR*/
void * insert_block_in_tree(free_block_header_ptr root,free_block_header_ptr block_ptr)
{
	/*Base condition*/
	if(root == NULL)
	{
		/*Here the size is already setted in Header field, when we were allocating the blocks*/
		block_ptr ->height = 1;
		block_ptr ->count = 1;
		block_ptr ->left = NULL;
		block_ptr ->right = NULL;
		return block_ptr;
	}

	if(root ->size > block_ptr ->size)
	{
		root->left =  insert_block_in_tree(root ->left, block_ptr);
	}
	else if (root ->size < block_ptr ->size)
	{
		root ->right = insert_block_in_tree(root ->right,block_ptr);
	}
	else
	 {
		 /*that means the free block of this size already exist in the tree, we'll just increase the count of it*/
		 block_ptr ->count = block_ptr ->count + 1;
		 return root;
	}
	/*Now update the height of node here and perform rotation if required.*/
	
	root ->height = 1 + get_max_height(root ->left,root ->right);
	int height_difference = get_left_right_height_difference(root);

	/*Here arises 4 standard cases of AVL tree i.e, LL, RR, LR, RL*/	

    if (height_difference >= 2 && get_left_right_height_difference(root->left) >= 0) /*LL*/
        return right_rotate(root); 
   
    else if (height_difference >= 2 && get_left_right_height_difference(root->left) < 0) /*LR*/
    { 
        root->left =  left_rotate(root->left); 
        return right_rotate(root); 
    } 

    else if (height_difference <= -2 && get_left_right_height_difference(root->right) <= 0) /*RR*/
        return left_rotate(root); 
  
    else if (height_difference <= -2 && get_left_right_height_difference(root->right) > 0) /*RL*/
    { 
        root->right = right_rotate(root->right); 
        return left_rotate(root); 
    } 
	return root;
}

/*this function will give the smallest size greater than requirement (if present, otherwise NULL)*/
void * best_fit_in_Avl_tree(free_block_header_ptr root,int size)
{
	
	if(root == NULL) return NULL;

	if(root ->size > size)
	{
		void * current_best_fit_ans = root;
		void * left_ans = best_fit_in_Avl_tree(root ->left,size);
		void* my_ans =  (left_ans == NULL)? current_best_fit_ans : left_ans;
		return my_ans;
 	}
	else if(root ->size < size)
	{
		void * right_ans = best_fit_in_Avl_tree(root ->right, size);
		return right_ans;
	}
	else
	{
		/*That means we got the perfect size*/
		return root;
	}
	
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
		tree_root = delete_block_from_tree(tree_root,prev_block_ptr,tree_root,-1);
		tree_root = delete_block_from_tree(tree_root,next_block_ptr,tree_root,-1); 

		Header(prev_block_ptr) = Header(prev_block_ptr) + (Header(block_ptr) & -2) + Header(next_block_ptr);
		Footer(prev_block_ptr) = Header(prev_block_ptr);
		return prev_block_ptr;	
	} 
	else if(!is_prev_allocated && is_next_allocated)
	{
		/*Delete the prev block from freelist to maintain consistency*/
		tree_root = delete_block_from_tree(tree_root,prev_block_ptr,tree_root,-1);

		Header(prev_block_ptr) = Header(prev_block_ptr) + (Header(block_ptr) & -2);
		Footer(prev_block_ptr) = Header(prev_block_ptr);
		return prev_block_ptr;
	}
	else if(is_prev_allocated && !is_next_allocated)
	{
		/*Delete the next block from freelist to maintain consistency*/
		tree_root = delete_block_from_tree(tree_root,next_block_ptr,tree_root,-1);

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
	tree_root = NULL;
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


	// Before allocating size find in the freelist (Tree).
	void* block_ptr = best_fit_in_Avl_tree(tree_root,size_include_payload);     /* This function will give you the best size block in O(logn) time.*/
	if(block_ptr == NULL)  /* That means we didn't get anything from the freelist (AVL Tree)*/
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
		/*Here we need to do the splitting of the block we got from best_fit_in_Avl_tree*/
		if((Header(block_ptr) - size_include_payload) >= MIN_BLOCK_SIZE )
		{
			/*Here we are tolerating some extent of internal fragmentation*/
			int old_block_size = Header(block_ptr);
			Header(block_ptr) = size_include_payload | 1;
			Footer(block_ptr) = size_include_payload | 1;


			void * new_free_block_ptr = (char*)block_ptr + (Header(block_ptr) & -2);
			Header(new_free_block_ptr) = old_block_size - size_include_payload;
			Footer(new_free_block_ptr) = old_block_size - size_include_payload;
			// printf("New block Address %p\n", new_free_block_ptr);
			// printf("New block size %d\n", Header(new_free_block_ptr));

			/*Now Delete the old block from free list (Avl Tree)*/			
			tree_root = delete_block_from_tree(tree_root,block_ptr,tree_root,-1);

			/*Add the new free block into the free list*/
			tree_root = insert_block_in_tree(tree_root,new_free_block_ptr);
		}
		else
		{
			/*Mark the block allocated and remove it from the freelist.*/

			Header(block_ptr) = Header(block_ptr) | 1;
			Footer(block_ptr) = Footer(block_ptr) | 1;

			tree_root = delete_block_from_tree(tree_root,block_ptr,tree_root,-1);
		}
	}
	/*

	Uncomment this code to see the heap after every malloc request.

	printf("\n---------------------Malloc Malloc Request %d------------------\n",++count + 4);
	print_heap_condition();
	printf("\n---------------------FreeList Inorder----------------------\n");
	print_inorder(tree_root);
	printf("\n");
	*/
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
		|Block)	    | count, height  |  block    |
		----------------------------------------- 	
    */
	void* block_ptr = Block_ptr(ptr);
	block_ptr = coaelesce_next_and_prev_free_block(block_ptr);

	/*Mark this block as Free Block*/
	Header(block_ptr) = Header(block_ptr) & -2;
	Footer(block_ptr) = Footer(block_ptr) & -2;

	/*Now add this free block to tree -- O(logn) operation*/
	tree_root = insert_block_in_tree(tree_root,block_ptr);
	
	/*
	Uncomment this code to see the heap after every Free Request.. 

	printf("\n---------------------Printing Free Request %d------------------\n",++count + 4);
	print_heap_condition();
	printf("\n---------------------FreeList_Inorder----------------------\n");
	print_inorder(tree_root); 
	*/
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














