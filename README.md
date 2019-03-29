# Linux Memory Management Wrapper Library
Easy to use [Valgrind](http://valgrind.org/) alternative in Linux kernel for memory leaks detection and debugging. The library wraps Linux Kernel & user side `[k]malloc`, `[k]realloc`, `[k]calloc`, `[k]free` functions and tracks memory allocations.
Function `void mmwl_status (void);` prints list of allocated memory which are not freed till the point; with useful information like where the memory was allocated (filename, function name and line number) and stack trace at the time of allocation.
In addition to memory leaks, library can also detect possible out of bound writes to the allocated memory or invalid address passed to `[k]free` call by comparing the block signatures.

## How to use
MMWL is very easy to use in both user & kernel side. Since the function calls `[k]malloc`, `[k]realloc`, `[k]calloc`, `[k]free` are defined as macros in `mmwl.h`, there is no need to replace each of these calls within the code base with the wrapper functions.
 - Include `mmwl.h` header file in your program
 - Call `void mmwl_status (void);` just before the end of the execution
 - Build including `mmwl_core.c` source file

### User program example
```c
...
#include <stdlib.h>
#include "mmwl.h"		/* include library header */

int main () {
	char* str1 = (char *) malloc(60);
	free(str1);

	mmwl_status();		/* see status just before exiting */
	return 0;
}
```

### Kernel program example
```c
....
#include <linux/kernel.h>
....
#include "mmwl.h"		/* include library header */

....
int __init init_module(void) {
	mem = (int *)kmalloc(300, GFP_KERNEL);
	return 0;
}

void __exit cleanup_module(void) {
	kfree(mem);
	mmwl_status();		/* see status just before exiting */
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

## Author
[Abhishek Ghogare](https://github.com/abhishek-ghogare)

## License
This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details
