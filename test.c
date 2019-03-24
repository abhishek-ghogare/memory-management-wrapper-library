#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mmwl.h"

int main () {
	char* str1 = (char *) malloc(60);
	char* str2 = (char *) malloc(1200);
	char* str3 = NULL;

	strcpy(str1, "test string 1");
	strcpy(str2, "test string 2");
	*(str1+61) ='X';
	printf("%s\n", str1);
	printf("%s\n", str2);

	str3 = realloc(str2, 2000);

	strcpy(str3, "test string 3");
	printf("%s\n", str3);

	free(str1);
	free(str3);
	return 0;
}
