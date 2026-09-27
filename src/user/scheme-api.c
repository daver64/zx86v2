#include "common.h"
#include "graphics.h"
#include <scheme.h>
#include "scheme-private.h"
#include "paging.h"
#include "kheap.h"
#include "task.h"
#include "timer.h"
#include "amp.h"

extern int want_scheme_quit;
extern uint32_t nframes;
extern uint32_t tick;
extern heap_t *kheap;
extern volatile task_t *current_task;
extern volatile amp_workspace_t *amp_workspace;
extern bool amp_initialized;

#define SCHEME_BUFFER_SIZE (4096)
int32_t sh_getline(char *buffer, int32_t buflen);

pointer sc_exit(scheme *sc, pointer args)
{
	want_scheme_quit = true;
	return sc->NIL;
}

pointer sc_gotoxy(scheme *sc, pointer args)
{
	int x = sc->vptr->ivalue(sc->vptr->pair_car(args));
	args = sc->vptr->pair_cdr(args);
	int y = sc->vptr->ivalue(sc->vptr->pair_car(args));
	gotoxy(x, y);
	return sc->NIL;
}

pointer sc_cls(scheme *sc, pointer args)
{
	cls();
	return sc->NIL;
}

pointer sc_textcolour(scheme *sc, pointer args)
{
	char *fgcs = sc->vptr->string_value(sc->vptr->pair_car(args));
	args = sc->vptr->pair_cdr(args);
	char *bgcs = sc->vptr->string_value(sc->vptr->pair_car(args));

	int fgc = get_foreground_colour();
	int bgc = get_background_colour();

	if (!strcmp(fgcs, "VGA_BLACK"))
		fgc = VGA_BLACK;
	else if (!strcmp(fgcs, "VGA_BLUE"))
		fgc = VGA_BLUE;
	else if (!strcmp(fgcs, "VGA_GREEN"))
		fgc = VGA_GREEN;
	else if (!strcmp(fgcs, "VGA_CYAN"))
		fgc = VGA_CYAN;
	else if (!strcmp(fgcs, "VGA_RED"))
		fgc = VGA_RED;
	else if (!strcmp(fgcs, "VGA_MAGENTA"))
		fgc = VGA_MAGENTA;
	else if (!strcmp(fgcs, "VGA_BROWN"))
		fgc = VGA_BROWN;
	else if (!strcmp(fgcs, "VGA_LIGHTGREY"))
		fgc = VGA_LIGHTGREY;
	else if (!strcmp(fgcs, "VGA_DARKGREY"))
		fgc = VGA_DARKGREY;
	else if (!strcmp(fgcs, "VGA_LIGHTBLUE"))
		fgc = VGA_LIGHTBLUE;
	else if (!strcmp(fgcs, "VGA_LIGHTGREEN"))
		fgc = VGA_LIGHTGREEN;
	else if (!strcmp(fgcs, "VGA_LIGHTCYAN"))
		fgc = VGA_LIGHTCYAN;
	else if (!strcmp(fgcs, "VGA_LIGHTRED"))
		fgc = VGA_LIGHTRED;
	else if (!strcmp(fgcs, "VGA_LIGHTMAGENTA"))
		fgc = VGA_LIGHTMAGENTA;
	else if (!strcmp(fgcs, "VGA_YELLOW"))
		fgc = VGA_YELLOW;
	else if (!strcmp(fgcs, "VGA_WHITE"))
		fgc = VGA_WHITE;

	if (!strcmp(bgcs, "VGA_BLACK"))
		bgc = VGA_BLACK;
	else if (!strcmp(bgcs, "VGA_BLUE"))
		bgc = VGA_BLUE;
	else if (!strcmp(bgcs, "VGA_GREEN"))
		bgc = VGA_GREEN;
	else if (!strcmp(bgcs, "VGA_CYAN"))
		bgc = VGA_CYAN;
	else if (!strcmp(bgcs, "VGA_RED"))
		bgc = VGA_RED;
	else if (!strcmp(bgcs, "VGA_MAGENTA"))
		bgc = VGA_MAGENTA;
	else if (!strcmp(bgcs, "VGA_BROWN"))
		bgc = VGA_BROWN;
	else if (!strcmp(bgcs, "VGA_LIGHTGREY"))
		bgc = VGA_LIGHTGREY;
	else if (!strcmp(bgcs, "VGA_DARKGREY"))
		bgc = VGA_DARKGREY;
	else if (!strcmp(bgcs, "VGA_LIGHTBLUE"))
		bgc = VGA_LIGHTBLUE;
	else if (!strcmp(bgcs, "VGA_LIGHTGREEN"))
		bgc = VGA_LIGHTGREEN;
	else if (!strcmp(bgcs, "VGA_LIGHTCYAN"))
		bgc = VGA_LIGHTCYAN;
	else if (!strcmp(bgcs, "VGA_LIGHTRED"))
		bgc = VGA_LIGHTRED;
	else if (!strcmp(bgcs, "VGA_LIGHTMAGENTA"))
		bgc = VGA_LIGHTMAGENTA;
	else if (!strcmp(bgcs, "VGA_YELLOW"))
		bgc = VGA_YELLOW;
	else if (!strcmp(bgcs, "VGA_WHITE"))
		bgc = VGA_WHITE;

	set_text_colour(fgc, bgc);
	return sc->NIL;
}

// Kernel introspection functions
pointer sc_memory_stats(scheme *sc, pointer args)
{
	char output_buffer[1024];
	char temp_buffer[256];
	output_buffer[0] = '\0';  // Initialize empty string
	
	strcat(output_buffer, "Memory Statistics:\n");
	
	// Add total frames
	sprintf(temp_buffer, "  Total Frames: %u\n", nframes);
	strcat(output_buffer, temp_buffer);
	
	// Calculate total memory in MB
	uint32_t total_memory_mb = (nframes * 4096) / (1024 * 1024);
	sprintf(temp_buffer, "  Total Memory: %u MB\n", total_memory_mb);
	strcat(output_buffer, temp_buffer);
	
	// Add heap information if available
	if (kheap) {
		strcat(output_buffer, "\nHeap Information:\n");
		sprintf(temp_buffer, "  Heap Start: 0x%08X\n", kheap->start_address);
		strcat(output_buffer, temp_buffer);
		
		sprintf(temp_buffer, "  Heap End: 0x%08X\n", kheap->end_address);
		strcat(output_buffer, temp_buffer);
		
		sprintf(temp_buffer, "  Heap Max: 0x%08X\n", kheap->max_address);
		strcat(output_buffer, temp_buffer);
		
		uint32_t heap_size = kheap->end_address - kheap->start_address;
		uint32_t heap_size_kb = heap_size / 1024;
		sprintf(temp_buffer, "  Current Heap Size: %u KB\n", heap_size_kb);
		strcat(output_buffer, temp_buffer);
	} else {
		strcat(output_buffer, "\nHeap: Not initialized\n");
	}
	
	return sc->vptr->mk_string(sc, output_buffer);
}

pointer sc_system_info(scheme *sc, pointer args)
{
	char output_buffer[512];
	char temp_buffer[128];
	output_buffer[0] = '\0';
	
	strcat(output_buffer, "System Information:\n");
	
	// Add timer ticks (uptime)
	sprintf(temp_buffer, "  Timer Ticks: %u\n", tick);
	strcat(output_buffer, temp_buffer);
	
	// Add uptime in seconds (assuming 100Hz timer)
	uint32_t uptime_seconds = tick / 100;
	uint32_t hours = uptime_seconds / 3600;
	uint32_t minutes = (uptime_seconds % 3600) / 60;
	uint32_t seconds = uptime_seconds % 60;
	
	sprintf(temp_buffer, "  Uptime: %u:%02u:%02u (%u seconds)\n", 
		hours, minutes, seconds, uptime_seconds);
	strcat(output_buffer, temp_buffer);
	
	return sc->vptr->mk_string(sc, output_buffer);
}

pointer sc_process_info(scheme *sc, pointer args)
{
	char output_buffer[512];
	char temp_buffer[128];
	output_buffer[0] = '\0';
	
	strcat(output_buffer, "Process Information:\n");
	
	if (current_task) {
		sprintf(temp_buffer, "  Current PID: %d\n", current_task->id);
		strcat(output_buffer, temp_buffer);
		
		sprintf(temp_buffer, "  Stack Pointer (ESP): 0x%08X\n", current_task->esp);
		strcat(output_buffer, temp_buffer);
		
		sprintf(temp_buffer, "  Base Pointer (EBP): 0x%08X\n", current_task->ebp);
		strcat(output_buffer, temp_buffer);
		
		sprintf(temp_buffer, "  Instruction Pointer (EIP): 0x%08X\n", current_task->eip);
		strcat(output_buffer, temp_buffer);
		
		sprintf(temp_buffer, "  Page Directory: 0x%08X\n", (uint32_t)current_task->page_directory);
		strcat(output_buffer, temp_buffer);
		
		sprintf(temp_buffer, "  Kernel Stack: 0x%08X\n", current_task->kernel_stack);
		strcat(output_buffer, temp_buffer);
	} else {
		strcat(output_buffer, "  Error: No current task\n");
	}
	
	return sc->vptr->mk_string(sc, output_buffer);
}

pointer sc_cpu_info(scheme *sc, pointer args)
{
	char output_buffer[512];
	char temp_buffer[128];
	output_buffer[0] = '\0';
	
	strcat(output_buffer, "CPU Information:\n");
	
	sprintf(temp_buffer, "  Architecture: i386\n");
	strcat(output_buffer, temp_buffer);
	
	sprintf(temp_buffer, "  Paging: Enabled\n");
	strcat(output_buffer, temp_buffer);
	
	if (amp_initialized) {
		uint8_t cpu_count = amp_get_cpu_count();
		sprintf(temp_buffer, "  Total CPUs: %d\n", cpu_count);
		strcat(output_buffer, temp_buffer);
		
		uint8_t current_cpu = amp_get_current_cpu_id();
		sprintf(temp_buffer, "  Current CPU: %d\n", current_cpu);
		strcat(output_buffer, temp_buffer);
	} else {
		sprintf(temp_buffer, "  AMP Status: Not initialized\n");
		strcat(output_buffer, temp_buffer);
	}
	
	return sc->vptr->mk_string(sc, output_buffer);
}

pointer sc_amp_stats(scheme *sc, pointer args)
{
	char output_buffer[2048];
	char temp_buffer[256];
	output_buffer[0] = '\0';  // Initialize empty string
	
	// Check if AMP is initialized
	strcat(output_buffer, "AMP Status:\n");
	sprintf(temp_buffer, "  Initialized: %s\n", amp_initialized ? "yes" : "no");
	strcat(output_buffer, temp_buffer);
	
	if (amp_initialized && amp_workspace) {
		// CPU information
		uint8_t cpu_count = amp_get_cpu_count();
		sprintf(temp_buffer, "  CPU Count: %d\n", cpu_count);
		strcat(output_buffer, temp_buffer);
		
		uint8_t current_cpu = amp_get_current_cpu_id();
		sprintf(temp_buffer, "  Current CPU: %d\n", current_cpu);
		strcat(output_buffer, temp_buffer);
		
		// Workspace information
		sprintf(temp_buffer, "  Workspace Address: 0x%08X\n", (uint32_t)amp_workspace);
		strcat(output_buffer, temp_buffer);
		
		// Statistics
		strcat(output_buffer, "\nStatistics:\n");
		sprintf(temp_buffer, "  Total Functions Executed: %u\n", amp_workspace->total_functions_executed);
		strcat(output_buffer, temp_buffer);
		
		sprintf(temp_buffer, "  Total Errors: %u\n", amp_workspace->total_errors);
		strcat(output_buffer, temp_buffer);
		
		sprintf(temp_buffer, "  Total Timeouts: %u\n", amp_workspace->total_timeouts);
		strcat(output_buffer, temp_buffer);
		
		// Queue status
		strcat(output_buffer, "\nQueue Status:\n");
		sprintf(temp_buffer, "  Queue Head: %u\n", amp_workspace->queue_head);
		strcat(output_buffer, temp_buffer);
		
		sprintf(temp_buffer, "  Queue Tail: %u\n", amp_workspace->queue_tail);
		strcat(output_buffer, temp_buffer);
		
		sprintf(temp_buffer, "  Next Task ID: %u\n", amp_workspace->next_task_id);
		strcat(output_buffer, temp_buffer);
		
		// CPU status for each CPU
		strcat(output_buffer, "\nPer-CPU Status:\n");
		for (int i = 0; i < cpu_count && i < 4; i++) {
			sprintf(temp_buffer, "  CPU %d: %s, Role: %d, Functions: %u\n",
				amp_workspace->cpu_info[i].cpu_id,
				amp_workspace->cpu_info[i].online ? "Online" : "Offline",
				amp_workspace->cpu_info[i].role,
				amp_workspace->cpu_info[i].functions_executed);
			strcat(output_buffer, temp_buffer);
		}
	} else {
		strcat(output_buffer, "  AMP not initialized or workspace unavailable\n");
	}
	
	return sc->vptr->mk_string(sc, output_buffer);
}

pointer sc_listregistered(scheme *sc, pointer args);
scheme_registerable ff_list[] = {
	{sc_exit, "exit"},
	{sc_gotoxy, "gotoxy"},
	{sc_cls, "cls"},
	{sc_listregistered, "api"},
	{sc_textcolour, "textcolour"},
	{sc_memory_stats, "memory-stats"},
	{sc_system_info, "system-info"},
	{sc_process_info, "process-info"},
	{sc_cpu_info, "cpu-info"},
	{sc_amp_stats, "amp-stats"}};

pointer sc_listregistered(scheme *sc, pointer args)
{
	int s = sizeof(ff_list) / sizeof(scheme_registerable);
	scheme_registerable *rg = &ff_list[0];
	while (s--)
	{
		printf("%s ", rg->name);
		rg++;
	}
	printf("\n");
	terminal_flush();
	return sc->NIL;
}

scheme *sc_init()
{
	int s = sizeof(ff_list) / sizeof(scheme_registerable);
	want_scheme_quit = false;
	scheme *sc;
	//printf("scheme_init_new()\n");
	sc = scheme_init_new();
	//printf("set ports\n");
	scheme_set_input_port_file(sc, stdin);
	scheme_set_output_port_file(sc, stdout);
	scheme_register_foreign_func_list(sc, ff_list, s);
	return sc;
}

extern char *prompt;
extern int plen_active;
int sc_main(int argc, char **argv)
{
	int ofgc = get_foreground_colour();
	int obgc = get_background_colour();

	plen_active = false;
	sprintf(prompt, "%s", "#");
	int result = 0;
	//printf("init 1\n");
	scheme *sc = sc_init();
	//printf("init 2\n");
	char *lbuffer = malloc(SCHEME_BUFFER_SIZE);
	char *sbuffer = malloc(SCHEME_BUFFER_SIZE);
	char *obuffer = malloc(SCHEME_BUFFER_SIZE);
	char *obuffer_end = obuffer + SCHEME_BUFFER_SIZE;
	
	int count = 0;

	memset(sbuffer, 0, SCHEME_BUFFER_SIZE);
	memset(lbuffer, 0, SCHEME_BUFFER_SIZE);
	memset(obuffer, 0, SCHEME_BUFFER_SIZE);
	FILE *scminit = fopen("/init.scm", "r");
	//printf("init 3\n");
	if (scminit)
	{
		int len = fgetsize(scminit);
		char *buf = malloc(len + 1);
		memset(buf, 0, len + 1);
		fread(buf, 1, len, scminit);
		fclose(scminit);
		scheme_load_string(sc, buf);
		free(buf);
	}
	if (argc == 2)
	{
		FILE *fp = fopen(argv[1], "r");
		if (fp)
		{
			int len = fgetsize(fp);
			char *buf = malloc(len + 1);
			memset(buf, 0, len + 1);
			fread(buf, 1, len, fp);
			scheme_load_string(sc, buf);
			fclose(fp);
			free(buf);
			sprintf(prompt, "%s", ">");
			plen_active = true;
			scheme_deinit(sc);
			free(sbuffer);
			free(lbuffer);
			free(obuffer);
			return result;
		}
	}
	else
	{
		//printf("init 4\n");
		printf("TinyScheme 2.3.0\n");
		printf("%s", prompt);
		terminal_flush();
		do
		{
			sh_getline(lbuffer, SCHEME_BUFFER_SIZE);
			if (strlen(lbuffer) > 1)
			{
				sprintf(sbuffer, "(display %s)", lbuffer);
				memset(obuffer, 0, SCHEME_BUFFER_SIZE);
				scheme_set_output_port_string(sc, obuffer, obuffer_end);
				scheme_load_string(sc, sbuffer);
				if ((strcmp(obuffer, "()") != 0)) //&& (strcmp(obuffer,"#t")!=0) && (strcmp(obuffer,"#f")!=0) )
				{
					printf("%s\n%s", obuffer, prompt);
					terminal_flush();
				}
				else
				{
					if (!want_scheme_quit)
					{
						printf("%s", prompt);
						terminal_flush();
					}
				}
				memset(sbuffer, 0, SCHEME_BUFFER_SIZE);
				memset(lbuffer, 0, SCHEME_BUFFER_SIZE);
				count++;
			}
		} while (!want_scheme_quit);
	}
	scheme_deinit(sc);
	free(sc);
	sc=NULL;
	free(sbuffer);
	free(lbuffer);
	free(obuffer);
	set_foreground_colour(ofgc);
	set_background_colour(obgc);
	sprintf(prompt, "%s", ">");
	plen_active = true;
	return result;
}
