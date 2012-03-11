#ifndef IOREMAP_H
#define	IOREMAP_H

void * ioremap(unsigned long physaddr, unsigned size, int sync);
int iounmap(volatile void *start, size_t length);

#endif	/* IOREMAP_H */

