// common.c -- Defines some global functions.
//             From JamesM's kernel development tutorials.

#include "common.h"
#include <limits.h>

uint32_t next_pow2(uint32_t x)
{
	return x == 1 ? 1 : 1 << (32 - __builtin_clz(x - 1));
}
// Write a byte out to the specified port.
void outb(uint16_t port, uint8_t value)
{
    asm volatile ("outb %1, %0" : : "dN" (port), "a" (value));
}
void outw(uint16_t port, uint16_t value)
{
    asm volatile ("outw %1, %0" : : "dN" (port), "a" (value));
}

void outl(uint16_t port, uint32_t value)
{
    asm volatile ("outl %1, %0" : : "dN" (port), "a" (value));
}
uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}

uint16_t inw(uint16_t port)
{
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}

uint32_t inl(uint16_t port)
{
    uint32_t ret;
    asm volatile ("inl %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}

void enable_interrupts()
{
	asm volatile("sti");
}
void disable_interrupts()
{
	asm volatile("cli");
}
void halt_cpu()
{
	asm volatile("hlt");
}
void pause_cpu()
{
	asm volatile("pause");
}
void invlpg(uint32_t addr)
{
	 asm volatile("invlpg (%0)" ::"r" (addr) : "memory");
}
void io_wait()
{
    outb(0x80,0);
}

bool are_interrupts_enabled()
{
    uint32_t flags;
    asm volatile ("pushf\n\t"
                "pop %0"
                : "=g"(flags));
    return flags & (1<<9);
}

uint32_t save_irqdisable()
{
    uint32_t flags;
    asm volatile ("pushf\n\tcli\n\tpop %0" : "=r"(flags) : : "memory");
    return flags;
}
void restore_irqs(uint32_t flags)
{
    asm volatile ("push %0\n\tpopf" :: "rm"(flags) : "memory","cc");
}
/*
void intended usage()
{
    unsigned long f = save_irqdisable();
    do_whatever_without_irqs();
    restore_irqs(f);
}
*/
uint64_t rdmsr(uint32_t msr_id)
{
    uint64_t msr_value;
    asm volatile ( "rdmsr" : "=A"(msr_value) : "c" (msr_id) );
    return msr_value;
}
void wrmsr(uint32_t msr_id, uint64_t msr_value)
{
    asm volatile ("wrmsr" : : "c"(msr_id), "A"(msr_value));
}


extern void panic(const char *message, const char *file, uint32_t line)
{
    // We encountered a massive problem and have to stop.
    asm volatile("cli"); // Disable interrupts.
    printf("PANIC (%s)\n",message);

    for(;;) asm volatile("hlt");
}

extern void panic_assert(const char *file, uint32_t line, const char *desc)
{
    // An assertion failed, and we have to panic.
    asm volatile("cli"); // Disable interrupts.
    printf("PANIC ASSERT (%s)\n",desc);

    // Halt by going into an infinite loop (interrupts disabled so NMI-only wake, but avoids spin).
    for(;;) asm volatile("hlt");
}


// optimize("no-omit-frame-pointer") doesn't seem to work
// we still don't get a frame-point unless we force -O0 for the function with optimize(0)
__attribute__((noinline, noclone, returns_twice, optimize(0)))
int setjmp(jmp_buf var){
    // relies on the compiler to make a stack-frame
    // because we're using inline asm inside a function instead of at global scope
     __asm__(
             "    mov 8(%ebp), %eax     # get pointer to jmp_buf, passed as argument on stack\n"
             "    mov    %ebx, (%eax)   # jmp_buf[0] = ebx\n"
             "    mov    %esi, 4(%eax)  # jmp_buf[1] = esi\n"
             "    mov    %edi, 8(%eax)  # jmp_buf[2] = edi\n"
             "    mov    (%ebp), %ecx\n"
             "    mov    %ecx, 12(%eax) # jmp_buf[3] = ebp\n"
             "    lea    8(%ebp), %ecx  # get previous value of esp, before call\n"
             "    mov    %ecx, 16(%eax) # jmp_buf[4] = esp before call\n"
             "    mov    4(%ebp), %ecx  # get saved caller eip from top of stack\n"
             "    mov    %ecx, 20(%eax) #jmp_buf[5] = saved eip\n"
             "    xor    %eax, %eax     #eax = 0\n"
     );

    return 0;
}

__attribute__((noinline, noclone, optimize(0)))
void longjmp(jmp_buf var,int m){
    __asm__("    mov  8(%ebp),%edx # get pointer to jmp_buf, passed as argument 1 on stack\n"
            "    mov  12(%ebp),%eax #get int val in eax, passed as argument 2 on stack\n"
            "    test    %eax,%eax # is int val == 0?\n"
            "    jnz 1f\n"
            "    inc     %eax      # if so, eax++\n"
            "1:\n"
            "    mov   (%edx),%ebx # ebx = jmp_buf[0]\n"
            "    mov  4(%edx),%esi # esi = jmp_buf[1]\n"
            "    mov  8(%edx),%edi #edi = jmp_buf[2]\n"
            "    mov 12(%edx),%ebp # ebp = jmp_buf[3]\n"
            "    mov 16(%edx),%ecx # ecx = jmp_buf[4]\n"
            "    mov     %ecx,%esp # esp = ecx\n"
            "    mov 20(%edx),%ecx # ecx = jmp_buf[5]\n"
            "    jmp *%ecx         # eip = ecx");
}

typedef struct xorstate
{
	uint32_t x[5];
	uint32_t counter;
} xorstate;

static xorstate rng_xorstate = {0, 0, 0, 0, 0, 0};
static int xorstate_seeded = false;

uint32_t xorshift(xorstate *state)
{
	uint32_t t = state->x[4];
	uint32_t s = state->x[0];
	state->x[4] = state->x[3];
	state->x[3] = state->x[2];
	state->x[2] = state->x[1];
	state->x[1] = s;
	t ^= t >> 2;
	t ^= t << 1;
	t ^= s ^ (s << 4);
	state->x[0] = t;
	state->counter += 362437;
	return t + state->counter;
}


void srand(unsigned int seed)
{
	unsigned int aseed = seed + 2947835;
	rng_xorstate.counter = aseed + 87656434;
	rng_xorstate.x[0] = aseed << 8;
	rng_xorstate.x[1] = aseed ^ 23846272;
	rng_xorstate.x[2] = rng_xorstate.x[1] + aseed;
	rng_xorstate.x[3] = aseed;
	rng_xorstate.x[4] = aseed * 16;
	xorstate_seeded = true;
	for (int i = 0; i < 5; i++)
	{
		xorshift(&rng_xorstate);
	}
}

int rand()
{
	if (!xorstate_seeded)
	{
		srand(17);
		xorstate_seeded = true;
	}
	return (int)xorshift(&rng_xorstate);
}

float randf()
{
	int ri=rand();
	float rf = (float)ri/(float)INT_MAX;
	return rf;
}
