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


#define STACK_DUMP_DEPTH	32
#define FILENAME_SIZE		300
#define FUNCNAME_SIZE		100

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

/*
 *	Block signature size
 */
#define SIGNATURE_SIZE 8
#define SIGN_COMP(sign_array) SIGN_COMPx8(sign_array, 0)
static const unsigned long SIGNATURE[SIGNATURE_SIZE] = {SIGNx8};

#ifdef __KERNEL__
/*
 *	Kernel side macros
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sched/debug.h>
#include <linux/stacktrace.h>
MODULE_LICENSE("Dual MIT/GPL");
#define MMWL_MUTEX_LOCK(lock) spin_lock(lock)
#define MMWL_MUTEX_UNLOCK(lock) spin_unlock(lock)

#else /* __KERNEL__ */

/*
 *	User side macros
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct list_head {
	struct list_head *next, *prev;
};

static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	new->next = head;
	new->prev = head->prev;
	head->prev->next = new;
	head->prev = new;
}
static inline void list_del(struct list_head * entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	entry->next = entry->prev = NULL;
}

#define container_of(ptr, type, member)		(type *)( (char *)ptr - (size_t)&((type *)0)->member)
#define list_entry(ptr, type, member)		container_of(ptr, type, member)

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define MMWL_MUTEX_LOCK(lock) pthread_mutex_lock(lock)
#define MMWL_MUTEX_UNLOCK(lock) pthread_mutex_unlock(lock)

#endif /* __KERNEL__ */


/*
 *	MMWL instance type
 */
struct mmwl_instance {
	unsigned long long alloc_count;
	unsigned long long free_count;
	unsigned long long alloc_size;
	unsigned long long free_size;
#ifdef __KERNEL__
	spinlock_t lock;
#else
	pthread_mutex_t lock;
#endif
	struct list_head head;
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
	char			filename[FILENAME_SIZE];
	char			func_name[FUNCNAME_SIZE];
	unsigned int		line_num;
	size_t 			size;
	unsigned long		signature[SIGNATURE_SIZE];
	char			end[];
};

/*
 *	Footer of allocated block entry
 */
struct block_footer {
	unsigned long signature[SIGNATURE_SIZE];
};



// Size to be allocated including header and footer
#define BLOCK_SIZE(size)	(size+sizeof(struct block_header)+sizeof(struct block_footer))

// Pointer to header
#define BLOCK_HEADER_OF(ptr)	((struct block_header*)((void*)ptr - (size_t)&((struct block_header *)0)->end))

// Pointer to footer
#define BLOCK_FOOTER_OF(ptr)	((struct block_footer*)((void*)ptr + BLOCK_HEADER_OF(ptr)->size))
#define INIT_BLOCK(block, alloc_size, filename, func_name, line_num)				\
	do {											\
		INIT_LIST_HEAD(&block->head);							\
		strcpy(block->filename, filename);						\
		strcpy(block->func_name, func_name);						\
		block->line_num = line_num;							\
		block->size = alloc_size;							\
		memcpy(block->signature, SIGNATURE, sizeof(SIGNATURE));				\
		memcpy(BLOCK_FOOTER_OF(block->end)->signature, SIGNATURE, sizeof(SIGNATURE));	\
	} while(0)

// Initialize block header & footer
#define INIT_BLOCK_OF(ptr, alloc_size, filename, func_name, line_num)				\
	INIT_BLOCK(BLOCK_HEADER_OF(ptr), alloc_size, filename, func_name, line_num)

#define CHECK_SIGN_OF(ptr)	((SIGN_COMP(BLOCK_HEADER_OF(ptr)->signature) && 		\
				SIGN_COMP(BLOCK_FOOTER_OF(ptr)->signature)))
#define CHECK_SIGN(block)	((SIGN_COMP(block->signature) && 				\
				SIGN_COMP(BLOCK_FOOTER_OF(block->end)->signature)))

// Prints block details
#define print_block_details(log_level, block)		\
	MMWL_LOG_ ## log_level("addr:%p size:%lu @%s:%u in %s", block->end, block->size, block->func_name, block->line_num, block->filename)

#define print_block_details_of(log_level, ptr) 		\
	print_block_details(log_level, BLOCK_HEADER_OF(ptr))



// Verify signature of header & footer
static inline int verify_signature (struct block_header *block)
{
	if( !CHECK_SIGN(block) )
	{
		MMWL_LOG_ERROR("signature mismatch, addr:%p", block->end);
		return -1;
	}

	return 0;
}
#define verify_signature_of (void *ptr) 	verify_signature(BLOCK_HEADER_OF(ptr))


static void add_malloc_entry (	struct	block_header * 	block,
					size_t		size,
				const	char *		filename,
				const	char *		func_name,
				const	unsigned int	line_num )
{
	INIT_BLOCK(block, size, filename, func_name, line_num);
	MMWL_MUTEX_LOCK(&mmwl_gbl_inst.lock);
	list_add_tail(&block->head, &mmwl_gbl_inst.head);
	mmwl_gbl_inst.alloc_count++;
	mmwl_gbl_inst.alloc_size += (unsigned long long)size;
	MMWL_MUTEX_UNLOCK(&mmwl_gbl_inst.lock);
}

static void remove_malloc_entry (	struct	block_header * 	block,
					const	char *		filename,
					const	char *		func_name,
					const	unsigned int	line_num )
{	
	if(verify_signature(block) != 0) {
		print_block_details(ERROR, block);
		// TODO::do not free block
	}

	MMWL_MUTEX_LOCK(&mmwl_gbl_inst.lock);
	list_del(&block->head);
	mmwl_gbl_inst.free_count++;
	mmwl_gbl_inst.free_size += block->size;
	MMWL_MUTEX_UNLOCK(&mmwl_gbl_inst.lock);
}



/*
 *	Wrapper function for malloc & kmalloc
 */
void * mmwl_malloc (		size_t		size,
		#ifdef __KERNEL__
				gfp_t		flags,
		#endif
			const	char *		filename,
			const	char *		func_name,
			const	unsigned int	line_num )
{
	struct block_header * block = NULL;

	#ifdef __KERNEL__
	block = (struct block_header *) kmalloc(BLOCK_SIZE(size), flags);
	#else
	block = (struct block_header *) malloc(BLOCK_SIZE(size));
	#endif

	if (block != NULL)
	{
		add_malloc_entry(block, size, filename, func_name, line_num);
		return (void*)block->end;
	}

	return block;
}

/*
 *	Wrapper function for realloc & krealloc
 */
void * mmwl_realloc (	void *		ptr,
			size_t		size,
	#ifdef __KERNEL__
			gfp_t 		flags,
	#endif
		const	char *		filename,
		const	char *		func_name,
		const	unsigned int	line_num )
{
	struct block_header * block_old = BLOCK_HEADER_OF(ptr);
	struct block_header * block_new = NULL;
	remove_malloc_entry(block_old, filename, func_name, line_num);

	#ifdef __KERNEL__
	block_new = krealloc(block_old, BLOCK_SIZE(size), flags);
	#else
	block_new = realloc(block_old, BLOCK_SIZE(size));
	#endif

	if (block_new != NULL)
	{
		add_malloc_entry(block_new, size, filename, func_name, line_num);
		return (void*)block_new->end;
	}

	return block_new;
}

/*
 *	Wrapper function for free & kfree
 */
void mmwl_free (	void *		ptr,
		const	char *		filename,
		const	char *		func_name,
		const	unsigned int	line_num )
{
	struct block_header * block = BLOCK_HEADER_OF(ptr);
	
	remove_malloc_entry(block, filename, func_name, line_num);

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
}


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

	MMWL_LOG_INFO("list of allocations     :");
	for(iter = mmwl_gbl_inst.head.next ; iter != &mmwl_gbl_inst.head ; iter = iter->next)
	{
		struct block_header * block = list_entry(iter, struct block_header, head);
		if(verify_signature(block) != 0) 
			print_block_details(ERROR, block);
		else 
			print_block_details(INFO, block);
	}
	MMWL_LOG_INFO("*** mmwl statistics END ***");
	MMWL_MUTEX_UNLOCK(&mmwl_gbl_inst.lock);
}
