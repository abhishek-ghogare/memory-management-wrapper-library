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

//static const unsigned long SIGNx1 = 0x4B1D4B1D;
#define SIGNx1 0x4B1D4B1D
#define SIGNx2 SIGNx1, SIGNx1
#define SIGNx4 SIGNx2, SIGNx2
#define SIGNx8 SIGNx4, SIGNx4
#define SIGNx16 SIGNx8, SIGNx8
#define SIGNx32 SIGNx16, SIGNx16

#define SIGN_COMPx1(sign_array, X) sign_array[X] == SIGNx1
#define SIGN_COMPx2(sign_array, X) SIGN_COMPx1(sign_array, X) && SIGN_COMPx1(sign_array, X+1)
#define SIGN_COMPx4(sign_array, X) SIGN_COMPx2(sign_array, X) && SIGN_COMPx2(sign_array, X+2)
#define SIGN_COMPx8(sign_array, X) SIGN_COMPx4(sign_array, X) && SIGN_COMPx4(sign_array, X+4)
#define SIGN_COMPx16(sign_array, X) SIGN_COMPx8(sign_array, X) && SIGN_COMPx8(sign_array, X+8)
#define SIGN_COMPx32(sign_array, X) SIGN_COMPx16(sign_array, X) && SIGN_COMPx16(sign_array, X+16)

#define SIGNATURE_SIZE 8
static const unsigned long SIGNATURE[SIGNATURE_SIZE] = {SIGNx8};
#define SIGN_COMP(sign_array) SIGN_COMPx8(sign_array, 0)

#define STACK_DUMP_DEPTH 32

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sched/debug.h>
#include <linux/stacktrace.h>
MODULE_LICENSE("Dual MIT/GPL");
#define MMWL_MUTEX_LOCK(lock) spin_lock(lock)
#define MMWL_MUTEX_UNLOCK(lock) spin_unlock(lock)
#else
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
	head->prev = new;
	new->next = head;
	new->prev = head->prev;
	head->prev->next = new;
}
static inline void list_del(struct list_head * entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	entry->next = entry->prev = NULL;
}

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define MMWL_MUTEX_LOCK(lock) pthread_mutex_lock(lock)
#define MMWL_MUTEX_UNLOCK(lock) pthread_mutex_unlock(lock)
#endif

#include "mmwl_core.h"

/*
 *	Memory management wrapper library instance
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

struct block_header {
	struct list_head head;
	//unsigned long stack_entries[STACK_DUMP_DEPTH];
	//struct stack_trace trace;
	size_t size;
	unsigned long signature[SIGNATURE_SIZE];
	char end[];
};

struct block_footer {
	unsigned long signature[SIGNATURE_SIZE];
};

#define BLOCK_SIZE(size)	(size+sizeof(struct block_header)+sizeof(struct block_footer))
#define BLOCK_HEADER_OF(ptr)	((struct block_header*)((void*)ptr - (size_t)&((struct block_header *)0)->end))
#define BLOCK_FOOTER_OF(ptr)	((struct block_footer*)((void*)ptr + BLOCK_HEADER_OF(ptr)->size))
#define INIT_BLOCK_OF(ptr, alloc_size)										\
	do {													\
		INIT_LIST_HEAD(&BLOCK_HEADER_OF(ptr)->head);							\
		\
		BLOCK_HEADER_OF(ptr)->size = alloc_size;							\
		memcpy(BLOCK_HEADER_OF(ptr)->signature, SIGNATURE, sizeof(SIGNATURE));				\
		memcpy(BLOCK_FOOTER_OF(ptr)->signature, SIGNATURE, sizeof(SIGNATURE));				\
	} while(0)

#define VERIFY_SIGN_OF(ptr)	(SIGN_COMP(BLOCK_HEADER_OF(ptr)->signature) && \
				SIGN_COMP(BLOCK_FOOTER_OF(ptr)->signature))




void print_block_info(struct block_header *block)
{
	
}


void * mmwl_malloc (		size_t		size,
		#ifdef __KERNEL__
				gfp_t 		flags,
		#endif
			const	char *		filename,
			const	char *		func_name,
			const	unsigned int	line_num )
{
	struct block_header * ptr_new = NULL;

	#ifdef __KERNEL__
	ptr_new = (struct block_header *) kmalloc(BLOCK_SIZE(size), flags);
	#else
	ptr_new = (struct block_header *) malloc(BLOCK_SIZE(size));
	#endif

	if (ptr_new != NULL) {
		INIT_BLOCK_OF(ptr_new->end, size);
		MMWL_MUTEX_LOCK(&mmwl_gbl_inst.lock);
		list_add_tail(&mmwl_gbl_inst.head, &ptr_new->head);
		mmwl_gbl_inst.alloc_count++;
		mmwl_gbl_inst.alloc_size += (unsigned long long)size;
		MMWL_MUTEX_UNLOCK(&mmwl_gbl_inst.lock);
		MMWL_LOG_INFO("allocating %lu size", BLOCK_HEADER_OF(&ptr_new->end)->size);
		return (void*)ptr_new->end;
	}

	return ptr_new;
}

void * mmwl_realloc (		void *		ptr,
				size_t		size,
#ifdef __KERNEL__
				gfp_t 		flags,
#endif
			const	char *		filename,
			const	char *		func_name,
			const	unsigned int	line_num )
{
	struct block_header * ptr_new = NULL;
	MMWL_LOG_INFO("reallocating %lu size", size);
	list_del(&BLOCK_HEADER_OF(ptr)->head);
	if(!VERIFY_SIGN_OF(ptr)) {
		MMWL_LOG_ERROR("Signature mismatch, possible out of bound write detected");
	}
	MMWL_MUTEX_LOCK(&mmwl_gbl_inst.lock);
	mmwl_gbl_inst.free_count++;
	mmwl_gbl_inst.free_size += ((struct block_header *)ptr)->size;
	MMWL_MUTEX_UNLOCK(&mmwl_gbl_inst.lock);

	#ifdef __KERNEL__
	ptr_new = krealloc(BLOCK_HEADER_OF(ptr), BLOCK_SIZE(size), flags);
	#else
	ptr_new = realloc(BLOCK_HEADER_OF(ptr), BLOCK_SIZE(size));
	#endif

	if (ptr_new != NULL) {
		INIT_BLOCK_OF(ptr_new->end, size);
		MMWL_MUTEX_LOCK(&mmwl_gbl_inst.lock);
		list_add_tail(&mmwl_gbl_inst.head, &ptr_new->head);
		mmwl_gbl_inst.alloc_count++;
		mmwl_gbl_inst.alloc_size += (unsigned long long)size;
		MMWL_MUTEX_UNLOCK(&mmwl_gbl_inst.lock);
		return (void*)ptr_new->end;
	}

	return ptr_new;
}

void mmwl_free (	void *		ptr,
		const	char *		filename,
		const	char *		func_name,
		const	unsigned int	line_num )
{
	list_del(&BLOCK_HEADER_OF(ptr)->head);
	if(!VERIFY_SIGN_OF(ptr)) {
		MMWL_LOG_ERROR("Signature mismatch, possible out of bound write detected");
	}

	MMWL_MUTEX_LOCK(&mmwl_gbl_inst.lock);
	mmwl_gbl_inst.free_count++;
	mmwl_gbl_inst.free_size += ((struct block_header *)ptr)->size;
	MMWL_MUTEX_UNLOCK(&mmwl_gbl_inst.lock);

	#ifdef __KERNEL__
	kfree(BLOCK_HEADER_OF(ptr));
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
	free(BLOCK_HEADER_OF(ptr));
	#endif
	MMWL_LOG_INFO("free memory");

}


void mmwl_status (void)
{

}
