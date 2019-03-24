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

#ifndef MMWL_CORE_H_
#define MMWL_CORE_H_

#ifdef __KERNEL__
#define MMWL_LOG_INFO(log, args...)		printk(KERN_INFO "mmwl:info: " log "\n", ## args)
#define MMWL_LOG_ERROR(log, args...)	printk(KERN_ERR "mmwl:error: " log "\n", ## args)
#else
#define MMWL_LOG_INFO(log, args...)		fprintf(stdout, "mmwl:info: " log "\n", ## args)
#define MMWL_LOG_ERROR(log, args...)	fprintf(stderr, "mmwl:error: " log "\n", ## args)
#endif


void * mmwl_malloc (		size_t			size,
#ifdef __KERNEL__
							gfp_t 			flags,
#endif
					const	char *			filename,
					const	char *			func_name,
					const	unsigned int	line_num
);

void * mmwl_realloc (		void *			ptr,
							size_t			size,
#ifdef __KERNEL__
							gfp_t 			flags,
#endif
					const	char *			filename,
					const	char *			func_name,
					const	unsigned int	line_num
);

void mmwl_free (			void *			ptr,
					const	char *			filename,
					const	char *			func_name,
					const	unsigned int	line_num
);

#endif //MMWL_CORE_H_
