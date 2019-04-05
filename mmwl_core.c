/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/* * Copyright (C) 2019 Abhishek Ghogare <abhishek.ghogare@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "mmwl_core.h"


#define STACK_DUMP_DEPTH	32	// Depth of the stack dump to be stored
#define FILENAME_SIZE		300	// Size of filename to be maintained
#define FUNCNAME_SIZE		100	// Size of function name to be maintained

// TODO::add freed block signature to determine if the block was already freed
#define SIGNx1 0x4B1D4B1D
#define SIGNx2 SIGNx1, SIGNx1
#define SIGNx4 SIGNx2, SIGNx2
#define SIGNx8 SIGNx4, SIGNx4
#define SIGNx16 SIGNx8, SIGNx8
#define SIGNx32 SIGNx16, SIGNx16

#define SIGN_COMPx1(sign_array, X) (sign_array[X] == SIGNx1)
#define SIGN_COMPx2(sign_array, X) (SIGN_COMPx1(sign_array, X) && SIGN_COMPx1(sign_array, X+1))
#define SIGN_COMPx4(sign_array, X) (SIGN_COMPx2(sign_array, X) && SIGN_COMPx2(sign_array, X+2))
#define SIGN_COMPx8(sign_array, X) (SIGN_COMPx4(sign_array, X) && SIGN_COMPx4(sign_array, X+4))
#define SIGN_COMPx16(sign_array, X) (SIGN_COMPx8(sign_array, X) && SIGN_COMPx8(sign_array, X+8))
#define SIGN_COMPx32(sign_array, X) (SIGN_COMPx16(sign_array, X) && SIGN_COMPx16(sign_array, X+16))


#define SIGNATURE_SIZE 8						// Block signature size
#define SIGN_COMP(sign_array) SIGN_COMPx8(sign_array, 0)		// Validates signature
static const unsigned long SIGNATURE[SIGNATURE_SIZE] = {SIGNx8};	// Signature to validate the allocated block

#ifdef __KERNEL__
// Kernel side macros
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sched/debug.h>
#include <linux/stacktrace.h>
MODULE_LICENSE("Dual MIT/GPL");				// Kernel module license
#define MMWL_MUTEX_LOCK(lock) spin_lock(lock)		// Kernel side mutex lock
#define MMWL_MUTEX_UNLOCK(lock) spin_unlock(lock)	// Kernel side mutex unlock
#define assert(X) BUG_ON(!(X))

#else /* __KERNEL__ */
// User side macros
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

/*
 *	User-side implementation of linked list
 */
#define LIST_HEAD_INIT(name) { &(name), &(name) }	// Initializes list head
#define container_of(ptr, type, member)		(type *)( (char *)ptr - (size_t)&((type *)0)->member)
#define list_entry(ptr, type, member)		container_of(ptr, type, member)

struct list_head {
	struct list_head *next, *prev;
};
// Initializes already declared list head
static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}
// Adds a list node at the end of the linked list
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	new->next = head;
	new->prev = head->prev;
	head->prev->next = new;
	head->prev = new;
}
// Removes a list node from the linked list
static inline void list_del(struct list_head * entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	entry->next = entry->prev = NULL;
}

#define MMWL_MUTEX_LOCK(lock) pthread_mutex_lock(lock)		// User side mutex lock
#define MMWL_MUTEX_UNLOCK(lock) pthread_mutex_unlock(lock)	// User side mutex unlock

#endif /* __KERNEL__ */







/*
 *	MMWL instance type
 */
struct mmwl_instance {
	unsigned long long alloc_count;		// Number of allocated blocks
	unsigned long long free_count;		// Number of freed blocks
	unsigned long long alloc_size;		// Total size allocated
	unsigned long long free_size;		// Total size freed
#ifdef __KERNEL__
	spinlock_t lock;			// Kernel side mutex lock
#else
	pthread_mutex_t lock;			// User side mutex lock
#endif
	struct list_head head;			// Head of the linked list of blocks
};


/*
 *	MMWL global instance
 */
struct mmwl_instance mmwl_gbl_inst = {
	.alloc_count	= 0,
	.free_count	= 0,
	.alloc_size	= 0,
	.free_size	= 0,
#ifdef __KERNEL__
	.lock		= __SPIN_LOCK_UNLOCKED(mmwl_gbl_inst.lock),
#else
	.lock		= PTHREAD_MUTEX_INITIALIZER,
#endif
	.head		= LIST_HEAD_INIT(mmwl_gbl_inst.head)
};

/*
 *	Header of allocated block entry
 */
struct block_header {
	struct			list_head head;
	//unsigned long 	stack_entries[STACK_DUMP_DEPTH];
	//struct stack_trace 	trace;
	char			filename[FILENAME_SIZE];	// Name of the source file from where block was allocated
	char			func_name[FUNCNAME_SIZE];	// Name of the function from which block was allocated
	unsigned int		line_num;			// Line number in source file where block was allocated
	size_t 			size;				// Size of the user block allocated
	unsigned long		signature[SIGNATURE_SIZE];	// For validating the header
	char			end[];				// Start of the user block
};

/*
 *	Footer of allocated block entry
 */
struct block_footer {
	unsigned long signature[SIGNATURE_SIZE];		// For validating the footer
};



// Size to be allocated including header and footer
#define BLOCK_SIZE(size)	(size+sizeof(struct block_header)+sizeof(struct block_footer))


// Returns pointer to the header when pointer to the user block provided
#define BLOCK_HEADER_OF(ptr)	((struct block_header*)((void*)ptr - (size_t)&((struct block_header *)0)->end))
// returns pointer to footer when pointer to the user block provided
#define BLOCK_FOOTER_OF(ptr)	((struct block_footer*)((void*)ptr + BLOCK_HEADER_OF(ptr)->size))


// Set signature for allocated block
#define SIGNATURE_SET(block)									\
	do {											\
		memcpy(block->signature, SIGNATURE, sizeof(SIGNATURE));				\
		memcpy(BLOCK_FOOTER_OF(block->end)->signature, SIGNATURE, sizeof(SIGNATURE));	\
	} while(0)
// Erase signature of block when freed
#define SIGNATURE_ERASE(block)									\
	do {											\
		memset(block->signature, 0, sizeof(SIGNATURE));					\
		memset(BLOCK_FOOTER_OF(block->end)->signature, 0, sizeof(SIGNATURE));		\
	} while(0)


// Initializes block header & footer
#define INIT_BLOCK(block, alloc_size, filename, func_name, line_num)				\
	do {											\
		INIT_LIST_HEAD(&block->head);							\
		strncpy(block->filename, filename, FILENAME_SIZE-1);				\
		strncpy(block->func_name, func_name, FUNCNAME_SIZE-1);				\
		block->line_num = line_num;							\
		block->size = alloc_size;							\
		SIGNATURE_SET(block);								\
	} while(0)


// Initializes block header & footer
#define INIT_BLOCK_OF(ptr, alloc_size, filename, func_name, line_num)				\
	INIT_BLOCK(BLOCK_HEADER_OF(ptr), alloc_size, filename, func_name, line_num)


// Boolean expression to verify the signature of the block
#define CHECK_SIGN_OF(ptr)	((SIGN_COMP(BLOCK_HEADER_OF(ptr)->signature) && 		\
				SIGN_COMP(BLOCK_FOOTER_OF(ptr)->signature)))
#define CHECK_SIGN(block)	((SIGN_COMP(block->signature) && 				\
				SIGN_COMP(BLOCK_FOOTER_OF(block->end)->signature)))


// Assert consistency of the allocation statistics
#define MMWL_BASIC_ASSERT									\
	do {											\
		MMWL_MUTEX_LOCK(&mmwl_gbl_inst.lock);						\
		assert(										\
			(mmwl_gbl_inst.head.next 	== &(mmwl_gbl_inst.head)		\
			&& mmwl_gbl_inst.alloc_count 	== mmwl_gbl_inst.free_count		\
			&& mmwl_gbl_inst.alloc_size 	== mmwl_gbl_inst.free_size)		\
			||									\
			(mmwl_gbl_inst.head.next 	!= &(mmwl_gbl_inst.head)		\
			&& mmwl_gbl_inst.alloc_count 	> mmwl_gbl_inst.free_count		\
			&& mmwl_gbl_inst.alloc_size 	> mmwl_gbl_inst.free_size)		\
		);										\
		MMWL_MUTEX_UNLOCK(&mmwl_gbl_inst.lock);						\
	} while(0)



/*
 *	Add block to the allocation list
 */
static void add_malloc_entry (	struct	block_header * 	block,			// Pointer to block
					size_t		size,			// Size of the user block
				const	char *		filename,		// Filename from where alloc was called
				const	char *		func_name,		// Function name from which alloc was called
				const	unsigned int	line_num )		// Line number of alloc function call
{
	INIT_BLOCK(block, size, filename, func_name, line_num);			// Initialize block
	MMWL_MUTEX_LOCK(&mmwl_gbl_inst.lock);					// Lock MMWL instance
	list_add_tail(&block->head, &mmwl_gbl_inst.head);			// Add new block at the end of the list
	mmwl_gbl_inst.alloc_count++;						// Increment allocation count
	mmwl_gbl_inst.alloc_size += (unsigned long long)size;			// Add user block size to total allocation size
	MMWL_MUTEX_UNLOCK(&mmwl_gbl_inst.lock);					// Unlock MMWL instance
}




/*
 *	Remove block from allocation list
 */
static void remove_malloc_entry (	struct	block_header * 	block,		// Pointer to block
					const	char *		filename,	// Filename from where alloc was called
					const	char *		func_name,	// Function name from which alloc was called
					const	unsigned int	line_num )	// Line number of alloc function call
{
	if(!CHECK_SIGN(block)) {						// Verify block signature
		// Signature mismatch for following reasons:
		//	1. Signature was overwritten by the user program, suggesting out of bound write.
		//	2. Wrong pointer was given to "free" call
		//	3. The block was already freed, hence multiple "free" called on same address
		MMWL_LOG_ERROR("caller @%s:%u in %s"
				, func_name
				, line_num
				, filename);
		MMWL_LOG_ERROR("\tsignature mismatch, addr:%p"
				, block->end);
		MMWL_LOG_ERROR("\tblock origin @%s:%u in %s"
				, block->func_name
				, block->line_num
				, block->filename);
		// TODO::do not free block
	}

	SIGNATURE_ERASE(block);							// Erase signature
	MMWL_MUTEX_LOCK(&mmwl_gbl_inst.lock);					// Lock MMWL instance
	list_del(&block->head);							// Remove block from allocation list
	mmwl_gbl_inst.free_count++;						// Increment free count
	mmwl_gbl_inst.free_size += block->size;					// Add user block size to total freed size
	MMWL_MUTEX_UNLOCK(&mmwl_gbl_inst.lock);					// Unlock MMWL instance
}





/*
 *	Wrapper function for malloc & kmalloc
 */
void * mmwl_malloc (		size_t		size,				// Size of the user block
		#ifdef __KERNEL__
				gfp_t		flags,				// kmalloc flags
		#endif
			const	char *		filename,			// Filename from where alloc was called
			const	char *		func_name,			// Function name from which alloc was called
			const	unsigned int	line_num )			// Line number of alloc function call
{
	struct block_header * block = NULL;

	// Call wrapped function
	#ifdef __KERNEL__
	block = (struct block_header *) kmalloc(BLOCK_SIZE(size), flags);
	#else
	block = (struct block_header *) malloc(BLOCK_SIZE(size));
	#endif

	if (block != NULL)
	{
		add_malloc_entry(block, size, filename, func_name, line_num);
		MMWL_BASIC_ASSERT;
		return (void*)block->end;
	}

	MMWL_BASIC_ASSERT;
	return block;
}




/*
 *	Wrapper function for realloc & krealloc
 */
void * mmwl_realloc (	void *		ptr,					// User block address to be 'realloc'ated
			size_t		size,					// Size of the user block
	#ifdef __KERNEL__
			gfp_t 		flags,					// kmalloc flags
	#endif
		const	char *		filename,				// Filename from where realloc was called
		const	char *		func_name,				// Function name from which realloc was called
		const	unsigned int	line_num )				// Line number of realloc function call
{
	struct block_header * block_old = BLOCK_HEADER_OF(ptr);
	struct block_header * block_new = NULL;
	remove_malloc_entry(block_old, filename, func_name, line_num);

	// Call wrapped function
	#ifdef __KERNEL__
	block_new = krealloc(block_old, BLOCK_SIZE(size), flags);
	#else
	block_new = realloc(block_old, BLOCK_SIZE(size));
	#endif

	if (block_new != NULL)
	{
		add_malloc_entry(block_new, size, filename, func_name, line_num);
		MMWL_BASIC_ASSERT;
		return (void*)block_new->end;
	}

	MMWL_BASIC_ASSERT;
	return block_new;
}




/*
 *	Wrapper function for free & kfree
 */
void mmwl_free (	void *		ptr,					// User block address to be freed
		const	char *		filename,				// Filename from where free was called
		const	char *		func_name,				// Function name from which free was called
		const	unsigned int	line_num )				// Line number of function call
{
	struct block_header * block = BLOCK_HEADER_OF(ptr);

	remove_malloc_entry(block, filename, func_name, line_num);


	// Call wrapped function
	#ifdef __KERNEL__
	kfree(block);
	/*
	{
		static unsigned long entries[32];
		static struct stack_trace trace = {
			.entries = entries,
			.max_entries = ARRAY_SIZE(entries),
		};

		trace.nr_entries = 0;
		trace.skip = 0; // skip the last two entries
		save_stack_trace(&trace);

		print_stack_trace(&trace, 0);
	}
	*/
	#else
	free(block);
	#endif
	MMWL_BASIC_ASSERT;
}




/*
 *	Print current status of allocation tracking list
 */
void mmwl_status (void)
{
	struct list_head *iter = NULL;

	MMWL_MUTEX_LOCK(&mmwl_gbl_inst.lock);
	MMWL_LOG_INFO("*** mmwl statistics START ***");
	MMWL_LOG_INFO("total alloc count       : %llu", mmwl_gbl_inst.alloc_count);
	MMWL_LOG_INFO("total free count        : %llu", mmwl_gbl_inst.free_count);
	MMWL_LOG_INFO("total size allocated    : %llu", mmwl_gbl_inst.alloc_size);
	MMWL_LOG_INFO("total size freed        : %llu", mmwl_gbl_inst.free_size);
	MMWL_LOG_INFO("current allocated size  : %llu", mmwl_gbl_inst.alloc_size-mmwl_gbl_inst.free_size);
	MMWL_LOG_INFO("current alloc count     : %llu", mmwl_gbl_inst.alloc_count-mmwl_gbl_inst.free_count);

	// Print list of allocated blocks if list is not empty
	if(mmwl_gbl_inst.head.next == &(mmwl_gbl_inst.head))
	{
		MMWL_LOG_INFO("list of allocations is empty");
	}
	else
	{
		MMWL_LOG_INFO("list of allocations     :");
		for(iter = mmwl_gbl_inst.head.next ; iter != &mmwl_gbl_inst.head ; iter = iter->next)
		{
			struct block_header * block = list_entry(iter, struct block_header, head);
			// Verify signature
			if(!CHECK_SIGN(block))
			{
				MMWL_LOG_ERROR("\taddr:%p size:%lu block origin @%s:%u in %s <signature mismatch>"
						, block->end
						, block->size
						, block->func_name
						, block->line_num
						, block->filename);
			}
			else
			{
				MMWL_LOG_INFO ("\taddr:%p size:%lu block origin @%s:%u in %s"
						, block->end
						, block->size
						, block->func_name
						, block->line_num
						, block->filename);
			}
		}
	}
	MMWL_LOG_INFO("*** mmwl statistics END ***");
	MMWL_MUTEX_UNLOCK(&mmwl_gbl_inst.lock);
	MMWL_BASIC_ASSERT;
}
