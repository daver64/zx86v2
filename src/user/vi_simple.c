#include "common.h"
#include "graphics.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "syscall.h"
#include "vfs.h"

#define MAX_LINE_LENGTH 256
#define MAX_LINES 16384

// ===== FASM SYNTAX HIGHLIGHTING =====

typedef enum {
    TOKEN_NORMAL = 0,
    TOKEN_INSTRUCTION,    // mov, add, jmp, etc.
    TOKEN_REGISTER,       // eax, ebx, esp, etc.
    TOKEN_DIRECTIVE,      // format, section, use32, etc.
    TOKEN_NUMBER,         // 0x1234, 123, 0b1010
    TOKEN_STRING,         // "hello", 'a'
    TOKEN_COMMENT,        // ; comment
    TOKEN_LABEL,          // start:, .loop:
    TOKEN_SYMBOL,         // constants, variables
    // Markdown tokens
    TOKEN_MD_HEADER,      // # ## ### etc.
    TOKEN_MD_BOLD,        // **bold**
    TOKEN_MD_ITALIC,      // *italic*
    TOKEN_MD_CODE,        // `code` or ```code```
    TOKEN_MD_LINK,        // [text](url)
    TOKEN_MD_LIST         // - * +
} syntax_token_type_t;

// Color definitions for FASM syntax
#define COLOR_INSTRUCTION   VGA_WHITE
#define COLOR_REGISTER      VGA_LIGHTGREEN
#define COLOR_DIRECTIVE     VGA_YELLOW
#define COLOR_NUMBER        VGA_LIGHTRED
#define COLOR_STRING        VGA_LIGHTCYAN
#define COLOR_COMMENT       VGA_DARKGREY
#define COLOR_LABEL         VGA_LIGHTMAGENTA
#define COLOR_SYMBOL        VGA_LIGHTBLUE
#define COLOR_NORMAL        VGA_LIGHTGREY

// Color definitions for Markdown syntax
#define COLOR_MD_HEADER     VGA_YELLOW
#define COLOR_MD_BOLD       VGA_WHITE
#define COLOR_MD_ITALIC     VGA_LIGHTCYAN
#define COLOR_MD_CODE       VGA_LIGHTGREEN
#define COLOR_MD_LINK       VGA_LIGHTBLUE
#define COLOR_MD_LIST       VGA_LIGHTMAGENTA

// FASM instruction set
static const char *fasm_instructions[] = {
    // Data movement
    "mov", "movzx", "movsx", "xchg", "lea", "cmp", "test",
    // Arithmetic
    "add", "sub", "mul", "div", "imul", "idiv", "inc", "dec", "neg", "adc", "sbb",
    // Logical
    "and", "or", "xor", "not", "shl", "shr", "sal", "sar", "rol", "ror", "rcl", "rcr",
    // Control flow
    "jmp", "call", "ret", "retf", "iret", "int", "into", "bound",
    "je", "jne", "jz", "jnz", "jl", "jg", "jle", "jge", "ja", "jb", "jae", "jbe",
    "jc", "jnc", "jo", "jno", "js", "jns", "jp", "jnp", "jpe", "jpo",
    "jecxz", "jcxz", "loop", "loope", "loopne", "loopz", "loopnz",
    // Stack operations
    "push", "pop", "pushf", "popf", "pushad", "popad", "pushfd", "popfd",
    // String operations
    "movs", "movsb", "movsw", "movsd", "stos", "stosb", "stosw", "stosd",
    "lods", "lodsb", "lodsw", "lodsd", "scas", "scasb", "scasw", "scasd",
    "cmps", "cmpsb", "cmpsw", "cmpsd", "rep", "repe", "repz", "repne", "repnz",
    // I/O
    "in", "out", "ins", "outs",
    // Processor control
    "nop", "hlt", "wait", "lock", "cld", "std", "cli", "sti", "clc", "stc", "cmc",
    NULL
};

// FASM directives
static const char *fasm_directives[] = {
    "format", "use16", "use32", "use64", "section", "segment",
    "org", "entry", "stack", "heap", "data", "code", "resource",
    "export", "import", "include", "load", "store",
    "macro", "endm", "struc", "struct", "ends", "union", "common",
    "db", "dw", "dd", "dq", "dt", "dp", "df",
    "rb", "rw", "rd", "rq", "rt", "rp", "rf",
    "file", "virtual", "repeat", "times", "if", "else", "end",
    "while", "break", "display", "err", "fix", "restore",
    "align", "public", "extrn", "extern", "global", "local",
    // Section attributes and formats
    "readable", "writable", "writeable", "executable", "shared", "discardable",
    "notpageable", "noexecute", "nocache", "mergeable", "linkinfo",
    "linkremove", "comdat", "gprel", "mem_fardata", "mem_purgeable",
    "mem_16bit", "mem_locked", "mem_preload", "type", "nobase",
    "elf", "pe", "coff", "ms", "mz", "binary",
    NULL
};

// x86 registers
static const char *x86_registers[] = {
    // 32-bit general purpose
    "eax", "ebx", "ecx", "edx", "esi", "edi", "esp", "ebp",
    // 16-bit general purpose  
    "ax", "bx", "cx", "dx", "si", "di", "sp", "bp",
    // 8-bit general purpose
    "al", "ah", "bl", "bh", "cl", "ch", "dl", "dh",
    // Segment registers
    "cs", "ds", "es", "fs", "gs", "ss",
    // Control registers
    "cr0", "cr1", "cr2", "cr3", "cr4", "cr8",
    // Debug registers
    "dr0", "dr1", "dr2", "dr3", "dr6", "dr7",
    // Test registers
    "tr3", "tr4", "tr5", "tr6", "tr7",
    // MMX/SSE registers
    "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7",
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
    NULL
};

static int syntax_highlighting_enabled = 1;

// File type detection
static int is_asm_file(const char *filename) {
    if (!filename) return 0;
    
    int len = strlen(filename);
    if (len < 4) return 0;
    
    // Check for .asm or .inc extensions
    if (strcmp(filename + len - 4, ".asm") == 0 ||
        strcmp(filename + len - 4, ".inc") == 0) {
        return 1;
    }
    
    return 0;
}

static int is_md_file(const char *filename) {
    if (!filename) return 0;
    
    int len = strlen(filename);
    if (len < 3) return 0;
    
    // Check for .md extension
    if (strcmp(filename + len - 3, ".md") == 0) {
        return 1;
    }
    
    return 0;
}

// Check if a string is a number (hex, binary, or decimal)
static int is_number(const char *str, int len) {
    if (!str || len <= 0) return 0;
    
    char token[64];
    if (len >= sizeof(token)) len = sizeof(token) - 1;
    strncpy(token, str, len);
    token[len] = '\0';
    
    // Hex number (0x1234, 1234h, or $1234)
    if (len >= 3 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
        return 1;
    }
    if (len > 1 && (token[len-1] == 'h' || token[len-1] == 'H')) {
        return 1;
    }
    if (len >= 2 && token[0] == '$') {
        // Check if rest are hex digits
        for (int i = 1; i < len; i++) {
            char c = token[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                return 0;
            }
        }
        return 1;
    }
    
    // Binary number (0b1010 or 1010b)  
    if (len >= 3 && token[0] == '0' && (token[1] == 'b' || token[1] == 'B')) {
        return 1;
    }
    if (len > 1 && (token[len-1] == 'b' || token[len-1] == 'B')) {
        return 1;
    }
    
    // Decimal number
    for (int i = 0; i < len; i++) {
        if (token[i] < '0' || token[i] > '9') {
            return 0;
        }
    }
    return len > 0;
}

// Get token type for syntax highlighting
static syntax_token_type_t get_token_type(const char *word, int len) {
    if (!word || len <= 0) return TOKEN_NORMAL;
    
    char token[64];
    if (len >= sizeof(token)) len = sizeof(token) - 1;
    strncpy(token, word, len);
    token[len] = '\0';
    
    // Convert to lowercase for comparison
    for (int i = 0; token[i]; i++) {
        if (token[i] >= 'A' && token[i] <= 'Z') {
            token[i] += 32;
        }
    }
    
    // Check if it's an instruction
    for (int i = 0; fasm_instructions[i]; i++) {
        if (strcmp(token, fasm_instructions[i]) == 0) {
            return TOKEN_INSTRUCTION;
        }
    }
    
    // Check if it's a register
    for (int i = 0; x86_registers[i]; i++) {
        if (strcmp(token, x86_registers[i]) == 0) {
            return TOKEN_REGISTER;
        }
    }
    
    // Check if it's a directive
    for (int i = 0; fasm_directives[i]; i++) {
        if (strcmp(token, fasm_directives[i]) == 0) {
            return TOKEN_DIRECTIVE;
        }
    }
    
    // Check if it's a number
    if (is_number(word, len)) {
        return TOKEN_NUMBER;
    }
    
    // Check if it ends with ':' (label)
    if (len > 1 && word[len-1] == ':') {
        return TOKEN_LABEL;
    }
    
    return TOKEN_NORMAL;
}

// Get token type for Markdown syntax highlighting
static syntax_token_type_t get_md_token_type(const char *line, int pos, int *token_len) {
    if (!line || pos < 0) return TOKEN_NORMAL;
    
    int line_len = strlen(line);
    if (pos >= line_len) return TOKEN_NORMAL;
    
    char ch = line[pos];
    *token_len = 1; // Default token length
    
    // Headers: # ## ### etc. (must be at start of line or after whitespace)
    if (ch == '#' && (pos == 0 || line[pos-1] == ' ' || line[pos-1] == '\t')) {
        int count = 0;
        while (pos + count < line_len && line[pos + count] == '#') {
            count++;
        }
        if (count <= 6) { // Valid header levels
            *token_len = count;
            return TOKEN_MD_HEADER;
        }
    }
    
    // Code blocks: ```
    if (pos <= line_len - 3 && strncmp(line + pos, "```", 3) == 0) {
        *token_len = 3;
        return TOKEN_MD_CODE;
    }
    
    // Inline code: `
    if (ch == '`') {
        return TOKEN_MD_CODE;
    }
    
    // Bold: **text**
    if (pos <= line_len - 2 && strncmp(line + pos, "**", 2) == 0) {
        *token_len = 2;
        return TOKEN_MD_BOLD;
    }
    
    // Italic: *text*
    if (ch == '*' && pos + 1 < line_len && line[pos + 1] != '*') {
        return TOKEN_MD_ITALIC;
    }
    
    // Links: [text](url)
    if (ch == '[') {
        return TOKEN_MD_LINK;
    }
    if (ch == ']' && pos + 1 < line_len && line[pos + 1] == '(') {
        return TOKEN_MD_LINK;
    }
    if (ch == '(' && pos > 0 && line[pos - 1] == ']') {
        return TOKEN_MD_LINK;
    }
    if (ch == ')') {
        // Check if this closes a link
        int bracket_pos = -1;
        for (int i = pos - 1; i >= 0; i--) {
            if (line[i] == '(') {
                bracket_pos = i;
                break;
            }
        }
        if (bracket_pos > 0 && line[bracket_pos - 1] == ']') {
            return TOKEN_MD_LINK;
        }
    }
    
    // Lists: - * + (must be at start of line or after whitespace)
    if ((ch == '-' || ch == '*' || ch == '+') && 
        (pos == 0 || line[pos-1] == ' ' || line[pos-1] == '\t') &&
        pos + 1 < line_len && line[pos + 1] == ' ') {
        return TOKEN_MD_LIST;
    }
    
    return TOKEN_NORMAL;
}

// Set color for syntax highlighting
static void set_syntax_color(syntax_token_type_t token_type) {
    switch (token_type) {
        case TOKEN_INSTRUCTION: 
            set_foreground_colour(COLOR_INSTRUCTION); 
            break;
        case TOKEN_REGISTER:    
            set_foreground_colour(COLOR_REGISTER); 
            break;
        case TOKEN_DIRECTIVE:   
            set_foreground_colour(COLOR_DIRECTIVE); 
            break;
        case TOKEN_NUMBER:      
            set_foreground_colour(COLOR_NUMBER); 
            break;
        case TOKEN_STRING:      
            set_foreground_colour(COLOR_STRING); 
            break;
        case TOKEN_COMMENT:     
            set_foreground_colour(COLOR_COMMENT); 
            break;
        case TOKEN_LABEL:       
            set_foreground_colour(COLOR_LABEL); 
            break;
        case TOKEN_SYMBOL:      
            set_foreground_colour(COLOR_SYMBOL); 
            break;
        case TOKEN_MD_HEADER:
            set_foreground_colour(COLOR_MD_HEADER);
            break;
        case TOKEN_MD_BOLD:
            set_foreground_colour(COLOR_MD_BOLD);
            break;
        case TOKEN_MD_ITALIC:
            set_foreground_colour(COLOR_MD_ITALIC);
            break;
        case TOKEN_MD_CODE:
            set_foreground_colour(COLOR_MD_CODE);
            break;
        case TOKEN_MD_LINK:
            set_foreground_colour(COLOR_MD_LINK);
            break;
        case TOKEN_MD_LIST:
            set_foreground_colour(COLOR_MD_LIST);
            break;
        default:                
            set_foreground_colour(COLOR_NORMAL); 
            break;
    }
}

// Check if character is a word separator
static int is_word_separator(char c) {
    return (c == ' ' || c == '\t' || c == ',' || c == ';' || 
            c == '"' || c == '\'' || c == '[' || c == ']' ||
            c == '(' || c == ')' || c == '+' || c == '-' ||
            c == '*' || c == '/' || c == ':' || c == '\0');
}

typedef enum {
    MODE_NORMAL = 0,
    MODE_INSERT = 1,
    MODE_COMMAND = 2
} vi_mode_t;

typedef struct {
    char lines[MAX_LINES][MAX_LINE_LENGTH];
    int line_count;
    int cursor_x;
    int cursor_y;
    int scroll_y;        // Top line being displayed
    int scroll_x;        // Left column being displayed
    vi_mode_t mode;
    char* filename;
    bool dirty;
    bool quit;
    bool file_ends_with_newline; // Track if original file ended with newline
    char status[256];
    char command[256];
    int command_len;
} simple_editor_t;

static simple_editor_t ed;

// Function declarations
int update_scroll(void);
void update_cursor_position(void);

// Draw a line with FASM syntax highlighting
static void draw_line_with_syntax(const char *line, int line_y) {
    if (!line) return;
    
    int len = strlen(line);
    int pos = 0;
    
    // Parse and render with syntax highlighting
    while (pos < len) {
        char ch = line[pos];
        
        // Handle comments (everything after ';')
        if (ch == ';') {
            set_syntax_color(TOKEN_COMMENT);
            // Print remaining characters as comment
            while (pos < len) {
                os_putc(line[pos]);
                pos++;
            }
            break;
        }
        
        // Handle strings
        if (ch == '"' || ch == '\'') {
            char quote = ch;
            set_syntax_color(TOKEN_STRING);
            os_putc(ch);
            pos++;
            
            while (pos < len && line[pos] != quote) {
                os_putc(line[pos]);
                pos++;
            }
            
            if (pos < len && line[pos] == quote) {
                os_putc(line[pos]);
                pos++;
            }
            continue;
        }
        
        // Handle whitespace and punctuation
        if (is_word_separator(ch)) {
            set_foreground_colour(COLOR_NORMAL);
            os_putc(ch);
            pos++;
            continue;
        }
        
        // Handle words (instructions, registers, etc.)
        int word_start = pos;
        while (pos < len && !is_word_separator(line[pos])) {
            pos++;
        }
        
        int word_len = pos - word_start;
        if (word_len > 0) {
            syntax_token_type_t token_type = get_token_type(line + word_start, word_len);
            set_syntax_color(token_type);
            
            // Print the word character by character
            for (int i = 0; i < word_len; i++) {
                os_putc(line[word_start + i]);
            }
        }
    }
    
    // Reset color at end
    set_foreground_colour(COLOR_NORMAL);
}

static void draw_line_with_md_syntax(const char *line, int line_y) {
    if (!line) return;
    
    int len = strlen(line);
    int pos = 0;
    
    // Parse and render with Markdown syntax highlighting
    while (pos < len) {
        int token_len = 1;
        syntax_token_type_t token_type = get_md_token_type(line, pos, &token_len);
        
        if (token_type != TOKEN_NORMAL) {
            set_syntax_color(token_type);
            
            // Print the token
            for (int i = 0; i < token_len && pos + i < len; i++) {
                os_putc(line[pos + i]);
            }
            pos += token_len;
            
            // For certain tokens, continue with the same color until delimiter
            if (token_type == TOKEN_MD_BOLD && token_len == 2) {
                // Continue until closing **
                while (pos < len - 1) {
                    if (line[pos] == '*' && line[pos + 1] == '*') {
                        os_putc('*');
                        os_putc('*');
                        pos += 2;
                        break;
                    }
                    os_putc(line[pos]);
                    pos++;
                }
            } else if (token_type == TOKEN_MD_ITALIC && line[pos - 1] == '*') {
                // Continue until closing *
                while (pos < len) {
                    if (line[pos] == '*') {
                        os_putc('*');
                        pos++;
                        break;
                    }
                    os_putc(line[pos]);
                    pos++;
                }
            } else if (token_type == TOKEN_MD_CODE && line[pos - 1] == '`') {
                // Continue until closing `
                while (pos < len) {
                    if (line[pos] == '`') {
                        os_putc('`');
                        pos++;
                        break;
                    }
                    os_putc(line[pos]);
                    pos++;
                }
            } else if (token_type == TOKEN_MD_HEADER) {
                // Continue for rest of line with header color
                while (pos < len) {
                    os_putc(line[pos]);
                    pos++;
                }
                break;
            }
        } else {
            // Normal character
            set_foreground_colour(COLOR_NORMAL);
            os_putc(line[pos]);
            pos++;
        }
    }
    
    // Reset color at end
    set_foreground_colour(COLOR_NORMAL);
}

void show_status(const char* msg) {
    strcpy(ed.status, msg);
    
    // Show status at bottom
    int rows = get_num_rows();
    gotoxy(0, rows - 1);
    set_background_colour(VGA_BLUE);
    set_foreground_colour(VGA_WHITE);
    
    char mode_str[16];
    switch (ed.mode) {
        case MODE_NORMAL: strcpy(mode_str, "NORMAL"); break;
        case MODE_INSERT: strcpy(mode_str, "INSERT"); break;
        case MODE_COMMAND: strcpy(mode_str, "COMMAND"); break;
    }
    
    printf(" %s | %s | %s", mode_str, ed.filename ? ed.filename : "[No file]", msg);
    
    // Clear rest of line
    for (int i = strlen(mode_str) + strlen(msg) + 20; i < get_num_columns(); i++) {
        printf(" ");
    }
    
    set_background_colour(VGA_BLACK);
    set_foreground_colour(VGA_LIGHTGREY);
    
    // Return cursor to edit area
    gotoxy(ed.cursor_x, ed.cursor_y);
    terminal_flush();
}

void update_cursor_position(void) {
    int screen_cols = get_num_columns();
    int line_num_width = 6;
    int text_cols = screen_cols - line_num_width;
    
    if (text_cols <= 0) text_cols = 74;
    
    // Calculate the screen position for the cursor
    int screen_row = 0;
    for (int line_idx = ed.scroll_y; line_idx < ed.cursor_y && line_idx < ed.line_count; line_idx++) {
        int line_len = strlen(ed.lines[line_idx]);
        if (line_len == 0) {
            screen_row++;
        } else {
            screen_row += (line_len + text_cols - 1) / text_cols; // Ceiling division
        }
    }
    
    // Add the position within the current line
    int line_len = strlen(ed.lines[ed.cursor_y]);
    if (line_len > 0) {
        screen_row += ed.cursor_x / text_cols;
    }
    
    int screen_col = line_num_width + (ed.cursor_x % text_cols);
    
    gotoxy(screen_col, screen_row);
    terminal_flush();
}

void redraw_screen(void) {
    cls();
    
    int screen_rows = get_num_rows() - 2; // Reserve space for status line
    int screen_cols = get_num_columns();
    
    // Reserve space for line numbers (6 chars: "999: ")
    int line_num_width = 6;
    int text_cols = screen_cols - line_num_width;
    
    // Force reasonable defaults if screen dimensions are invalid
    if (screen_cols <= 0 || screen_cols > 200) {
        screen_cols = 80;
        text_cols = 74;
    }
    if (screen_rows <= 0 || screen_rows > 50) screen_rows = 23;
    
    int current_row = 0;
    
    // Draw lines starting from scroll_y with manual wrapping
    for (int line_idx = ed.scroll_y; line_idx < ed.line_count && current_row < screen_rows; line_idx++) {
        char* line = ed.lines[line_idx];
        int line_len = strlen(line);
        
        if (line_len == 0) {
            // Empty line
            gotoxy(0, current_row);
            // Set colors for line number: dark grey background, light green text
            set_background_colour(VGA_DARKGREY);
            set_foreground_colour(VGA_LIGHTGREEN);
            printf("%4d: ", line_idx + 1);
            // Reset to normal colors
            set_background_colour(VGA_BLACK);
            set_foreground_colour(VGA_LIGHTGREY);
            current_row++;
            continue;
        }
        
        // Manually wrap long lines at text_cols width
        int pos = 0;
        bool first_segment = true;
        
        while (pos < line_len && current_row < screen_rows) {
            gotoxy(0, current_row);
            
            if (first_segment) {
                // Show line number on first segment with colors
                set_background_colour(VGA_DARKGREY);
                set_foreground_colour(VGA_LIGHTGREEN);
                printf("%4d: ", line_idx + 1);
                set_background_colour(VGA_BLACK);
                first_segment = false;
            } else {
                // Indent continuation lines with colors
                set_background_colour(VGA_DARKGREY);
                set_foreground_colour(VGA_LIGHTGREEN);
                printf("    : ");
                set_background_colour(VGA_BLACK);
            }
            
            // Calculate how many characters fit on this row
            int chars_to_show = text_cols;
            if (pos + chars_to_show > line_len) {
                chars_to_show = line_len - pos;
            }
            
            // Create substring for this segment
            char segment[MAX_LINE_LENGTH];
            strncpy(segment, line + pos, chars_to_show);
            segment[chars_to_show] = '\0';
            
            // Apply syntax highlighting if enabled
            if (syntax_highlighting_enabled && ed.filename && 
                (is_asm_file(ed.filename) || is_md_file(ed.filename))) {
                
                if (is_asm_file(ed.filename)) {
                    draw_line_with_syntax(segment, current_row);
                } else if (is_md_file(ed.filename)) {
                    draw_line_with_md_syntax(segment, current_row);
                }
            } else {
                // No syntax highlighting - just print normally
                set_foreground_colour(VGA_LIGHTGREY);
                printf("%s", segment);
            }
            
            pos += chars_to_show;
            current_row++;
        }
    }
    
    show_status(ed.status);
    
    // Calculate cursor position with line numbers and wrapping
    int cursor_screen_row = 0;
    
    // Count rows used by lines before cursor line
    for (int line_idx = ed.scroll_y; line_idx < ed.cursor_y && line_idx < ed.line_count; line_idx++) {
        int line_len = strlen(ed.lines[line_idx]);
        if (line_len == 0) {
            cursor_screen_row++;
        } else {
            cursor_screen_row += (line_len + text_cols - 1) / text_cols; // Ceiling division
        }
    }
    
    // Add offset within current line
    if (ed.cursor_y >= ed.scroll_y && ed.cursor_y < ed.line_count) {
        cursor_screen_row += ed.cursor_x / text_cols;
    }
    
    int cursor_screen_col = line_num_width + (ed.cursor_x % text_cols);
    
    // Bounds check
    if (cursor_screen_row >= screen_rows) cursor_screen_row = screen_rows - 1;
    if (cursor_screen_row < 0) cursor_screen_row = 0;
    if (cursor_screen_col >= screen_cols) cursor_screen_col = screen_cols - 1;
    if (cursor_screen_col < line_num_width) cursor_screen_col = line_num_width;
    
    // Reset colors before positioning cursor
    set_background_colour(VGA_BLACK);
    set_foreground_colour(VGA_LIGHTGREY);
    
    gotoxy(cursor_screen_col, cursor_screen_row);
    terminal_flush();
}

int update_scroll(void) {
    int screen_rows = get_num_rows() - 2;
    int screen_cols = get_num_columns();
    int line_num_width = 6;
    int text_cols = screen_cols - line_num_width;
    int old_scroll_y = ed.scroll_y;
    
    if (screen_rows <= 0) screen_rows = 23;
    if (text_cols <= 0) text_cols = 74;
    
    // Calculate how many screen rows the cursor would need
    int rows_needed = 0;
    for (int line_idx = ed.scroll_y; line_idx <= ed.cursor_y && line_idx < ed.line_count; line_idx++) {
        int line_len = strlen(ed.lines[line_idx]);
        if (line_len == 0) {
            rows_needed++;
        } else {
            rows_needed += (line_len + text_cols - 1) / text_cols; // Ceiling division
        }
    }
    
    // If cursor is above visible area, scroll up
    if (ed.cursor_y < ed.scroll_y) {
        ed.scroll_y = ed.cursor_y;
        return 1; // Scrolling happened
    }
    
    // If cursor would be below visible area, scroll down
    while (rows_needed >= screen_rows && ed.scroll_y < ed.cursor_y) {
        // Remove the first line from our count
        int line_len = strlen(ed.lines[ed.scroll_y]);
        if (line_len == 0) {
            rows_needed--;
        } else {
            rows_needed -= (line_len + text_cols - 1) / text_cols;
        }
        ed.scroll_y++;
    }
    
    // Ensure scroll_y doesn't go negative or beyond file
    if (ed.scroll_y < 0) ed.scroll_y = 0;
    if (ed.scroll_y >= ed.line_count) ed.scroll_y = ed.line_count - 1;
    
    // No horizontal scrolling with line wrapping
    ed.scroll_x = 0;
    
    // Return 1 if scrolling happened, 0 if not
    return (ed.scroll_y != old_scroll_y);
}

void vi_main(const char* filename) {
    memset(&ed, 0, sizeof(ed));
    
    // Explicitly clear all line arrays to prevent garbage data
    for (int i = 0; i < MAX_LINES; i++) {
        memset(ed.lines[i], 0, MAX_LINE_LENGTH);
    }
    
    ed.mode = MODE_NORMAL;
    ed.line_count = 1;
    strcpy(ed.lines[0], "");
    ed.cursor_x = 0;
    ed.cursor_y = 0;
    
    if (filename) {
        ed.filename = malloc(strlen(filename) + 1);
        if (ed.filename) {
            memset(ed.filename, 0, strlen(filename) + 1);  // Clear the memory
            strcpy(ed.filename, filename);
        }
        
        // Try to load file
        FILE* fp = fopen(filename, "r");
        if (fp) {
            // Use fread to read entire file content
            // Read file in larger chunks to handle big files
            #define CHUNK_SIZE 8192
            char* file_buffer = malloc(CHUNK_SIZE);
            if (!file_buffer) {
                fclose(fp);
                show_status("Out of memory");
                return;
            }
            
            ed.line_count = 0;
            int pos = 0;
            memset(ed.lines[0], 0, MAX_LINE_LENGTH);
            
            unsigned int bytes_read;
            while ((bytes_read = fread(file_buffer, 1, CHUNK_SIZE, fp)) > 0 && ed.line_count < MAX_LINES) {
                for (unsigned int i = 0; i < bytes_read && ed.line_count < MAX_LINES; i++) {
                    char c = file_buffer[i];
                    
                    if (c == '\n') {
                        ed.lines[ed.line_count][pos] = '\0';
                        ed.line_count++;
                        pos = 0;
                        if (ed.line_count < MAX_LINES) {
                            memset(ed.lines[ed.line_count], 0, MAX_LINE_LENGTH);
                        }
                    } else if (c == '\r') {
                        // Skip carriage return - handle Windows line endings
                    } else if (pos < MAX_LINE_LENGTH - 1) {
                        ed.lines[ed.line_count][pos++] = c;
                    }
                }
            }
            
            free(file_buffer);
            
            // Handle final line if it doesn't end with newline
            if (pos > 0 && ed.line_count < MAX_LINES) {
                ed.lines[ed.line_count][pos] = '\0';
                ed.file_ends_with_newline = false;
                ed.line_count++;
            } else {
                ed.file_ends_with_newline = true;
            }
            
            if (ed.line_count == 0) {
                strcpy(ed.lines[0], "");
                ed.line_count = 1;
            }
            
            fclose(fp);
        } else {
            // For new files, default to ending with newline
            ed.file_ends_with_newline = true;
        }
    }
    
    // Initialize syntax highlighting (enable for ASM and MD files by default)
    syntax_highlighting_enabled = is_asm_file(filename) || is_md_file(filename);
    
    show_status("ZX86Vi - i=insert, dd=delete line, /text=search, :d,:q,:w");
    redraw_screen();
    
    while (!ed.quit) {
        int key = getchar();
        
        if (ed.mode == MODE_NORMAL) {
            switch (key) {
                case 'i':
                    ed.mode = MODE_INSERT;
                    show_status("-- INSERT --");
                    break;
                case 'a':
                    // Move cursor to the right (append after current character)
                    if (ed.cursor_x < strlen(ed.lines[ed.cursor_y])) {
                        ed.cursor_x++;
                    }
                    ed.mode = MODE_INSERT;
                    show_status("-- INSERT --");
                    update_cursor_position();
                    break;
                case 'h':
                    if (ed.cursor_x > 0) {
                        ed.cursor_x--;
                        if (update_scroll()) {
                            redraw_screen();
                        } else {
                            update_cursor_position();
                        }
                    }
                    break;
                case 'l':
                    if (ed.cursor_x < strlen(ed.lines[ed.cursor_y])) {
                        ed.cursor_x++;
                        if (update_scroll()) {
                            redraw_screen();
                        } else {
                            update_cursor_position();
                        }
                    }
                    break;
                case 'j':
                    if (ed.cursor_y < ed.line_count - 1) {
                        ed.cursor_y++;
                        if (ed.cursor_x > strlen(ed.lines[ed.cursor_y])) {
                            ed.cursor_x = strlen(ed.lines[ed.cursor_y]);
                        }
                        if (update_scroll()) {
                            redraw_screen();
                        } else {
                            update_cursor_position();
                        }
                    }
                    break;
                case 'k':
                    if (ed.cursor_y > 0) {
                        ed.cursor_y--;
                        if (ed.cursor_x > strlen(ed.lines[ed.cursor_y])) {
                            ed.cursor_x = strlen(ed.lines[ed.cursor_y]);
                        }
                        if (update_scroll()) {
                            redraw_screen();
                        } else {
                            update_cursor_position();
                        }
                    }
                    break;
                case ':':
                    ed.mode = MODE_COMMAND;
                    ed.command_len = 0;
                    ed.command[0] = '\0';
                    
                    gotoxy(0, get_num_rows() - 1);
                    set_background_colour(VGA_BLACK);
                    set_foreground_colour(VGA_WHITE);
                    printf(":");
                    terminal_flush();
                    break;
                case 's':
                    // Toggle syntax highlighting
                    syntax_highlighting_enabled = !syntax_highlighting_enabled;
                    char syntax_msg[100];
                    sprintf(syntax_msg, "Syntax highlighting %s", 
                            syntax_highlighting_enabled ? "enabled" : "disabled");
                    show_status(syntax_msg);
                    redraw_screen();
                    break;
                case 'd':
                    // Wait for second 'd' to delete line
                    show_status("Press 'd' again to delete line");
                    terminal_flush();
                    int second_key = getchar();
                    if (second_key == 'd') {
                        // Delete current line (same logic as :d command)
                        if (ed.line_count > 1) {
                            // Shift all lines up to delete current line
                            for (int i = ed.cursor_y; i < ed.line_count - 1; i++) {
                                strcpy(ed.lines[i], ed.lines[i + 1]);
                            }
                            ed.line_count--;
                            
                            // Adjust cursor position
                            if (ed.cursor_y >= ed.line_count) {
                                ed.cursor_y = ed.line_count - 1;
                            }
                            if (ed.cursor_x > strlen(ed.lines[ed.cursor_y])) {
                                ed.cursor_x = strlen(ed.lines[ed.cursor_y]);
                            }
                            
                            ed.dirty = true;
                            show_status("Line deleted");
                        } else {
                            // Last line - just clear it
                            strcpy(ed.lines[0], "");
                            ed.cursor_x = 0;
                            ed.dirty = true;
                            show_status("Line cleared");
                        }
                        update_scroll();
                        redraw_screen();
                    } else {
                        show_status("Normal mode");
                    }
                    break;
            }
        } else if (ed.mode == MODE_INSERT) {
            if (key == 27) { // ESC
                ed.mode = MODE_NORMAL;
                show_status("Normal mode");
            } else if (key >= 32 && key < 127) {
                // Insert character
                char* line = ed.lines[ed.cursor_y];
                int len = strlen(line);
                
                if (len < MAX_LINE_LENGTH - 1) {
                    // Shift characters right
                    for (int i = len; i >= ed.cursor_x; i--) {
                        line[i + 1] = line[i];
                    }
                    line[ed.cursor_x] = key;
                    ed.cursor_x++;
                    ed.dirty = true;
                    
                    update_scroll();
                    redraw_screen();
                    terminal_flush();
                }
            } else if (key == 8 || key == 127) { // Backspace
                if (ed.cursor_x > 0) {
                    char* line = ed.lines[ed.cursor_y];
                    int len = strlen(line);
                    
                    // Shift characters left
                    for (int i = ed.cursor_x - 1; i < len; i++) {
                        line[i] = line[i + 1];
                    }
                    ed.cursor_x--;
                    ed.dirty = true;
                    
                    update_scroll();
                    redraw_screen();
                    terminal_flush();
                }
            } else if (key == 13 || key == 10) { // Enter - support both CR and LF
                if (ed.line_count < MAX_LINES - 1) {
                    // Split line at cursor
                    char* line = ed.lines[ed.cursor_y];
                    
                    // Move lines down
                    for (int i = ed.line_count; i > ed.cursor_y + 1; i--) {
                        strcpy(ed.lines[i], ed.lines[i-1]);
                    }
                    
                    // Split current line
                    strcpy(ed.lines[ed.cursor_y + 1], line + ed.cursor_x);
                    line[ed.cursor_x] = '\0';
                    
                    ed.line_count++;
                    ed.cursor_y++;
                    ed.cursor_x = 0;
                    ed.dirty = true;
                    
                    redraw_screen();
                }
            }
        } else if (ed.mode == MODE_COMMAND) {
            if (key == 27) { // ESC
                ed.mode = MODE_NORMAL;
                show_status("Normal mode");
                redraw_screen();
            } else if (key == 13 || key == 10) { // Enter
                // Null terminate and trim the command
                ed.command[ed.command_len] = '\0';
                
                // Trim leading/trailing spaces
                char* cmd = ed.command;
                while (*cmd == ' ') cmd++; // Skip leading spaces
                
                int len = strlen(cmd);
                while (len > 0 && cmd[len-1] == ' ') {
                    cmd[len-1] = '\0';
                    len--;
                }
                
                if (strcmp(cmd, "q") == 0) {
                    if (ed.dirty) {
                        show_status("No write since last change (use :q! to override)");
                        ed.mode = MODE_NORMAL;
                        redraw_screen();
                    } else {
                        ed.quit = true;
                    }
                } else if (strcmp(cmd, "q!") == 0) {
                    ed.quit = true;
                } else if (strcmp(cmd, "d") == 0) {
                    // Delete current line
                    if (ed.line_count > 1) {
                        // Shift all lines up to delete current line
                        for (int i = ed.cursor_y; i < ed.line_count - 1; i++) {
                            strcpy(ed.lines[i], ed.lines[i + 1]);
                        }
                        ed.line_count--;
                        
                        // Adjust cursor position
                        if (ed.cursor_y >= ed.line_count) {
                            ed.cursor_y = ed.line_count - 1;
                        }
                        if (ed.cursor_x > strlen(ed.lines[ed.cursor_y])) {
                            ed.cursor_x = strlen(ed.lines[ed.cursor_y]);
                        }
                        
                        ed.dirty = true;
                        show_status("Line deleted");
                    } else {
                        // Last line - just clear it
                        strcpy(ed.lines[0], "");
                        ed.cursor_x = 0;
                        ed.dirty = true;
                        show_status("Line cleared");
                    }
                    ed.mode = MODE_NORMAL;
                    redraw_screen();
                } else if (len >= 1 && cmd[0] == '/') {
                    // Search forward for pattern
                    char* pattern = cmd + 1; // Skip the '/'
                    if (strlen(pattern) > 0) {
                        int found = 0;
                        int start_line = ed.cursor_y;
                        int start_col = ed.cursor_x + 1; // Start search after current position
                        
                        // Search from current position to end of file
                        for (int line = start_line; line < ed.line_count && !found; line++) {
                            char* line_text = ed.lines[line];
                            int search_start = (line == start_line) ? start_col : 0;
                            
                            char* match = strstr(line_text + search_start, pattern);
                            if (match) {
                                ed.cursor_y = line;
                                ed.cursor_x = match - line_text;
                                found = 1;
                                
                                char msg[100];
                                sprintf(msg, "Found at line %d, col %d", line + 1, ed.cursor_x + 1);
                                show_status(msg);
                            }
                        }
                        
                        // If not found, search from beginning to current position
                        if (!found) {
                            for (int line = 0; line <= start_line && !found; line++) {
                                char* line_text = ed.lines[line];
                                int search_end = (line == start_line) ? ed.cursor_x : strlen(line_text);
                                
                                // Create a temporary string for partial search
                                char temp_line[MAX_LINE_LENGTH];
                                strncpy(temp_line, line_text, search_end);
                                temp_line[search_end] = '\0';
                                
                                char* match = strstr(temp_line, pattern);
                                if (match) {
                                    ed.cursor_y = line;
                                    ed.cursor_x = match - temp_line;
                                    found = 1;
                                    
                                    char msg[100];
                                    sprintf(msg, "Found at line %d, col %d", line + 1, ed.cursor_x + 1);
                                    show_status(msg);
                                }
                            }
                        }
                        
                        if (!found) {
                            char msg[100];
                            sprintf(msg, "Pattern '%s' not found", pattern);
                            show_status(msg);
                        }
                    } else {
                        show_status("Usage: /pattern");
                    }
                    ed.mode = MODE_NORMAL;
                    redraw_screen();
                } else if (len >= 2 && cmd[0] == 'w' && cmd[1] == ' ') {
                    // Save to specific filename - manual character check
                    char debug_msg[256];
                    memset(debug_msg, 0, sizeof(debug_msg));
                    snprintf(debug_msg, sizeof(debug_msg), "VI: Matched 'w ' pattern manually\n");
                    serial_puts(debug_msg);
                    
                    char* new_filename = cmd + 2; // Skip "w "
                    if (strlen(new_filename) > 0) {
                        char msg[256];
                        memset(msg, 0, sizeof(msg));
                        snprintf(msg, sizeof(msg), "VI: Attempting to save to: %s\n", new_filename);
                        serial_puts(msg);
                        
                        FILE* fp = fopen(new_filename, "w");
                        if (fp) {
                            for (int i = 0; i < ed.line_count; i++) {
                                fprintf(fp, "%s", ed.lines[i]);
                                // Add newline except for last line if original didn't end with newline
                                if (i < ed.line_count - 1 || ed.file_ends_with_newline) {
                                    fprintf(fp, "\n");
                                }
                            }
                            fclose(fp);
                            serial_puts("VI: File save completed\n");
                            
                            // Update filename
                            if (ed.filename) free(ed.filename);
                            ed.filename = malloc(strlen(new_filename) + 1);
                            if (ed.filename) {
                                strcpy(ed.filename, new_filename);
                            }
                            
                            ed.dirty = false;
                            memset(msg, 0, sizeof(msg));
                            snprintf(msg, sizeof(msg), "VI: File saved as: %s\n", new_filename);
                            serial_puts(msg);
                            show_status("File saved");
                            ed.mode = MODE_NORMAL;
                            redraw_screen();
                        } else {
                            memset(msg, 0, sizeof(msg));
                            snprintf(msg, sizeof(msg), "VI: Error creating file: %s\n", new_filename);
                            serial_puts(msg);
                            show_status("Error saving file");
                            ed.mode = MODE_NORMAL;
                            redraw_screen();
                        }
                    } else {
                        show_status("No filename specified");
                        ed.mode = MODE_NORMAL;
                        redraw_screen();
                    }
                } else if (strcmp(cmd, "w") == 0) {
                    if (ed.filename) {
                        FILE* fp = fopen(ed.filename, "w");
                        if (fp) {
                            for (int i = 0; i < ed.line_count; i++) {
                                fprintf(fp, "%s", ed.lines[i]);
                                // Add newline except for last line if original didn't end with newline
                                if (i < ed.line_count - 1 || ed.file_ends_with_newline) {
                                    fprintf(fp, "\n");
                                }
                            }
                            fclose(fp);
                            ed.dirty = false;
                            show_status("File saved");
                            ed.mode = MODE_NORMAL;
                            redraw_screen();
                        } else {
                            show_status("Error saving file");
                            ed.mode = MODE_NORMAL;
                            redraw_screen();
                        }
                    } else {
                        show_status("No filename");
                        ed.mode = MODE_NORMAL;
                        redraw_screen();
                    }
                } else if (strcmp(cmd, "wq") == 0) {
                    if (ed.filename) {
                        FILE* fp = fopen(ed.filename, "w");
                        if (fp) {
                            for (int i = 0; i < ed.line_count; i++) {
                                fprintf(fp, "%s", ed.lines[i]);
                                // Add newline except for last line if original didn't end with newline
                                if (i < ed.line_count - 1 || ed.file_ends_with_newline) {
                                    fprintf(fp, "\n");
                                }
                            }
                            fclose(fp);
                            ed.quit = true;
                        } else {
                            show_status("Error saving file");
                            ed.mode = MODE_NORMAL;
                            redraw_screen();
                        }
                    } else {
                        show_status("No filename");
                        ed.mode = MODE_NORMAL;
                        redraw_screen();
                    }
                } else {
                    char msg[256];
                    memset(msg, 0, sizeof(msg));
                    snprintf(msg, sizeof(msg), "VI: Unknown command: '%s' (len=%d)\n", cmd, strlen(cmd));
                    serial_puts(msg);
                    show_status("Unknown command");
                    ed.mode = MODE_NORMAL;
                    redraw_screen();
                }
            } else if (key == 8 || key == 127) { // Backspace
                if (ed.command_len > 0) {
                    ed.command_len--;
                    
                    gotoxy(ed.command_len + 1, get_num_rows() - 1);
                    printf(" ");
                    gotoxy(ed.command_len + 1, get_num_rows() - 1);
                    terminal_flush();
                }
            } else if (key >= 32 && key < 127 && ed.command_len < sizeof(ed.command) - 1) {
                ed.command[ed.command_len] = key;
                ed.command_len++;
                
                printf("%c", key);
                terminal_flush();
            }
        }
    }
    
    // Clear screen when exiting
    cls();
    
    // Clean up allocated memory
    if (ed.filename) {
        free(ed.filename);
    }
}