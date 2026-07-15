/* Invalid_Code.c - Uses 1/8th the memory as the char hash table
   by Mike Slegeir for Mupen64-GC / MEM2 ver by emu_kidid
 */

#include "Invalid_Code.h"
#include <stdlib.h>

static unsigned char *invalid_code = NULL;

void invalid_code_alloc(void){
	if (!invalid_code) {
		invalid_code = (unsigned char *)malloc(0x100000);
	}
}

void invalid_code_free(void){
	if (invalid_code) { free(invalid_code); invalid_code = NULL; }
}

int inline invalid_code_get(int block_num){
	return invalid_code[block_num];
}

void inline invalid_code_set(int block_num, int value){
	invalid_code[block_num] = value;
}

