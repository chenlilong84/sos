/*
 * Entry point for C code. At this point, the MMU is enabled.
 */
#include "kernel.h"

#define VERBOSE false

#define proc_print(msg) do {\
	pid = current->id; \
	get_sp(sp); \
	printf("[pid=%u sp=0x%x]\t" msg, pid, sp); \
} while (0);

void main(uint32_t phys)
{
	puts("Hello world!\n");
	INIT_LIST_HEAD(process_list);

	if (VERBOSE) {
		printf("The physical location of the code was originally 0x%x\n", phys);
		printf("It is now 0x%x\n", &code_start);
	}

	printf("Initializing memory...\n");
	mem_init(phys, VERBOSE);
	printf("Done!\n");

}
