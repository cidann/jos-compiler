// program to cause a breakpoint trap

#include <inc/lib.h>

void
umain(int argc, char **argv)
{	
	
	asm volatile("int $3");
	cprintf("hello, world\n");
	asm volatile("int $3");
	for(int i=0;i<10;i++){
		cprintf("After break\n");
	}
	asm volatile("int $3");
}

