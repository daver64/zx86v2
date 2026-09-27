#include "common.h"
#include "graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <elf-module.h>

static void load_module(char *module);
static void *elf_malloc(size_t n);
static unsigned char *elf_memory_base = NULL;// (unsigned char *)0x3000000;
static unsigned char *elf_memory_ptr = NULL; // elf_memory_base;
extern uint32_t user_elf_start;
extern uint32_t user_elf_end;
/* Module table entry */
typedef struct module
{
	struct module *next;
	char *name;
} module_t;

/* Symbol table entry */
typedef struct symbol
{
	struct symbol *next;
	module_t *module;
	void *addr;
	char name[0];
} symbol_t;

/* Symbol table */
static symbol_t *symtab = NULL;
static module_t *modtab = NULL;

/* Program options */
static int verbose = 0;
static int loadfailed = true;
extern uint32_t user_elf_start;
static void init_core_module()
{
	symtab = NULL;
	modtab = NULL;
	elf_memory_base = (unsigned char*)user_elf_start;
	// if(!elf_memory_base)
	//	elf_memory_base=malloc(0x1000000);
	modtab = elf_malloc(sizeof(module_t)); // + strlen("core") + 1);
	modtab->name = elf_malloc(strlen("core") + 1);
	sprintf(modtab->name, "%s", "core");
	modtab->next = NULL;
	// symtab=NULL;
	module_t *mod = modtab;
	//KERNEL_LOG("%s", ">>Modules:\n");
	while (mod)
	{
	//	VPRINTF(">>%s::%08X\n", mod->name, (uint32_t)mod->next);
		mod = mod->next;
	}
	//VPRINTF("%s", ">>EndModules:\n");
}

void elf_release()
{
	memset(elf_memory_base, 0x00, 0x1000000);
	elf_memory_ptr = elf_memory_base;
}
void *elf_malloc(size_t n)
{
	if (!elf_memory_ptr)
		elf_memory_ptr = elf_memory_base;
	void *ptr = (void *)elf_memory_ptr;
	elf_memory_ptr += n;
	return ptr;
}

static void *load_file(char *file, size_t *size)
{
	FILE *f = path_fopen(file, "r");
	void *result;

	if (!f)
	{
		printf("fopen file on load_file returning null {%s}\n", file);
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	*size = ftell(f);
	fseek(f, 0, SEEK_SET);
	result = elf_malloc(*size);
	// result = malloc(*size);
	// printf("alloc %u bytes for file %s\n",*size,file);
	if (!result)
		goto out;

	if (fread(result, 1, *size, f) < *size)
	{
		// free(result);
		result = NULL;
		goto out;
	}

out:
	//	printf("file load %s %u bytes\n",file,*size);
	fclose(f);
	return result;
}

/*
 * Define new symbol. If elf is NULL, it's core API function.
 */
static int define_symbol(elf_module_t *elf, char *name, void *addr)
{
	symbol_t *sym = symtab;
	module_t *module = modtab; //= elf ? elf_module_get_data(elf) : &core_module;
	if (!elf)
	{
		//
		// VPRINTF("define core module symbol %s\n",name);
		// module = &core_module;
	}
	else
	{
		// VPRINTF("define module symbol %s\n",name);
		elf_module_get_data(elf);
		sprintf(modtab->name, "%s", name);
	}
	// VPRINTF("define symbol %s in %s.\n", name, module->name);

	while (sym)
	{
		if (!strcmp(sym->name, name))
		{
			//VPRINTF("Duplicate definition of `%s'.", name);
			snprintf(sym->name, 2, "%s", "+");
		}
		sym = sym->next;
	}
	// done:
	if (!sym)
	{
		size_t sym_mem_size = sizeof(*sym) + strlen(name) + 1;
		sym = elf_malloc(sym_mem_size);
	}
	strcpy(sym->name, name);
	sym->addr = addr;
	sym->module = module;
	sym->next = symtab;
	symtab = sym;

	return 0;
}

static symbol_t *lookup_symbol(char *name)
{
	symbol_t *sym = symtab;

	while (sym)
	{
		// printf("lookup '%s'::'%s'\n",sym->name,name);
		if (!strcmp(sym->name, name))
			return sym;
		sym = sym->next;
	}
	return NULL;
}

void elf_dump()
{
	symbol_t *sym = symtab;
	//VPRINTF("Symbols:\n");
	while (sym)
	{
	//	VPRINTF("%s::%08X\n", sym->name, (uint32_t)sym->addr);
		sym = sym->next;
	}
//	VPRINTF("EndSymbols:\n");
	module_t *mod = modtab;
//	VPRINTF("Modules:\n");
	while (mod)
	{
	//	VPRINTF("%s::%08X\n", mod->name, (uint32_t)mod->next);
		mod = mod->next;
	}
	//VPRINTF("EndModules:\n");
}
void free_elf_symbols()
{
	symbol_t *sym = symtab;
	while (sym)
	{
		symbol_t *d_sym = sym;
		sym = sym->next;
		free(d_sym);
	}
	symtab = NULL;
}
static module_t *lookup_module(char *name)
{
	module_t *mod = modtab;

	while (mod)
	{
		if (!strcmp(mod->name, name))
			return mod;
		mod = mod->next;
	}
	return NULL;
}

/*
 * Resolve symbol callback.
 *
 * If symbol is external(starts with 'MODULENAME__'), calls
 * load_module, if needed.
 */
static void *resolve_symbol(elf_module_t *elf, char *name)
{
	char *ptr;
	symbol_t *sym;
	sym = lookup_symbol(name);
	if (sym)
	{
		return sym->addr;
	}
	else
	{
		printf("Undefined symbol `%s'.", name);
		return NULL;
	}
}

static void *allocate(size_t size)
{
	return elf_malloc(size);
}
void *core = NULL;
static void load_module(char *module)
{
	int err;
	size_t size;

	char fullname[128];
	elf_module_t elf;
	size_t m_size = sizeof(module_t);
	// VPRINTF("alloc %u bytes for module %s\n",m_size,module);
	module_t *mod = elf_malloc(m_size);
	elf_module_link_cbs_t link =
		{
			.define = define_symbol,
			.resolve = resolve_symbol,
		};
	void *data;

	// VPRINTF("Loading module %s\n", module);
	// mod->name = module;
	mod->name = elf_malloc(strlen(module) + 1);
	sprintf(mod->name, "%s", module);
	sprintf(fullname, "%s", module);

	data = load_file(fullname, &size);
	if (!data)
	{
		printf("Can not load `%s'\n", module);
	}

	err = elf_module_init(&elf, data, size);
	if (err < 0)
	{
		printf("Not an executable.");
		return;
	}

	elf_module_set_data(&elf, mod);

	core = allocate(size);
	if (!core)
	{
		printf("Not enough memory.");
		return;
	}

	err = elf_module_load(&elf, core, &link);
	if (err < 0)
	{
		printf("Error loading module.");
		return;
	}
	mod->next = modtab;
	modtab = mod;
	loadfailed = false;
}
int elf_quickload(const char *filename)
{
	verbose = 0;
	init_core_module();
	FILE *fp = path_fopen(filename, "r");
	if (!fp)
	{
		return 1;
	}
	fclose(fp);
	int (*module_main)(void) = NULL;
	if (!symtab)
	{
		// LIBC
		define_symbol(NULL, "puts", puts);
		define_symbol(NULL, "getchar", getchar);
		define_symbol(NULL, "printf", printf);
		define_symbol(NULL, "putchar", putchar);
		define_symbol(NULL, "malloc", malloc);
		define_symbol(NULL, "free", free);
		define_symbol(NULL, "fopen", fopen);
		define_symbol(NULL, "fclose", fclose);
		define_symbol(NULL, "fread", fread);
		define_symbol(NULL, "fwrite", fwrite);
		define_symbol(NULL, "ftell", ftell);
		define_symbol(NULL, "fseek", fseek);
		define_symbol(NULL, "fprintf", fprintf);
		define_symbol(NULL, "fputs", fputs);
		define_symbol(NULL, "fputc", fputc);
		define_symbol(NULL, "fgetc", fgetc);
		define_symbol(NULL, "memcpy", memcpy);
		define_symbol(NULL, "memcmp", memcmp);
		define_symbol(NULL, "strcmp", strcmp);
		define_symbol(NULL, "strcat", strcat);
		define_symbol(NULL, "snprintf", snprintf);
		define_symbol(NULL, "path_fopen", path_fopen);
		define_symbol(NULL, "kbhit", kbhit);
		define_symbol(NULL, "srand", srand);
		define_symbol(NULL, "rand", rand);
		define_symbol(NULL, "randf", randf);

		// KERNEL
		define_symbol(NULL, "cls", cls);
		define_symbol(NULL, "gotoxy", gotoxy);
		//define_symbol(NULL, "kmesg", kmesg);
		//define_symbol(NULL, "read_number", read_number);
		//define_symbol(NULL, "read_hexnumber", read_hexnumber);
		//define_symbol(NULL, "read_label", read_label);
	}

	load_module((char *)filename);
	module_main = resolve_symbol(NULL, (char *)"emain");
	if (module_main)
	{
		module_main();
	}
	elf_dump();
	return 0;
}
