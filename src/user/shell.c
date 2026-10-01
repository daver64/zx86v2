#include "common.h"
#include "graphics.h"
#include "syscall.h"
#include "amp.h"
#include <ff.h>
#include "diskio.h"
#include "elf-32.h"	  // For ELF header definitions
#include "graphics.h" // For graphics_enabled() and terminal_flush()
#include "sound.h"	  // For sound card support
#include "network.h"  // For network card support
#include "vfs.h"      // For VFS operations
#include "blockdev.h" // For block device management

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h> 
#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include <ff.h>
#include <basic.h>

typedef int32_t (*user_f_t)();
int sh_test(int argc, char **args);
int32_t sh_getline(char *buffer, int32_t buflen);
int elf_process_quickload(const char *filename);
int elf_load_internal(const char *filename, int print_pid);
void cleanup_command_history(void);
char prompt[32];
uint32_t wantquit = false;
int ctok = 0;
int shell_num_base = 10;
int print_exit_code = true;
int allowpromptoverwrite = false;
int plen_active = true;
int plen = 0;
static int32_t sh_getline_count = -1;

int os_ls(int argc, char **argv);
int sh_ls(int argc, char **argv);
int sh_cd(int argc, char **argv);
int sh_chdrive(int argc, char **argv);
int sh_mkdir(int argc, char **argv);
int sh_rmdir(int argc, char **argv);
int os_rm(int argc, char **argv);
int os_mv(int argc, char **argv);
int sh_cls(int argc, char **argv);
int sh_kmesg(int argc, char **argv);
int sc_main(int argc, char **argv);
int sh_cat(int argc, char **argv);
int sh_bcat(int argc, char **argv);
int sh_help(int argc, char **argv);
int sh_fasm(int argc, char **argv);
int sh_vi(int argc, char **argv);
int sh_elfsup(int argc, char **argv);
int sh_disasm(int argc, char **argv);
int sh_peekmem(int argc, char **argv);
int sh_pokemem(int argc, char **argv);
int sh_dumpmem(int argc, char **argv);
int sh_binsave(int argc, char **argv);
int sh_binload(int argc, char **argv);
int sh_getenv(int argc, char **argv);
int sh_setenv(int argc, char **argv);
int sh_time(int argc, char **argv);
int sh_date(int argc, char **argv);
int sh_set_mode(int argc, char **argv);
int sh_basic(int argc, char **argv);
int sh_exec(int argc, char **argv);
int db_main(int argc, char **argv);
int sh_shutdown(int argc, char **args);
int sh_sound(int argc, char **argv);
int sh_amp(int argc, char **argv);
int sh_amp_exec(int argc, char **argv);
int sh_net(int argc, char **argv);
int sh_mount(int argc, char **argv);
int sh_unmount(int argc, char **argv);
int sh_lsdev(int argc, char **argv);
int sh_df(int argc, char **argv);
int rgl_main(int argc, char **argv);
int sh_tasklist(int argc, char **argv);
int sh_test(int argc, char **argv);
typedef struct shellcommand
{
	const char *cmd;
	const char *description;
	int (*function_t)(int, char **);
} shellcommand_t;

shellcommand_t cmds[] =
	{
		{"test", "current WIP testing. perilous (:", &sh_test},
		{"ls", "list files", &sh_ls},
		{"rm", "remove file", &os_rm},
		{"mkdir", "make directory", &sh_mkdir},
		{"rmdir", "remove directory", &sh_rmdir},
		{"mv", "move file", &os_mv},
		{"cls", "clear the screen", &sh_cls},
		{"kmesg", "kernel boot message", &sh_kmesg},
		{"scheme", "scheme interpreter", &sc_main},
		{"basic", "basic interpreter", &sh_basic},
		{"db", "database", &db_main},
		{"cd", "change directory", &sh_cd},
		{"chdrive", "change drive", &sh_chdrive},
		{"cat", "dump file contents", &sh_cat},
		{"bcat", "dump binary file contents", &sh_bcat},
		{"help", "display list of commands", &sh_help},
		{"fasm", "run fasm assembler", &sh_fasm},
		{"vi", "vi [filename] - Simple vi text editor", &sh_vi},
		{"elfsup", "load and run ELF executable", &sh_elfsup},
		{"disasm", "disassemble a file", &sh_disasm},
		{"peek", "peek at memory", &sh_peekmem},
		{"poke", "poke values into memory", &sh_pokemem},
		{"dump", "dump memory values", &sh_dumpmem},
		{"save", "save memory to file", &sh_binsave},
		{"load", "load file into memory", &sh_binload},
		{"getenv", "get environment vars", &sh_getenv},
		{"setenv", "set environment vars", &sh_setenv},
		{"time", "display current time", &sh_time},
		{"shutdown", "qemu only : power down computer", &sh_shutdown},
		{"sound", "sound card commands", &sh_sound},
		{"amp", "asymmetric multiprocessing commands", &sh_amp},
		{"amp_exec", "execute ELF process on secondary CPU", &sh_amp_exec},
		{"net", "network card commands", &sh_net},
		{"exec", "execute process by PID", &sh_exec},
		{"tasks", "list running tasks", &sh_tasklist},
		{"date", "display current date", &sh_date},
		{"set-mode", "set graphics video mode", &sh_set_mode},
		{"mount", "mount filesystem", &sh_mount},
		{"unmount", "unmount filesystem", &sh_unmount},
		{"lsdev", "list block devices", &sh_lsdev},
		{"rgl", "roguelike game", &rgl_main},
		{"df", "display mounted filesystems", &sh_df}};

int sh_num_cmds()
{
	return sizeof(cmds) / sizeof(shellcommand_t);
}

/*
 *

 ____  _          _ _
 / ___|| |__   ___| | |
 \___ \| '_ \ / _ \ | |
 ___) | | | |  __/ | |
 |____/|_| |_|\___|_|_|


 *
 */
#include "task.h"
void lzw_test();

// Input validation functions
bool is_safe_filename(const char *filename)
{
	if (!filename || strlen(filename) == 0)
		return false;
	if (strlen(filename) > 255)
		return false; // MAX_PATH

	// Check for dangerous patterns
	if (strstr(filename, ".."))
		return false;
	if (filename[0] == '/')
		return false; // No absolute paths

	// Check for valid characters (basic check)
	for (const char *p = filename; *p; p++)
	{
		if (*p < 32 || *p > 126)
			return false; // Only printable ASCII
		if (*p == '<' || *p == '>' || *p == '|' || *p == '*' || *p == '?')
			return false;
	}

	return true;
}

int sh_test(int argc, char **args)
{
	printf("Testing direct graphics functionality (main CPU only)...\n");

	if (argc < 2)
	{
		printf("Usage: test <command>\n");
		printf("Commands:\n");
		printf("  create - Create test pattern using direct framebuffer\n");
		printf("  fill - Test framebuffer filling with different colors\n");
		printf("  pattern - Draw test patterns directly to screen\n");
		return 0;
	}

	if (!graphics_enabled()) {
		printf("Graphics not enabled - cannot run graphics tests\n");
		return 1;
	}

	extern void fb_put_pixel(int x, int y, uint32_t colour);
	
	if (strcmp(args[1], "create") == 0)
	{
		printf("Drawing test pattern directly to framebuffer...\n");
		
		int base_x = 50;
		int base_y = 50;
		int size = 64;
		
		// Draw a blue square
		for (int y = 0; y < size; y++) {
			for (int x = 0; x < size; x++) {
				fb_put_pixel(base_x + x, base_y + y, 0xFF0000FF); // Blue
			}
		}
		
		printf("Blue square drawn at (%d, %d)\n", base_x, base_y);
		return 0;
	}

	if (strcmp(args[1], "fill") == 0)
	{
		printf("Drawing color test rectangles...\n");
		
		int base_x = 100;
		int base_y = 100;
		int rect_size = 32;
		int spacing = 40;
		
		// Red rectangle
		for (int y = 0; y < rect_size; y++) {
			for (int x = 0; x < rect_size; x++) {
				fb_put_pixel(base_x + x, base_y + y, 0xFFFF0000); // Red
			}
		}
		
		// Green rectangle
		for (int y = 0; y < rect_size; y++) {
			for (int x = 0; x < rect_size; x++) {
				fb_put_pixel(base_x + spacing + x, base_y + y, 0xFF00FF00); // Green
			}
		}
		
		// Blue rectangle
		for (int y = 0; y < rect_size; y++) {
			for (int x = 0; x < rect_size; x++) {
				fb_put_pixel(base_x + (spacing * 2) + x, base_y + y, 0xFF0000FF); // Blue
			}
		}
		
		printf("Color test rectangles drawn\n");
		return 0;
	}

	if (strcmp(args[1], "pattern") == 0)
	{
		printf("Drawing test patterns...\n");
		
		int base_x = 200;
		int base_y = 200;
		
		// Draw a checkerboard pattern
		for (int y = 0; y < 64; y++) {
			for (int x = 0; x < 64; x++) {
				uint32_t color = ((x / 8) + (y / 8)) % 2 ? 0xFFFFFFFF : 0xFF000000;
				fb_put_pixel(base_x + x, base_y + y, color);
			}
		}
		
		// Draw some diagonal lines
		for (int i = 0; i < 100; i++) {
			fb_put_pixel(base_x + 100 + i, base_y + i, 0xFFFF00FF); // Magenta line
			fb_put_pixel(base_x + 100 + i, base_y + 99 - i, 0xFF00FFFF); // Cyan line
		}
		
		printf("Test patterns drawn\n");
		return 0;
	}

	printf("Unknown test command: %s\n", args[1]);
	printf("Available commands: create, fill, pattern\n");
	return 1;
}
	
#include "task.h"
extern volatile task_t *ready_queue;
extern volatile task_t *current_task;

int sh_tasklist(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	volatile task_t *head = ready_queue;
	volatile task_t *task = head;
	int task_count = 0;
	const int max_tasks = 128;

	if (!task)
	{
		printf("No tasks found\n");
		return 0;
	}

	printf("PID  * EIP      ESP      EBP      DIR      KSTACK\n");
	while (task && task_count < max_tasks)
	{
		printf("%4d %c %08X %08X %08X %08X %08X\n",
			task->id,
			task == current_task ? '*' : ' ',
			task->eip,
			task->esp,
			task->ebp,
			(uint32_t)task->page_directory,
			task->kernel_stack);

		if (task->memory_layout)
		{
			process_memory_layout_t *layout = task->memory_layout;
			printf("     code %08X-%08X heap %08X-%08X stack %08X-%08X\n",
				layout->code_start,
				layout->code_end,
				layout->heap_start,
				layout->heap_end,
				layout->stack_bottom,
				layout->stack_top);
		}

		task_count++;
		volatile task_t *next = task->next;
		if (next == head)
			break;
		task = next;
	}

	if (task && task_count == max_tasks)
		printf("Task list truncated after %d entries\n", max_tasks);

	printf("%d task(s)\n", task_count);
	return 0;
}
int sh_basic(int argc, char **args)
{
	int result = basic_main(argc, args);
	return result;
}
int sh_time(int argc, char **args)
{
	printf("%s\n", os_asctime());
	return 0;
}
int sh_date(int argc, char **args)
{
	printf("%s\n", asciidate());
	return 0;
}
int sh_exec(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("Usage: exec <PID>\n");
		printf("Execute a process by its Process ID\n");
		return 1;
	}

	int pid = atoi(argv[1]);
	if (pid <= 0)
	{
		printf("Error: Invalid PID '%s'\n", argv[1]);
		return 1;
	}

	// Call syscall to execute the process
	int result = syscall_os_exec_process(pid);

	if (result != 0)
	{
		printf("Failed to execute process %d (error %d)\n", pid, result);
	}

	return (result == 0) ? 0 : 1;
}
int sh_cls(int argc, char **args)
{
	cls();
	return 0;
}
void boot_kmesg();

int sh_kmesg(int argc, char **args)
{
	cls();
	boot_kmesg();
	return 0;
}
int sh_ls(int argc, char **args)
{
	os_ls(argc, args);
	return 0;
}
int sh_cd(int argc, char **argv)
{
	os_cd(argc, argv);
	return 0;
}
int sh_chdrive(int argc, char **argv)
{
	os_chdrive(argc, argv);
	return 0;
}
int sh_getenv(int argc, char **argv)
{
	if (argc < 2)
	{
		printenvvars();
	}
	else
	{
		const char *value = getenv(argv[1]);
		printf("%s\n", value);
	}
	return 0;
}
int sh_setenv(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("usage:\nsetenv KEY=VALUE\n");
		return 1;
	}
	else
	{
		putenv(argv[1]);
	}
	return 0;
}
int sh_mkdir(int argc, char **argv)
{
	os_mkdir(argc, argv);
	return 0;
}
int sh_rmdir(int argc, char **argv)
{
	os_rmdir(argc, argv);
	return 0;
}

int sh_kb(int argc, char **args)
{
	// printf("keyboard probe, press ESC to quit\n");
	int k = 0;
	while (k != 27)
	{
		k = getchar();
		printf("%03d 0x%02x '%c'\n", k, k, k);
	}
	return 0;
}

int sh_shutdown(int argc, char **args)
{
	// beep(440,250);
	outw(0x604, 0x2000);
	//	acpi_shutdown();
	return 0;
}
int sh_cat(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("usage: cat <filename>\n");
		return 1;
	}

	// Use VFS to open and read the file (let VFS handle path resolution)
	int fd = vfs_open(argv[1], FREAD, 0);
	if (fd < 0)
	{
		printf("Error: Cannot open file '%s'\n", argv[1]);
		return 1;
	}

	// Read file in chunks to avoid large memory allocation
	char buffer[1024];
	long bytes_read;
	long total_bytes = 0;
	
	while ((bytes_read = vfs_read(fd, buffer, sizeof(buffer) - 1)) > 0)
	{
		total_bytes += bytes_read;
		
		
		// Print each character normally
		for (long i = 0; i < bytes_read; i++) {
			unsigned char c = buffer[i];
			
			if (c == '\0') {
				// Stop reading at first null byte - this is normal behavior
				goto cat_done;
			} else {
				putchar(c);
			}
		}
		terminal_flush();
	}

cat_done:
	vfs_close(fd);
	return 0;
}
int sh_bcat(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("usage: bcat <filename>\n");
		return 1;
	}

	// Use VFS to open the file (let VFS handle path resolution)
	int fd = vfs_open(argv[1], FREAD, 0);
	if (fd < 0)
	{
		printf("Error: Cannot open file '%s'\n", argv[1]);
		return 1;
	}

	// Read file in chunks since we don't have stat
	char buffer[512];
	long total_read = 0;
	long bytes_read;
	
	while ((bytes_read = vfs_read(fd, buffer, sizeof(buffer))) > 0)
	{
		// Output the binary data as hexadecimal
		for (long i = 0; i < bytes_read; i++)
		{
			printf("%02X ", (unsigned char)buffer[i]);
			if ((total_read + i + 1) % 16 == 0)
				printf("\n");
		}
		total_read += bytes_read;
	}
	
	if (total_read % 16 != 0) {
		printf("\n");
	}
	
	vfs_close(fd);
	
	if (bytes_read < 0)
	{
		printf("Error: Failed to read file '%s'\n", argv[1]);
		return 1;
	}

	return 0;
}
void screen_pause(const char *msg)
{
	uint8_t ofgc = get_foreground_colour();
	set_foreground_colour(VGA_YELLOW);
	printf("%s", msg);
	terminal_flush(); // Ensure the pause message is displayed before waiting
	set_foreground_colour(ofgc);
	
	// Simple approach - just wait for any character input
	getchar();
	printf("\n");
	terminal_flush(); // Ensure newline is displayed
}
int sh_help(int argc, char **args)
{
	cls();
	terminal_flush(); // Ensure screen is cleared and displayed
	
	int i;
	int count = 1; // Start at 1 to account for the "Available commands:" header
	int rows = get_num_rows();
	
	// Safety check - if get_num_rows() returns invalid value, use default
	if (rows <= 5 || rows > 100) {
		rows = 25;  // Default terminal height
	}
	
	printf("Available commands:\n");
	terminal_flush(); // Ensure header is displayed
	
	for (i = 0; i < sh_num_cmds(); i++)
	{
		printf("%-20s: %s\n", cmds[i].cmd, cmds[i].description);
		count++;
		
		// Page every (rows - 3) lines and only if there are more commands to show
		if (count >= (rows - 3) && i < sh_num_cmds() - 1)
		{
			terminal_flush(); // Ensure all output is flushed before pause
			screen_pause("-- Press any key to continue --");
			count = 0; // Reset counter after pause
		}
	}
	printf("\n");
	terminal_flush(); // Ensure final newline is displayed
	return 0;
}
int sh_fasm(int argc, char **argv)
{
	return syscall_os_fasm(argc, argv);
}
#define LINE_BUFFER_LEN (4096)

int exec_at_buffer(unsigned char *buffer)
{
	user_f_t f = (user_f_t)buffer;
	uint32_t result = f();
	if (print_exit_code)
	{
		printf("%d\n", result);
	}
	return result;
}
int shell_exec(const char *filename)
{
	int recognised_command = false;
	char *exe_filename = malloc(128);
	char *com_filename = malloc(128);
	snprintf(exe_filename, 128, "%s.elf", filename);
	snprintf(com_filename, 128, "%s.com", filename);
	
	// Try ELF file first (let VFS handle path resolution)
	int fd = vfs_open(exe_filename, FREAD, 0);
	if (fd >= 0)
	{
		vfs_close(fd);  // Just checking if file exists
		int result = elf_process_quickload(exe_filename);
		if (result == 0)
			recognised_command = true;
	}
	else
	{
		// Try COM file (let VFS handle path resolution)
		fd = vfs_open(com_filename, FREAD, 0);
		if (fd >= 0)
		{
			// Read COM file in chunks and build buffer dynamically
			size_t buffer_size = 1024;
			size_t total_size = 0;
			unsigned char *buffer = malloc(buffer_size);
			
			if (buffer)
			{
				long bytes_read;
				while ((bytes_read = vfs_read(fd, buffer + total_size, buffer_size - total_size)) > 0)
				{
					total_size += bytes_read;
					if (total_size + 512 > buffer_size)
					{
						buffer_size *= 2;
						unsigned char *new_buffer = realloc(buffer, buffer_size);
						if (!new_buffer)
						{
							free(buffer);
							buffer = NULL;
							break;
						}
						buffer = new_buffer;
					}
				}
				
				vfs_close(fd);
				
				if (buffer && bytes_read >= 0 && total_size > 0)
				{
					exec_at_buffer(buffer);
					recognised_command = true;
				}
				
				if (buffer) free(buffer);
			}
			else
			{
				vfs_close(fd);
			}
		}
	}
	free(exe_filename);
	free(com_filename);
	return recognised_command;
}

int sh_binload(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("usage: binload filename address\n");
		return 1;
	}

	// Validate filename
	if (!is_safe_filename(argv[1]))
	{
		printf("Error: Invalid filename '%s'\n", argv[1]);
		return 1;
	}

	char *fbuffer = NULL;
	size_t fbuffer_len = 0;
	uint32_t load_address = 0;
	
	// Parse load address
	load_address = strtoul(argv[2], 0, 16);

	// Use VFS to open the file (let VFS handle path resolution)
	int fd = vfs_open(argv[1], FREAD, 0);
	if (fd < 0)
	{
		printf("unable to open file %s\n", argv[1]);
		return 1;
	}

	// Read file in chunks and build buffer dynamically
	size_t buffer_size = 1024;
	size_t total_size = 0;
	fbuffer = malloc(buffer_size);
	
	if (!fbuffer)
	{
		printf("Error: Memory allocation failed\n");
		vfs_close(fd);
		return 1;
	}
	
	long bytes_read;
	while ((bytes_read = vfs_read(fd, (char*)fbuffer + total_size, buffer_size - total_size)) > 0)
	{
		total_size += bytes_read;
		if (total_size + 512 > buffer_size)
		{
			buffer_size *= 2;
			unsigned char *new_buffer = realloc(fbuffer, buffer_size);
			if (!new_buffer)
			{
				free(fbuffer);
				fbuffer = NULL;
				vfs_close(fd);
				printf("Error: Memory reallocation failed\n");
				return 1;
			}
			fbuffer = new_buffer;
		}
	}
	
	fbuffer_len = total_size;
	vfs_close(fd);
	
	if (bytes_read < 0 || total_size == 0)
	{
		printf("Error: Failed to read file '%s'\n", argv[1]);
		free(fbuffer);
		return 1;
	}
	
	fbuffer_len = bytes_read;  // Update with actual bytes read

	if (load_address > 0 && fbuffer_len > 0)
	{
		printf("load address 0x%08x , buffer length %u\n", load_address, fbuffer_len);
		memcpy((void *)load_address, (void *)fbuffer, fbuffer_len);
		free(fbuffer);
	}
	else
	{
		printf("incorrect load address 0x%08x or buffer length %u\n", load_address, fbuffer_len);
		free(fbuffer);
		return 1;
	}
	return 0;
}

int sh_binsave(int argc, char **argv)
{
	if (argc < 4)
	{
		printf("usage: binsave filename address length\n");
		return 1;
	}

	void *address = (void *)(strtoul(argv[2], 0, 16));
	size_t length = strtoul(argv[3], 0, 10);

	if (address == NULL || length == 0)
	{
		printf("parameter error\n");
		return 1;
	}

	// Use VFS to create/open the file for writing (let VFS handle path resolution)
	int fd = vfs_open(argv[1], FWRITE | FCREAT | FTRUNC, 0644);
	if (fd < 0)
	{
		printf("unable to open file %s\n", argv[1]);
		return 1;
	}

	long bytes_written = vfs_write(fd, address, length);
	vfs_close(fd);

	if (bytes_written < 0 || (size_t)bytes_written != length)
	{
		printf("Error: Failed to write %zu bytes to file '%s'\n", length, argv[1]);
		return 1;
	}

	return 0;
}

int sh_dumpmem(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("usage:\ndumpmem address num-16-byte-paragraphs\n");
		return 1;
	}
	uint8_t *address = NULL;
	int numparas = 0;
	for (int arg = 0; arg < argc; arg++)
	{
		if (arg == 1)
		{
			address = (uint8_t *)(strtoul(argv[arg], 0, 16));
		}
		if (arg == 2)
		{
			numparas = strtoul(argv[arg], 0, 10);
		}
	}
	if (address != NULL && numparas > 0)
	{
		int numbytes = numparas * 16;
		char *asciibuffer = malloc(17);
		memset(asciibuffer, 0, 17);
		for (int para = 0; para < numparas; para++)
		{
			char buf[2] = {0, 0};

			for (int i = 0; i < 16; i++)
			{
				uint8_t c = *(uint8_t *)(address + ((para * 16) + i));
				printf("%02X ", c);
			}
			printf("\t|");
			for (int i = 0; i < 16; i++)
			{
				uint8_t c = *(uint8_t *)(address + ((para * 16) + i));
				if (isprint(c) && c != '\n' && c != '\r' && c != '\t')
				{
					printf("%c", c);
				}
				else
				{
					printf(".");
				}
			}
			printf("|\n");
		}
		free(asciibuffer);
		printf("\n");
	}
	else
	{
		printf("parameter error\n");
		return 1;
	}
	return 0;
}

int sh_pokemem(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("usage:\npokemem address byte\n");
		return 1;
	}
	uint8_t *address = NULL;
	uint8_t byte = 0;
	for (int arg = 0; arg < argc; arg++)
	{
		if (arg == 1)
		{
			address = (uint8_t *)(strtoul(argv[arg], 0, 16));
		}
		if (arg == 2)
		{
			byte = strtoul(argv[arg], 0, 16);
		}
	}
	if (address != NULL)
	{
		// Add safety checks
		uint32_t addr_val = (uint32_t)address;
		if (addr_val < 0x1000 || addr_val > 0xFFFF0000)
		{
			printf("Error: Invalid address range 0x%08X\n", addr_val);
			return 1;
		}

		*address = byte;
		printf("Wrote 0x%02X to 0x%08X\n", byte, addr_val);
	}
	else
	{
		printf("parameter error\n");
	}
	return 0;
}
int sh_peekmem(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("usage:\npeekmem address\n");
		return 1;
	}
	uint32_t address = 0;
	for (int arg = 0; arg < argc; arg++)
	{
		if (arg == 1)
		{
			address = (strtoul(argv[arg], 0, 16));
			// printf("look up address %08X %08X\n",address,0xC0000000);
		}
	}
	if (address != 0)
	{
		// Add safety checks
		if (address < 0x1000 || address > 0xFFFF0000)
		{
			printf("Error: Invalid address range 0x%08X\n", address);
			return 1;
		}

		printf("0x%08X: %02X\n", address, *((uint8_t *)address));
	}
	else
	{
		printf("parameter error\n");
	}
	return 0;
}
int sh_disasm(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("usage:\ndisasm <filename> [max_bytes]\n");
		printf("  filename   - file to disassemble\n");
		printf("  max_bytes  - maximum bytes to disassemble (default: entire file, max: 4096)\n");
		return 1;
	}

	const char *filename = argv[1];
	uint32_t max_bytes = 4096; // Default maximum
	
	// Parse optional max_bytes parameter
	if (argc >= 3)
	{
		max_bytes = strtoul(argv[2], NULL, 10);
		if (max_bytes == 0 || max_bytes > 4096)
		{
			printf("max_bytes must be between 1 and 4096\n");
			return 1;
		}
	}

	// Open the file using standard libc (now with binary mode support!)
	FILE *fp = fopen(filename, "rb");
	if (!fp)
	{
		printf("Error: Could not open file '%s'\n", filename);
		return 1;
	}

	// Get file size
	fseek(fp, 0, SEEK_END);
	uint32_t file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (file_size == 0)
	{
		printf("Error: File '%s' is empty\n", filename);
		fclose(fp);
		return 1;
	}

	// Determine how many bytes to read
	uint32_t bytes_to_read = (file_size < max_bytes) ? file_size : max_bytes;

	// Allocate buffer
	unsigned char *buffer = (unsigned char *)malloc(bytes_to_read);
	if (!buffer)
	{
		printf("Error: Could not allocate memory for file\n");
		fclose(fp);
		return 1;
	}

	// Read file data
	uint32_t bytes_read = fread(buffer, 1, bytes_to_read, fp);
	fclose(fp);

	if (bytes_read == 0)
	{
		printf("Error: Could not read from file '%s'\n", filename);
		free(buffer);
		return 1;
	}

	// Print file information
	printf("Disassembling file: %s\n", filename);
	printf("File size: %u bytes\n", file_size);
	printf("Disassembling: %u bytes\n", bytes_read);
	if (bytes_read < file_size)
	{
		printf("(truncated to %u bytes)\n", max_bytes);
	}
	printf("\n");

	// Disassemble the code
	disasm(buffer, bytes_read);

	// Clean up
	free(buffer);
	return 0;
}
void shell_main()
{
	// Display boot banner
	extern void boot_kmesg();
	boot_kmesg();
	// Start in root directory where files exist - use direct call for user space consistency
	os_chdir("/");
	char *linebuffer = malloc(LINE_BUFFER_LEN);
	
	if (!linebuffer) {
		printf("Error: Failed to allocate line buffer\n");
		return;
	}
	while (!wantquit)
	{
		// Get current directory for prompt
		char current_dir[VFS_MAXPATHLEN];
		memset(current_dir, 0, sizeof(current_dir));  // Clear buffer
		os_getcwd(current_dir, sizeof(current_dir));
		
		// Convert to lowercase for display
		for (int i = 0; i < VFS_MAXPATHLEN && current_dir[i] != '\0'; i++)
		{
			current_dir[i] = tolower(current_dir[i]);
		}
		
		// Display current directory in blue
		int ofgc = get_foreground_colour();
		
		set_foreground_colour(VGA_LIGHTBLUE);
		
		printf("%s", current_dir);
		
		set_foreground_colour(ofgc);
		
		// Display prompt character only (don't repeat directory)
		memset(prompt, 0, sizeof(prompt));  // Clear prompt buffer
		
		sprintf(prompt, ">");  // Just the prompt character
		
		printf("%s", prompt);  // Use safe printf format

		// Force display of prompt in graphics mode
		if (graphics_enabled())
		{
			terminal_flush();
		}

		// plen=strlen(prompt);

		memset(linebuffer, 0, LINE_BUFFER_LEN);
		
		int len = sh_getline(linebuffer, LINE_BUFFER_LEN);

		char *cmdstart_ptr = linebuffer;
		
		while (*cmdstart_ptr != 0)
		{
			if (*cmdstart_ptr == '\t')
				*cmdstart_ptr = ' ';
			cmdstart_ptr++;
		}
		cmdstart_ptr = linebuffer;
		while (isspace(*cmdstart_ptr))
		{
			cmdstart_ptr++;
		}

		char **args;
		int argc;
		argc = string_split(cmdstart_ptr, ' ', &args);
		unsigned int i = 0;
		int recognised_command = false;
		for (i = 0; i < sh_num_cmds(); i++)
		{
			if (!strcmp(args[0], cmds[i].cmd))
			{
				recognised_command = true;
				int result = (*cmds[i].function_t)(argc, args);
			}
		}

		if (!recognised_command && strlen(args[0]) > 0)
		{
			// printf("\nshell_exec %s\n",args[0]);
			recognised_command = shell_exec(args[0]);
		}
		if (!recognised_command && strlen(args[0]) > 0)
		{
			printf("unrecognised command [");
			char *cptr = args[0];
			while (*cptr != '\0')
			{
				if (*cptr == 27)
					printf("ESC");
				else
					putchar(*cptr);
				cptr++;
			}
			printf("]\n");
		}

		for (i = 0; i < argc; i++)
		{
			free(*(args + i));
		}
		free(args);
	}

	// Cleanup before exit
	cleanup_command_history();
	free(linebuffer);
}

/*
 *
 ____                       _____           _
 |  _ \ __ _ _ __ ___  ___  |_   _|__   ___ | |___
 | |_) / _` | '__/ __|/ _ \   | |/ _ \ / _ \| / __|
 |  __/ (_| | |  \__ \  __/   | | (_) | (_) | \__ \
 |_|   \__,_|_|  |___/\___|   |_|\___/ \___/|_|___/

 *
 */

void cputchar(int c)
{
	if (c < 128)
		putchar(c);

	// Force display of character in graphics mode for immediate feedback
	if (graphics_enabled())
	{
		terminal_flush();
	}
}
void get_next_character()
{
	ctok = getchar();
	if (!allowpromptoverwrite)
	{
		if (ctok == '\b' && sh_getline_count == 0)
			return;
		if (sh_getline_count >= 0)
		{
			cputchar(ctok);
			return;
		}
		int len = strlen(prompt); //+plen;
		if (plen_active)
			len += plen;
		int cpos = get_cursor_x();
		if (ctok == 0x08 && len == cpos)
			return;
		else
			cputchar(ctok);
	}
	else
		cputchar(ctok);
}

void skip_ws()
{
	while (isspace(ctok))
		get_next_character();
}

char *read_label()
{
#define MAX_LABEL (256)
	char *labelbuffer = (char *)malloc(MAX_LABEL);
	memset(labelbuffer, 0, MAX_LABEL);
	size_t count = 0;
	while (ctok >= 'a' && ctok <= 'z' && count < MAX_LABEL)
	{
		labelbuffer[count] = ctok;
		count++;
		get_next_character();
	}
	return labelbuffer;
}

#define MAX_DIGITS (256)
char *read_digits()
{
	char *digitsbuffer = (char *)malloc(MAX_DIGITS);
	memset(digitsbuffer, 0, MAX_DIGITS);
	size_t count = 0;
	while (ctok >= '0' && ctok <= '9' && count < MAX_DIGITS)
	{
		digitsbuffer[count] = ctok;
		count++;
		get_next_character();
	}
	return digitsbuffer;
}

char *read_hexdigits()
{
	char *digitsbuffer = (char *)malloc(MAX_DIGITS);
	memset(digitsbuffer, 0, MAX_DIGITS);
	size_t count = 0;
	while (((ctok >= '0' && ctok <= '9') || (toupper(ctok) >= 'A' && toupper(ctok) <= 'F')) && count < MAX_DIGITS)
	{
		digitsbuffer[count] = ctok;
		count++;
		get_next_character();
	}
	return digitsbuffer;
}

int32_t read_number()
{
	char *digits = read_digits();
	int32_t number = strtol(digits, NULL, 10);
	free(digits);
	return number;
}

int32_t read_hexnumber()
{
	char *hexdigits = read_hexdigits();
	int32_t number = strtol(hexdigits, NULL, 16);
	free(hexdigits);
	return number;
}

char **command_history = NULL;
#define COMMAND_HISTORY_SIZE (128)
int32_t h_index = -1;

// Memory cleanup functions
void cleanup_command_history()
{
	if (command_history)
	{
		for (int i = 0; i < COMMAND_HISTORY_SIZE; i++)
		{
			if (*(command_history + i))
			{
				free(*(command_history + i));
			}
		}
		free(command_history);
		command_history = NULL;
	}
}

int32_t sh_getline(char *buffer, int32_t buflen)
{
	int32_t count = 0;
	ctok = 0;
	while (ctok != '\n' && count < buflen - 1) // Fix: Reserve space for null terminator
	{
		sh_getline_count = count;
		get_next_character();
		if (ctok == VK_UP)
		{
			char *b = *(command_history + h_index);
			if (!b)
			{
				h_index = 0;
			}
			b = *(command_history + h_index);
			if (b)
			{
				char cdirbuffer[VFS_MAXPATHLEN]={0};
				getcwd(cdirbuffer, VFS_MAXPATHLEN);
				int dplen=strlen(cdirbuffer);
				int aplen = strlen(prompt) + dplen;
				int px = get_cursor_x();
				while (px > aplen)
				{
					putchar('\b');
					px--;
				}
				snprintf(buffer, buflen, "%s", b);
				printf(buffer);
				count = strlen(buffer);
				h_index++;
			}
		}
		if (ctok != '\n' && ctok != '\b' && ctok != 27 && ctok < 128)
		{
			if (count < buflen - 1)
			{ // Fix: Add bounds check
				buffer[count] = ctok;
				count++;
			}
		}
		else if (ctok == '\b' && count > 0)
		{
			count--;
			buffer[count] = 0;
		}
	}
	sh_getline_count = -1;
	buffer[count] = '\0'; // Fix: Always null terminate
	if (ctok == '\n')
	{
		if (command_history == NULL)
		{
			command_history = malloc(sizeof(char *) * COMMAND_HISTORY_SIZE);
			for (int i = 0; i < COMMAND_HISTORY_SIZE; i++)
			{
				*(command_history + i) = NULL;
			}
		}
		char *command = *(command_history);
		int have_in_history = false;
		int i = 1;
		while (command != NULL)
		{
			if (strcmp(command, buffer) == 0)
			{
				have_in_history = true;
			}
			command = *(command_history + i);
			i++;
		}

		if (!have_in_history)
		{
			for (int i = 0; i < COMMAND_HISTORY_SIZE; i++)
			{
				if (*(command_history + i) == NULL)
				{
					int len = strlen(buffer) + 1;
					*(command_history + i) = malloc(len);
					snprintf(*(command_history + i), len, "%s", buffer);
					h_index = i;
					return count;
				}
			}
			printf("command history buffer full\n");
		}
	}
	return count;
}

int sh_elfsup(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("usage: elfsup filename\n");
		return 1;
	}

	// Use internal helper with print_pid = 1 to show the PID
	int pid = elf_load_internal(argv[1], 1);

	return (pid > 0) ? 0 : 1;
}

int elf_load_internal(const char *filename, int print_pid)
{
	// Validate filename
	if (!is_safe_filename(filename))
	{
		if (print_pid)
		{
			printf("Error: Invalid filename '%s'\n", filename);
		}
		return 1;
	}

	// Use VFS to open the file (let VFS handle path resolution)
	int fd = vfs_open(filename, FREAD, 0);
	if (fd < 0)
	{
		if (print_pid)
		{
			printf("Error: Cannot open file '%s'\n", filename);
		}
		return 1;
	}

	// Read file in chunks and build buffer dynamically
	size_t buffer_size = 1024;
	size_t total_size = 0;
	void *elf_buffer = malloc(buffer_size);
	
	if (!elf_buffer)
	{
		if (print_pid)
		{
			printf("Error: Memory allocation failed\n");
		}
		vfs_close(fd);
		return 1;
	}
	
	long bytes_read;
	while ((bytes_read = vfs_read(fd, (char*)elf_buffer + total_size, buffer_size - total_size)) > 0)
	{
		total_size += bytes_read;
		if (total_size + 512 > buffer_size)
		{
			buffer_size *= 2;
			void *new_buffer = realloc(elf_buffer, buffer_size);
			if (!new_buffer)
			{
				free(elf_buffer);
				vfs_close(fd);
				if (print_pid)
				{
					printf("Error: Memory reallocation failed\n");
				}
				return 1;
			}
			elf_buffer = new_buffer;
		}
	}
	
	size_t file_size = total_size;
	vfs_close(fd);
	
	if (bytes_read < 0 || file_size == 0)
	{
		if (print_pid)
		{
			printf("Error: Failed to read file or empty file\n");
		}
		free(elf_buffer);
		return 1;
	}

	// Validate ELF file format
	Elf_Ehdr *ehdr = (Elf_Ehdr *)elf_buffer;
	if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
		ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F')
	{
		if (print_pid)
		{
			printf("Error: Invalid ELF magic number\n");
		}
		free(elf_buffer);
		return 1;
	}

	if (ehdr->e_type != 2)
	{ // ET_EXEC
		if (print_pid)
		{
			printf("Error: Not an executable ELF file (type=%d)\n", ehdr->e_type);
		}
		free(elf_buffer);
		return 1;
	}

	if (ehdr->e_machine != 3)
	{ // EM_386
		if (print_pid)
		{
			printf("Error: Not an i386 ELF file (machine=%d)\n", ehdr->e_machine);
		}
		free(elf_buffer);
		return 1;
	}

	// Create ELF process via system call
	int pid = syscall_os_create_elf_process(elf_buffer, file_size);

	if (pid > 0)
	{
		if (print_pid)
		{
			printf("Process %d\n", pid);
		}
	}
	else
	{
		if (print_pid)
		{
			printf("Failed to create ELF process (error %d)\n", pid);
		}
	}

	free(elf_buffer);
	return (pid > 0) ? pid : -1; // Return PID on success, -1 on failure
}

int elf_process_quickload(const char *filename)
{
	// Load ELF file without printing PID
	int pid = elf_load_internal(filename, 0);

	if (pid <= 0)
	{
		return 1;
	}

	// Execute the process immediately
	int result = syscall_os_exec_process(pid);

	return (result == 0) ? 0 : 1;
}

// Sound card command implementation
int sh_sound(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("Usage: sound <command>\n");
		printf("Commands:\n");
		printf("  init     - Initialize sound card\n");
		printf("  detect   - Detect sound card\n");
		printf("  beep     - Test beep sounds\n");
		printf("  tones    - Test tone generation\n");
		printf("  simple   - Test simple single-cycle DMA\n");
		printf("  rawbeep  - Test simple raw square wave\n");
		printf("  16bit    - Test 16-bit stereo audio\n");
		printf("  volume <0-255> - Set volume\n");
		printf("  play <freq> <duration> - Play frequency\n");
		printf("  speaker <freq> <duration> - Test PC speaker\n");
		return 0;
	}

	if (strcmp(argv[1], "init") == 0)
	{
		if (sb16_init())
		{
			printf("Sound card initialized successfully\n");
		}
		else
		{
			printf("Failed to initialize sound card\n");
		}
		return 0;
	}

	if (strcmp(argv[1], "detect") == 0)
	{
		if (sb16_detect())
		{
			printf("Sound Blaster 16 detected\n");
		}
		else
		{
			printf("No Sound Blaster 16 found\n");
		}
		return 0;
	}

	if (strcmp(argv[1], "beep") == 0)
	{
		sb16_test_beep();
		return 0;
	}

	if (strcmp(argv[1], "tones") == 0)
	{
		sb16_test_tones();
		return 0;
	}

	if (strcmp(argv[1], "simple") == 0)
	{
		sb16_test_simple_dma();
		return 0;
	}

	if (strcmp(argv[1], "rawbeep") == 0)
	{
		sb16_test_simple_beep();
		return 0;
	}

	if (strcmp(argv[1], "16bit") == 0)
	{
		printf("Testing 16-bit stereo audio...\n");
		sb16_beep_16bit(440, 500); // A4 note for 500ms
		sb16_beep_16bit(523, 500); // C5 note for 500ms
		sb16_beep_16bit(659, 500); // E5 note for 500ms
		printf("16-bit stereo test complete\n");
		return 0;
	}

	if (strcmp(argv[1], "volume") == 0)
	{
		if (argc < 3)
		{
			printf("Usage: sound volume <0-15>\n");
			return 1;
		}
		int volume = atoi(argv[2]);
		if (volume < 0 || volume > 15)
		{
			printf("Volume must be between 0 and 15\n");
			return 1;
		}
		// Convert 0-15 to both left and right channels (0-255 internal range)
		int internal_volume = (volume << 4) | volume;	 // Both channels same level
		internal_volume = (internal_volume * 255) / 255; // Already 0-255, just for clarity
		sb16_set_volume(internal_volume);
		printf("Volume set to %d (mixer value: 0x%02X)\n", volume, (volume << 4) | volume);
		return 0;
	}

	if (strcmp(argv[1], "play") == 0)
	{
		if (argc < 4)
		{
			printf("Usage: sound play <frequency> <duration_ms>\n");
			return 1;
		}
		int freq = atoi(argv[2]);
		int duration = atoi(argv[3]);
		if (freq < 20 || freq > 20000)
		{
			printf("Frequency must be between 20 and 20000 Hz\n");
			return 1;
		}
		if (duration < 1 || duration > 10000)
		{
			printf("Duration must be between 1 and 10000 ms\n");
			return 1;
		}
		sb16_play_tone(freq, duration);
		return 0;
	}

	if (strcmp(argv[1], "speaker") == 0)
	{
		if (argc < 4)
		{
			printf("Usage: sound speaker <frequency> <duration_ms>\n");
			return 1;
		}
		int freq = atoi(argv[2]);
		int duration = atoi(argv[3]);
		if (freq < 20 || freq > 20000)
		{
			printf("Frequency must be between 20 and 20000 Hz\n");
			return 1;
		}
		if (duration < 1 || duration > 10000)
		{
			printf("Duration must be between 1 and 10000 ms\n");
			return 1;
		}
		printf("Testing PC speaker at %d Hz for %d ms\n", freq, duration);
		pc_speaker_beep(freq, duration);
		return 0;
	}

	printf("Unknown sound command: %s\n", argv[1]);
	return 1;
}

// AMP command implementation
int sh_amp(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("Usage: amp <command>\n");
		printf("Commands:\n");
		printf("  status   - Show AMP status and CPU info\n");
		printf("  test     - Run AMP examples\n");
		printf("  examples - Run all example functions\n");
		return 1;
	}

	if (strcmp(argv[1], "status") == 0)
	{
		printf("AMP Status:\n");
		printf("  Initialized: %s\n", syscall_os_amp_is_initialized() ? "Yes" : "No");
		printf("  Active CPUs: %d\n", syscall_os_amp_get_cpu_count());
		// Re-enable detailed status with safety checks
		syscall_os_amp_print_status();
		return 0;
	}
	else if (strcmp(argv[1], "test") == 0)
	{
		printf("Running basic AMP test...\n");
		if (!syscall_os_amp_is_initialized())
		{
			printf("AMP not initialized\n");
			return 1;
		}
		
		printf("Running AMP examples (this tests all functionality)...\n");
		printf("AMP examples have been removed. Use 'amp exec <filename>' to test CPU execution.\n");
		return 0;
	}
	else if (strcmp(argv[1], "examples") == 0)
	{
		printf("Running AMP example functions...\n");
		printf("AMP examples have been removed. Use 'amp exec <filename>' to test CPU execution.\n");
		return 0;
	}

	printf("Unknown amp command: %s\n", argv[1]);
	return 1;
}

int sh_net(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("Usage: net <command>\n");
		printf("Commands:\n");
		printf("  detect     - Detect and initialize network cards\n");
		printf("  status     - Show network card status\n");
		printf("  stats      - Show network statistics\n");
		printf("  test       - Send test packet\n");
		return 1;
	}

	if (strcmp(argv[1], "detect") == 0)
	{
		printf("Scanning for network cards...\n");
		
		// Call the RTL8139 initialization function (which includes detection)
		bool initialized = rtl8139_init();
		
		if (initialized)
		{
			printf("RTL8139 network card detected and initialized\n");
		}
		else
		{
			printf("No supported network cards found\n");
		}
		return initialized ? 0 : 1;
	}
	else if (strcmp(argv[1], "status") == 0)
	{
		// Show network card status
		rtl8139_show_status();
		return 0;
	}
	else if (strcmp(argv[1], "stats") == 0)
	{
		// Show network statistics
		rtl8139_show_stats();
		return 0;
	}
	else if (strcmp(argv[1], "test") == 0)
	{
		printf("Sending test packet...\n");
		
		// Test packet transmission
		int result = rtl8139_send_test_packet();
		
		if (result == 0)
		{
			printf("Test packet sent successfully\n");
		}
		else
		{
			printf("Failed to send test packet (error %d)\n", result);
		}
		return result;
	}

	printf("Unknown net command: %s\n", argv[1]);
	return 1;
}

int sh_set_mode(int argc, char **argv) {
	if (argc == 1) {
		// Display current mode and available modes
		graphics_mode_t *mode = graphics_get_mode();
		if (mode && mode->enabled) {
			printf("Current graphics mode: %dx%dx%d\n", mode->width, mode->height, mode->bpp);
		} else {
			printf("Graphics not enabled\n");
		}
		
		printf("\nUsage: set-mode <width> <height> [bpp]\n");
		printf("Common resolutions:\n");
		printf("  640x480    - VGA\n");
		printf("  800x600    - SVGA\n");
		printf("  1024x768   - XGA\n");
		printf("  1280x720   - HD 720p\n");
		printf("  1280x1024  - SXGA\n");
		printf("  1920x1080  - Full HD 1080p\n");
		printf("\nSupported BPP: 16, 24, 32 (default: 32)\n");
		return 0;
	}
	
	if (argc < 3) {
		printf("Usage: set-mode <width> <height> [bpp]\n");
		return 1;
	}
	
	// Parse arguments
	int width = atoi(argv[1]);
	int height = atoi(argv[2]);
	int bpp = (argc >= 4) ? atoi(argv[3]) : 32;  // Default to 32 bpp
	
	// Validate parameters
	if (width <= 0 || height <= 0) {
		printf("ERROR: Invalid resolution %dx%d\n", width, height);
		return 1;
	}
	
	if (bpp != 16 && bpp != 24 && bpp != 32) {
		printf("ERROR: Invalid BPP %d (supported: 16, 24, 32)\n", bpp);
		return 1;
	}
	
	// Warn about terminal compatibility
	if (width < 640 || height < 480) {
		printf("WARNING: Resolution %dx%d may be too small for terminal output\n", width, height);
	}
	
	// Attempt to set the mode
	printf("Setting graphics mode to %dx%dx%d...\n", width, height, bpp);
	
	if (graphics_set_mode(width, height, bpp)) {
		printf("Graphics mode changed successfully\n");
		
		// Display some info about the new mode
		graphics_mode_t *new_mode = graphics_get_mode();
		if (new_mode) {
			printf("New mode: %dx%dx%d (pitch: %d bytes)\n", 
				   new_mode->width, new_mode->height, new_mode->bpp, new_mode->pitch);
			printf("Terminal size: %dx%d characters\n", 
				   terminal_get_cols(), terminal_get_rows());
		}
		return 0;
	} else {
		printf("ERROR: Failed to set graphics mode\n");
		return 1;
	}
}

// Mount filesystem command
int sh_mount(int argc, char **argv) {
	if (argc < 4) {
		printf("Usage: mount <device> <mount_point> <filesystem_type>\n");
		printf("Examples:\n");
		printf("  mount disk0 / fat32   - Mount primary disk as root\n");
		printf("  mount ram0 /tmp fat32 - Mount RAM disk at /tmp\n");
		printf("  mount devfs /dev devfs    - Mount device filesystem\n");
		return 1;
	}
	
	const char *device = argv[1];
	const char *mount_point = argv[2];
	const char *fstype = argv[3];
	
	printf("Mounting %s at %s (type: %s)...\n", device, mount_point, fstype);
	
	// Call VFS mount function
	if (vfs_mount(device, mount_point, fstype, 0, NULL) == 0) {
		printf("Mount successful\n");
		return 0;
	} else {
		printf("Mount failed\n");
		return 1;
	}
}

// Unmount filesystem command
int sh_unmount(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage: unmount <mount_point>\n");
		printf("Examples:\n");
		printf("  unmount /tmp    - Unmount /tmp\n");
		printf("  unmount /dev    - Unmount device filesystem\n");
		return 1;
	}
	
	const char *mount_point = argv[1];
	
	printf("Unmounting %s...\n", mount_point);
	
	// Call VFS unmount function
	if (vfs_unmount(mount_point, 0) == 0) {
		printf("Unmount successful\n");
		return 0;
	} else {
		printf("Unmount failed\n");
		return 1;
	}
}

// List block devices command
int sh_lsdev(int argc, char **argv) {
	printf("Listing block devices:\n");
	blockdev_list();
	return 0;
}

// Display filesystem command (like df)
int sh_df(int argc, char **argv) {
	printf("Mounted filesystems:\n");
	vfs_list_mounts();
	return 0;
}

// AMP Process execution command
int sh_amp_exec(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage: amp_exec <elf_file> [cpu_core]\n");
		printf("Execute an ELF program on a specific CPU core via AMP\n");
		printf("  elf_file  - ELF file to execute\n");
		printf("  cpu_core  - CPU core (0-3, default 0)\n");
		printf("              0 = master CPU (uses syscalls)\n");
		printf("              1-3 = secondary CPUs (no syscalls, messaging only)\n");
		return 1;
	}

	// Parse CPU core parameter (default to 0)
	int target_cpu = 0;
	if (argc >= 3) {
		target_cpu = atoi(argv[2]);
		if (target_cpu < 0 || target_cpu > 3) {
			printf("Error: CPU core must be 0-3\n");
			return 1;
		}
	}

	printf("Target CPU core: %d\n", target_cpu);

	// Check if file exists and is accessible
	if (!is_safe_filename(argv[1])) {
		printf("Error: Invalid filename\n");
		return 1;
	}

	// Try with .elf extension if not provided
	char *filename = malloc(128);
	if (strstr(argv[1], ".elf") != NULL) {
		snprintf(filename, 128, "%s", argv[1]);
	} else {
		snprintf(filename, 128, "%s.elf", argv[1]);
	}

	printf("Loading ELF file %s...\n", filename);

	// Create ELF process via system call and execute through AMP system
	int result;

	// All CPUs now go through the AMP system for consistent counting
	// Only for secondary CPUs (1-3): Load file manually for AMP execution
	// Use VFS to open file (handles path resolution like shell_exec does)
	int fd = vfs_open(filename, FREAD, 0);
	if (fd < 0) {
		printf("Error: Cannot open file %s\n", filename);
		free(filename);
		return 1;
	}

	// Get file size using VFS
	long file_size = vfs_lseek(fd, 0, VFS_SEEK_END);
	vfs_lseek(fd, 0, VFS_SEEK_SET);

	if (file_size <= 0) {
		printf("Error: File %s is empty or invalid\n", filename);
		vfs_close(fd);
		free(filename);
		return 1;
	}

	// Allocate buffer and read file using VFS
	void *elf_data = malloc(file_size);  // Use regular malloc instead of kmalloc
	if (!elf_data) {
		printf("Error: Cannot allocate memory for ELF file\n");
		vfs_close(fd);
		free(filename);
		return 1;
	}

	long bytes_read = vfs_read(fd, elf_data, file_size);
	vfs_close(fd);

	if (bytes_read != file_size) {
		printf("Error: Failed to read complete ELF file (read %ld, expected %ld)\n", bytes_read, file_size);
		free(elf_data);
		free(filename);
		return 1;
	}

	printf("Loaded %ld bytes, creating ELF process...\n", file_size);

	// Validate ELF file format (like elf_load_internal does)
	Elf_Ehdr *ehdr = (Elf_Ehdr *)elf_data;
	if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
		ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
		printf("Error: Invalid ELF magic number\n");
		free(elf_data);
		free(filename);
		return 1;
	}

	if (ehdr->e_type != 2) { // ET_EXEC
		printf("Error: Not an executable ELF file (type=%d)\n", ehdr->e_type);
		free(elf_data);
		free(filename);
		return 1;
	}

	if (ehdr->e_machine != 3) { // EM_386
		printf("Error: Not an i386 ELF file (machine=%d)\n", ehdr->e_machine);
		free(elf_data);
		free(filename);
		return 1;
	}

	// Handle CPU0 differently - it's not part of AMP system
	if (target_cpu == 0) {
		printf("Creating process for execution on CPU 0 (master CPU with syscalls)...\n");
		
		// CPU0 executes directly using normal syscall path
		int pid = elf_load_internal(filename, 0);  // 0 = don't print PID
		
		if (pid <= 0) {
			printf("Failed to create process for %s\n", filename);
			free(elf_data);
			free(filename);
			return 1;
		}
		
		printf("Created process %d, executing on CPU 0 (master)...\n", pid);
		
		// Execute the process using normal syscall mechanism
		result = syscall_os_exec_process(pid);
		
		if (result == 0) {
			printf("Process execution completed successfully on CPU 0\n");
		} else {
			printf("Process execution failed on CPU 0, result: %d\n", result);
		}
		
	} else {
		// CPU 1-3 (secondary): Use AMP execution without syscalls
		printf("Creating process for execution on CPU %d (no syscalls, messaging only)...\n", target_cpu);
		
		// First create the process normally
		int pid = elf_load_internal(filename, 0);  // 0 = don't print PID
		
		if (pid <= 0) {
			printf("Failed to create process for %s\n", filename);
			free(elf_data);
			free(filename);
			return 1;
		}
		
		printf("Created process %d, executing on CPU %d...\n", pid, target_cpu);
		
		// Execute on the specified secondary CPU using our AMP system
		extern int amp_exec_process_on_cpu(int pid, int cpu_id, int wait_for_completion);
		result = amp_exec_process_on_cpu(pid, target_cpu, 1);  // 1 = wait for completion
		
		if (result == 0) {
			printf("Process execution completed successfully on CPU %d\n", target_cpu);
		} else {
			printf("Process execution failed on CPU %d, result: %d\n", target_cpu, result);
		}
	}
	
	free(elf_data);
	free(filename);
	
	return result;
}

int isaddop(char c)
{
	return (c == '+' || c == '-');
}
int ismulop(char c)
{
	return (c == '*' || c == '/');
}

// Vi editor command
int sh_vi(int argc, char **argv)
{
	// External function from vi.c
	extern void vi_main(const char* filename);
	
	if (argc < 2) {
		vi_main(NULL);
	} else {
		vi_main(argv[1]);
	}
	return 0;
}
