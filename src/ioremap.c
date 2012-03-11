#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

void * ioremap(unsigned long physaddr, unsigned size, int sync)
{
	int axs_mem_fd = -1;
	unsigned long page_addr, ofs_addr, reg, pgmask;
	void* reg_mem = NULL;

	/*
	 * looks like mmap wants aligned addresses?
	 */
	pgmask = getpagesize()-1;
	page_addr = physaddr & ~pgmask;
	ofs_addr  = physaddr & pgmask;

	/*
	 * Don't forget O_SYNC, esp. if address is in RAM region.
	 * Note: if you do know you'll access in Read Only mode,
	 *	pass O_RDONLY to open, and PROT_READ only to mmap
	 */
	if (axs_mem_fd == -1) {
		axs_mem_fd = open("/dev/mem", O_RDWR|(sync ? O_SYNC : 0));
		if (axs_mem_fd < 0) {
				perror("AXS: can't open /dev/mem");
				return NULL;
		}
	}

	/* memory map */
	reg_mem = mmap(
		(caddr_t)reg_mem,
		size+ofs_addr,
		PROT_READ|PROT_WRITE,
		MAP_SHARED,
		axs_mem_fd,
		page_addr
	);
	if (reg_mem == MAP_FAILED) {
		perror("AXS: mmap error");
		close(axs_mem_fd);
		return NULL;
	}

	reg = (unsigned long )reg_mem + ofs_addr;
	return (void *)reg;
}

int iounmap(volatile void *start, size_t length)
{
	unsigned long ofs_addr;
	ofs_addr = (unsigned long)start & (getpagesize()-1);

	/* do some cleanup when you're done with it */
	return munmap((unsigned char*)start-ofs_addr, length+ofs_addr);
}

