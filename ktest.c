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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include "mmwl.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Abhishek Ghogare <abhishek.ghogare@gmail.com>");
MODULE_DESCRIPTION("Test module for memory management wrapper library");


static int *test_mem1;
//static void *test_mem2;

int __init init_module(void) {
	test_mem1 = (int *)kmalloc(300, GFP_KERNEL);
	test_mem1[2] = 0;
	test_mem1 = krealloc(test_mem1, 500, GFP_KERNEL);
	test_mem1[-6] = 12;
	test_mem1 = (int *)kmalloc(400, GFP_KERNEL);
	test_mem1 = (int *)kmalloc(600, GFP_KERNEL);
	printk(KERN_INFO "test module inserted");
	return 0;
}

void __exit cleanup_module(void) {
	kfree(test_mem1);
	mmwl_status();
	printk(KERN_INFO "cleaning up test module");
}
