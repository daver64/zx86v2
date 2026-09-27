#include "common.h"
#include "bget.h"
#include "kheap.h"
uint32_t heap_end;
uint32_t heap_start;
uint32_t hmem;
uint32_t user_elf_start;
uint32_t user_elf_end;
uint32_t unspecified_start;
uint32_t unspecified_end;
uint32_t libc_heap_start;
uint32_t libc_heap_end;
uint32_t get_libc_heap_start()
{
	return libc_heap_start;
}
uint32_t get_libc_heap_end()
{
	return libc_heap_end;
}
uint32_t get_libc_heap_size()
{
	return libc_heap_end-libc_heap_start;
}
void malloc_init()
{
	uint32_t *heap_start_physical=(uint32_t*)1;
	heap_start = kmalloc_int(0x800000, 1, heap_start_physical);
	heap_end=heap_start + 0x800000;
	//printf("malloc heap start virt=0x%08X : phys=0x%08X\n",heap_start,*((uint32_t*)heap_start_physical));
	bpool((void *)heap_start, (bufsize)(heap_end - heap_start));
	libc_heap_start=heap_start;
	libc_heap_end=heap_end;
	//printf("libc heap start=0x%08X, libc heap end=0x%08X\n",heap_start,heap_end);
}

void *malloc(size_t size)
{
	//printf("malloc %u\n",size);
	return bget(size);
}
extern void *additional_memory;
void *fasm_malloc(size_t size)
{
	//printf("\nfasm malloc %u bytes\n",size);
	void *ptr=malloc(size);
	additional_memory=ptr;
	//printf("pointer=0x%0X\n",(uint32_t)(ptr));
	return ptr;  // Fix: Return the allocated pointer
}
void *calloc(size_t count, size_t size)
{
	return bgetz(count * size);
}

void *realloc(void *ptr, size_t size)
{
	return bgetr(ptr, size);
}

void free(void *ptr)
{
	if(ptr)
		brel(ptr);
}
void fasm_free(void *ptr)
{
	//printf("fasm free ptr=0x%08X\n",(uint32_t)ptr);
	if(ptr)
		brel(ptr);
	//printf("done free\n");
}
void *aligned_malloc(size_t size, int alignment)
{
	void *p1;
	void **p2;
	int offset = alignment - 1 + sizeof(void *);
	// printf("aligned malloc: offset=%u totsize=%u\n",offset,size+offset);
	p1 = malloc(size + offset);
	if (!p1)
		return NULL;
	p2 = (void **)(((uintptr_t)(p1) + offset) & ~(alignment - 1));
	p2[-1] = p1;
	return p2;
}

void aligned_free(void *ptr)
{
	free(((void **)ptr)[-1]);
}

void print_mem_stats()
{
	bufsize curalloc, totfree, maxfree;
	long nget, nrel;
	bstats(&curalloc, &totfree, &maxfree, &nget, &nrel);
	printf("allocated=%u\nfree=%u\nmaxfree=%u\n", curalloc, totfree, maxfree);
}

void print_heap_stats()
{
	bufsize curalloc, totfree, maxfree;
	long nget, nrel;
	bstats(&curalloc, &totfree, &maxfree, &nget, &nrel);
	float ftmp = (float)maxfree;
	ftmp /= (1024.0 * 1024.0);
	printf("Heap Size=%0.0fM\n", ftmp);
}
