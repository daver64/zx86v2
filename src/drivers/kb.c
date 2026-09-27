
#include "common.h"
#include "isr.h"
#include "syscall.h"

int buf[KEYBUF_SIZE];
volatile int index=0;
int keybqueue_size()
{
	return index;
}
int kbhit()
{
	if(index>0)
		return buf[0];
	else
		return 0;
}
int pop_keypress()
{
	if(index>0)
		index--;
	return buf[index];
}

void push_keypress(int k)
{
	if(k==0)
		return;
	buf[index]=k;
	if(index<KEYBUF_SIZE)
		index++;
}

int getchar()
{	
	// Runs in ring3 (called from shell after switch_to_user_mode) - hlt would #GP here,
	// so block via the os_getc syscall, which halts safely at CPL0 until a key arrives.
	return syscall_os_getc(0);
}
int os_getc(int fd)
{
	while(keybqueue_size()==0)
	{
		halt_cpu();
	}
	int c= pop_keypress();
	return c;	
}

void ungetc(int k,FILE* fp)
{
	if(fp==stdout || fp==stderr)
		return;
	if(fp==stdin)
		push_keypress(k);
	else
	{

		int p=(int)ftell(fp);
	//	printf("ungetc %d to %d\n",p,p-1);
		if(p>0)
			fseek(fp,p-1,SEEK_SET);
		fputc(k,fp);
		p=(int)ftell(fp);
		if(p>0)
			fseek(fp,p-1,SEEK_SET);
	}
}

unsigned char kbd_gb_bottom[128] ={
	0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
	'9', '0', '-', '=', '\b',	/* Backspace */
	'\t',			/* Tab */
	'q', 'w', 'e', 'r',	/* 19 */
	't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',		/* Enter key */
	161,			/* 29   - Control */
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
	'\'', 35,   162,		/* Left shift */
	'#', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
	'm', ',', '.', '/',   163,					/* Right shift */
	'*',
	160,	/* Alt */
	' ',	/* Space bar */
	160,	/* Caps lock */
	141,	/* 59 - F1 key ... > */
	142,   143,   144,   145,   146,   147,   148,   149,
	150,	/* < ... F10 */
	157,	/* 69 - Num lock*/
	158,	/* Scroll Lock */
	159,	/* Home key */
	130,	/* Up Arrow */
	153,	/* Page Up */
	'-',
	128,	/* Left Arrow */
	0,
	129,	/* Right Arrow */
	'+',
	160,	/* 79 - End key*/
	140,	/* Down Arrow */
	154,	/* Page Down */
	155,	/* Insert Key */
	156,	/* Delete Key */
	0,   0,   '\\',
	151,	/* F11 Key */
	152,	/* F12 Key */
	0	/* All other keys are undefined */
};

unsigned char kbd_gb_top[128] ={
	/* shifted from here */
	0,  0, '!', '\"', 156, '$', '%', '^', '&', '*',	/* 9 */
	'(', ')', '_', '+', '\b',	/* Backspace */
	'\t',			/* Tab */
	'Q', 'W', 'E', 'R',	/* 19 */
	'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',		/* Enter key */
	161,			/* 29   - Control */
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',	/* 39 */
	'@', '~',   162,		/* Left shift */
	'~', 'Z', 'X', 'C', 'V', 'B', 'N',			/* 49 */
	'M', '<', '>', '?',   163,					/* Right shift */
	'*',
	160,	/* Alt */
	' ',	/* Space bar */
	0,	/* Caps lock */
	141,	/* 59 - F1 key ... > */
	142,   143,   144,   145,   146,   147,   148,   149,
	150,	/* < ... F10 */
	0,	/* 69 - Num lock*/
	0,	/* Scroll Lock */
	0,	/* Home key */
	130,	/* Up Arrow */
	0,	/* Page Up */
	'-',
	128,	/* Left Arrow */
	0,
	129,	/* Right Arrow */
	'+',
	0,	/* 79 - End key*/
	0,	/* Down Arrow */
	0,	/* Page Down */
	0,	/* Insert Key */
	0,	/* Delete Key */
	0,   0,   '|',
	151,	/* F11 Key */
	152,	/* F12 Key */
	0	/* All other keys are undefined */  
};

/* Handles the keyboard interrupt */
void keyboard_handler(registers_t *r)
{
	unsigned char scancode;
	static int lshiftpress = 0;
	static int rshiftpress = 0;
	/* Read from the keyboard's data buffer */
	scancode = inb(0x60);
	//syscall_os_puts("got a key\n");
	//monitor_write_dec (scancode);
	switch(scancode)
	{
		case 0x2A : lshiftpress=1; break;
		case 0xAA : lshiftpress=0; break;
		case 0x36 : rshiftpress=1; break;
		case 0xB6 : rshiftpress=0; break;
	}

	if(!(scancode & 0x80))
	{
		if(lshiftpress || rshiftpress)
			push_keypress(kbd_gb_top[scancode]);
		else
			push_keypress(kbd_gb_bottom[scancode]);
	}
	
	// Process any deferred executions (safe context after handling key)
	extern void process_deferred_executions(void);
	process_deferred_executions();
}

/* Installs the keyboard handler into IRQ1 */
void initialise_keyboard()
{
	//os_puts("install keyboard\n");
	register_interrupt_handler(IRQ1, &keyboard_handler);
}
