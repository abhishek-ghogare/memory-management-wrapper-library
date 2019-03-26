# Linux Memory Management Wrapper Library in C
Easy to use [Valgrind](http://valgrind.org/) alternative in Linux Kernel for memory leaks detection and debugging in C. The library wraps Linux Kernel & user side `[k]malloc`, `[k]realloc`, `[k]calloc`, `[k]free` functions and tracks memory allocations. 
Function `void mmwl_status (void);` prints list of allocated memory which are not freed till the point; with useful information like where the memory was allocated (filename, function name and line number) and stack trace at the time of allocation.
In addition to memory leaks, library can also detect possible out of bound writes to the allocated memory or invalid address passed to `[k]free` call by comparing the block signatures.

## How to use
MMWL is very easy to use in both user & kernel side.
 - Include `mmwl.h` header file in your program
 - Call `void mmwl_status (void);` function at the exit point of the program

### User program example
```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mmwl.h"

int main () {
	char* str1 = (char *) malloc(60);
	str1 = realloc(str2, 2000);
	
	free(str1);

	mmwl_status();
	return 0;
}
```

### Kernel program example
```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include "mmwl.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("<AUTHOR>");
MODULE_DESCRIPTION("Test module for memory management wrapper library");

static int *test_mem1;

int __init init_module(void) {
	test_mem1 = (int *)kmalloc(300, GFP_KERNEL);
	test_mem1 = krealloc(test_mem1, 500, GFP_KERNEL);
	return 0;
}

void __exit cleanup_module(void) {
	kfree(test_mem1);
	mmwl_status();
}
```

### Sample log
```
mmwl:inf: *** mmwl statistics START ***
mmwl:inf: total alloc count       : 3
mmwl:inf: total free count        : 2
mmwl:inf: total size allocated    : 3260
mmwl:inf: total size freed        : 1260
mmwl:inf: current allocated size  : 2000
mmwl:inf: current alloc count     : 1
mmwl:inf: list of allocations :
mmwl:inf: addr:0x55a30b2d61d0 size:2000 @main:17 in test.c
mmwl:err: signature mismatch, addr:0x55a30b2d5450
mmwl:err: addr:0x55a30b2d5450 size:60 @main:7 in test.c
mmwl:inf: *** mmwl statistics END ***
```
