// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/env.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "display current function call stack", mon_backtrace },
	{ "continue", "continue from breakpoint", mon_break_continue },
	{ "si", "continue from breakpoint", mon_break_step },
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	uint32_t *ebp=(uint32_t *)read_ebp();
	struct Eipdebuginfo info={0};
	cprintf("Stack backtrace:\n");
	while (ebp!=0)
	{
		cprintf(
			"  ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n",
			ebp,
			ebp[1],
			ebp[2],
			ebp[3],
			ebp[4],
			ebp[5],
			ebp[6]
		);
		if(debuginfo_eip(ebp[1],&info)==-1){
			return -1;
		}
		
		cprintf(
			"       %s:%d: %.*s+%d\n",
			info.eip_file,
			info.eip_line,
			info.eip_fn_namelen,
			info.eip_fn_name,
			ebp[1]-info.eip_fn_addr
		);

		ebp=(uint32_t *)ebp[0];
	}
	
	

	return 0;
}

int
mon_break_continue(int argc, char **argv, struct Trapframe *tf)
{
	if(tf->tf_trapno!=T_BRKPT&&tf->tf_trapno!=T_DEBUG){
		return -1;
	}
	
	//asm volatile("jmp %0"::"r" (tf->tf_eip));
	asm volatile(
		"\tmovl %0,%%esp\n"
		"\tpopal\n"
		"\tpopl %%es\n"
		"\tpopl %%ds\n"
		"\taddl $0x8,%%esp\n" /* skip tf_trapno and tf_errcode */
		"\tandl $0x11111011, 8(%%esp)\n"
		"\tiret\n"
		: : "g" (tf) : "memory");
		
	env_pop_tf(tf);

	return -1;
}

int
mon_break_step (int argc, char **argv, struct Trapframe *tf){
	if(tf->tf_trapno!=T_BRKPT&&tf->tf_trapno!=T_DEBUG){
		return -1;
	}
	
	//asm volatile("jmp %0"::"r" (tf->tf_eip));
	
	cprintf("Next instruction eip:%x %x\n",tf->tf_eip,(read_eflags()|FL_TF)&FL_IF);
	asm volatile(
		"\tmovl %0,%%esp\n"
		"\tpopal\n"
		"\tpopl %%es\n"
		"\tpopl %%ds\n"
		"\taddl $0x8,%%esp\n" /* skip tf_trapno and tf_errcode */
		"\torl $0x00000100, 8(%%esp)\n"
		"\tiret\n"
		: : "g" (tf) : "memory");

	
	return -1;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0){
				cprintf("Command failed!\n");
				break;
			}
	}
}
