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

#ifndef MMWL_H_
#define MMWL_H_
#include "mmwl_core.h"

#ifdef __KERNEL__

#include <linux/kernel.h>
#include <linux/slab.h>
#define kmalloc(size, flags)		mmwl_malloc(size, flags, __FILE__, __FUNCTION__, __LINE__)
#define krealloc(ptr, size, flags)	mmwl_realloc(ptr, size, flags, __FILE__, __FUNCTION__, __LINE__)
#define kcalloc(blocks, size)		mmwl_malloc((blocks)*(size), flags, __FILE__, __FUNCTION__, __LINE__)
#define kfree(ptr)			mmwl_free(ptr, __FILE__, __FUNCTION__, __LINE__)

#else /* __KERNEL__ */

#include <stdlib.h>
#define malloc(size)			mmwl_malloc(size, __FILE__, __FUNCTION__, __LINE__)
#define realloc(ptr, size)		mmwl_realloc(ptr, size, __FILE__, __FUNCTION__, __LINE__)
#define calloc(blocks, size)		mmwl_malloc((blocks)*(size), __FILE__, __FUNCTION__, __LINE__)
#define free(ptr)			mmwl_free(ptr, __FILE__, __FUNCTION__, __LINE__)

#endif /* __KERNEL__ */

#endif /* MMWL_H_ */
