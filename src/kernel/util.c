#include "common.h"

size_t write(int fd, const void *buffer, size_t count)
{
	char *buf = (char *)buffer;
	for (int i = 0; i < count; i++)
	{
		if(*buf)
			putchar(*(buf++));
	}
	//printf("write called\n");
	return count;
}

static int exitcalled = 0;
void exit(int r)
{
	//printf("exit called\n");
	exitcalled = 1 + r;
}

void acquire_mutex(atomic_flag *lock)
{
	while (atomic_flag_test_and_set(lock))
	{
		__builtin_ia32_pause();
	}
}
void release_mutex(atomic_flag *lock)
{
	atomic_flag_clear(lock);
}

