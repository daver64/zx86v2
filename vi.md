# ZX86Vi - Simple Vi Text Editor

ZX86Vi is a simple vi-compatible text editor included with the ZX86v2 operating system. It provides basic text editing functionality with vi-style modal editing, syntax highlighting for assembly language, and essential file operations.

## Overview

ZX86Vi operates in three distinct modes, similar to traditional vi:
- **Normal Mode**: Navigation and commands
- **Insert Mode**: Text insertion and editing
- **Command Mode**: File operations and advanced commands

## Starting the Editor

### Launch Vi
```bash
vi                    # Create new file
vi filename.txt       # Open existing file or create new
vi program.asm        # Open assembly file (enables syntax highlighting)
```

### Automatic Features
- **Syntax highlighting** automatically enabled for `.asm` files
- **Status line** shows current mode and helpful commands
- **File detection** preserves original file formatting

## Mode Operations

### Normal Mode (Default)
This is the primary navigation and command mode. All key commands are executed immediately without pressing Enter.

#### Cursor Movement
| Key | Action |
|-----|--------|
| `h` | Move cursor left |
| `j` | Move cursor down |
| `k` | Move cursor up |
| `l` | Move cursor right |

#### Mode Switching
| Key | Action |
|-----|--------|
| `i` | Enter Insert mode at cursor position |
| `:` | Enter Command mode |
| `ESC` | Return to Normal mode (from any other mode) |

#### Line Operations
| Key | Action |
|-----|--------|
| `dd` | Delete current line (requires two 'd' keypresses) |

#### Special Features
| Key | Action |
|-----|--------|
| `s` | Toggle syntax highlighting on/off |

#### Search Operations
| Key | Action |
|-----|--------|
| `/pattern` | Search forward for text pattern |

### Insert Mode
In Insert mode, you can type text normally. All printable characters are inserted at the cursor position.

#### Text Input
| Key | Action |
|-----|--------|
| `Any character` | Insert character at cursor |
| `Enter` | Create new line and move cursor down |
| `Backspace` | Delete character to the left of cursor |
| `ESC` | Return to Normal mode |

#### Line Editing
- **Character insertion**: Text is inserted at cursor position, shifting existing text right
- **Line splitting**: Enter key splits current line at cursor position
- **Backspace**: Removes character to the left, shifting text left
- **Automatic wrapping**: Lines are limited to 255 characters

### Command Mode
Command mode allows file operations and editor commands. Commands are typed after the `:` prompt.

#### File Operations
| Command | Action |
|---------|--------|
| `:w` | Write (save) current file |
| `:w filename` | Write to specific filename |
| `:q` | Quit editor (only if no unsaved changes) |
| `:q!` | Force quit without saving changes |
| `:wq` | Write file and quit |

#### Line Operations
| Command | Action |
|---------|--------|
| `:d` | Delete current line |

#### Search Operations
| Command | Action |
|---------|--------|
| `:/pattern` | Search forward for text pattern |

#### Command Input
- **Typing**: Enter commands after the `:` prompt
- **Backspace**: Remove characters from command line
- **Enter**: Execute command
- **ESC**: Cancel command and return to Normal mode

## Syntax Highlighting

ZX86Vi includes comprehensive syntax highlighting for FASM (Flat Assembler) assembly language.

### Automatic Detection
- Syntax highlighting automatically enables for `.asm` files
- Can be manually toggled with `s` key in Normal mode

### Color Scheme
| Element | Color | Examples |
|---------|--------|----------|
| **Instructions** | White | `mov`, `add`, `jmp`, `call`, `ret` |
| **Registers** | Light Green | `eax`, `ebx`, `esp`, `edi` |
| **Directives** | Yellow | `format`, `section`, `use32` |
| **Numbers** | Light Red | `0x1234`, `123`, `0b1010` |
| **Strings** | Light Cyan | `"hello"`, `'a'` |
| **Comments** | Dark Grey | `; this is a comment` |
| **Labels** | Light Magenta | `start:`, `.loop:` |
| **Symbols** | Light Blue | Constants and variables |

### Supported Assembly Features
- **x86 instruction set**: Complete coverage of common instructions
- **Register names**: All x86 32-bit and 16-bit registers
- **Assembler directives**: FASM-specific directives
- **Number formats**: Hexadecimal, decimal, binary, and octal
- **String literals**: Both single and double quoted
- **Comments**: Semicolon-style comments
- **Labels**: Standard assembly labels

## Status Line

The status line at the bottom of the screen shows:
- **Current mode**: NORMAL, INSERT, or COMMAND
- **Quick help**: Key command reminders
- **Status messages**: File operations, search results, errors

### Status Messages
| Message | Meaning |
|---------|---------|
| `-- INSERT --` | Currently in Insert mode |
| `Normal mode` | Returned to Normal mode |
| `File saved` | File successfully written |
| `Line deleted` | Line deletion completed |
| `Found at line X, col Y` | Search result location |
| `Pattern 'text' not found` | Search failed |
| `No write since last change` | Unsaved changes prevent quit |

## File Operations

### Creating New Files
```bash
vi newfile.txt        # Creates empty buffer, saves on :w
```

### Opening Existing Files
```bash
vi existingfile.asm   # Loads file content into editor
```

### Saving Files
- `:w` - Save to current filename
- `:w newname.txt` - Save with new filename
- `:wq` - Save and quit

### File Safety
- Editor tracks unsaved changes ("dirty" flag)
- `:q` prevented if unsaved changes exist
- `:q!` forces quit without saving
- Status messages confirm save operations

## Search Functionality

### Forward Search
1. In Normal mode, type `/` followed by search pattern
2. Press Enter to execute search
3. Cursor moves to first occurrence
4. Search wraps around from end to beginning of file

### Search Features
- **Case sensitive** pattern matching
- **Wrap-around** search from current position
- **Position reporting** shows line and column of matches
- **Not found** messages when pattern doesn't exist

### Search Examples
```
/hello              # Find "hello"
/mov eax            # Find "mov eax" instruction
/0x                 # Find hexadecimal numbers
```

## Keyboard Shortcuts Summary

### Normal Mode Quick Reference
```
Movement:     h j k l        (left, down, up, right)
Insert:       i              (enter insert mode)
Delete:       dd             (delete line)
Search:       /pattern       (find text)
Command:      :              (enter command mode)
Syntax:       s              (toggle highlighting)
```

### Command Mode Quick Reference
```
:w            Save file
:w filename   Save as filename
:q            Quit (if no changes)
:q!           Force quit
:wq           Save and quit
:d            Delete current line
```

### Insert Mode Quick Reference
```
ESC           Return to normal mode
Backspace     Delete character left
Enter         New line
Any char      Insert character
```

## Limitations

### Current Limitations
- **File size**: Maximum 100 lines per file
- **Line length**: Maximum 255 characters per line
- **Single file**: Only one file open at a time
- **No undo**: No undo/redo functionality
- **Basic search**: Forward search only, no replace

### Memory Management
- Files loaded entirely into memory
- Automatic memory cleanup on exit
- Dynamic filename allocation

## Advanced Features

### Syntax Highlighting Engine
- **Token-based parsing** for accurate highlighting
- **Instruction recognition** for x86 assembly
- **Flexible color scheme** with VGA color support
- **Runtime toggling** for performance or preference

### File Format Support
- **Newline preservation** maintains original file formatting
- **Cross-platform compatibility** handles different line endings
- **Binary safety** preserves file integrity

## Tips and Best Practices

### Efficient Editing
1. Use `hjkl` for navigation instead of arrow keys
2. Toggle syntax highlighting (`s`) for better visibility
3. Use `dd` for quick line deletion
4. Save frequently with `:w`

### Assembly Development
1. Syntax highlighting auto-enables for `.asm` files
2. Use `:w program.asm` to save with proper extension
3. Comments are highlighted for better code documentation
4. Register and instruction highlighting aids in code review

### File Management
1. Always check status line for unsaved changes
2. Use `:wq` for quick save-and-exit
3. Use `:q!` only when certain about discarding changes
4. Search functionality helps navigate large assembly files

ZX86Vi provides a solid foundation for text editing within the ZX86v2 operating system, with particular strength in assembly language development and basic text manipulation tasks.
