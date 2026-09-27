// disasm.c -- x86-32 disassembler
// Comprehensive 32-bit x86 instruction decoder and disassembler

#include "../include/common.h"

// Register names
static const char* reg32_names[] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"};
static const char* reg16_names[] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
static const char* reg8_names[] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};

// Instruction context
typedef struct {
    uint8_t* code;
    uint32_t address;
    uint32_t pos;
    uint32_t max_len;
    bool has_66h_prefix;  // operand size override
    bool has_67h_prefix;  // address size override
    bool has_f2h_prefix;  // repne
    bool has_f3h_prefix;  // rep/repe
} disasm_ctx_t;

// Helper to read bytes
static uint8_t read_byte(disasm_ctx_t* ctx) {
    if (ctx->pos >= ctx->max_len) return 0;
    return ctx->code[ctx->pos++];
}

static uint16_t read_word(disasm_ctx_t* ctx) {
    uint16_t val = read_byte(ctx);
    val |= (uint16_t)read_byte(ctx) << 8;
    return val;
}

static uint32_t read_dword(disasm_ctx_t* ctx) {
    uint32_t val = read_word(ctx);
    val |= (uint32_t)read_word(ctx) << 16;
    return val;
}

// ModR/M and SIB byte decoding
static void decode_modrm_sib(disasm_ctx_t* ctx, uint8_t modrm, char* output, bool is_32bit) {
    uint8_t mod = (modrm >> 6) & 3;
    uint8_t rm = modrm & 7;
    
    if (mod == 3) {
        // Register direct
        if (is_32bit) {
            sprintf(output, "%s", reg32_names[rm]);
        } else {
            sprintf(output, "%s", reg16_names[rm]);
        }
        return;
    }
    
    // Memory addressing
    if (rm == 4 && !ctx->has_67h_prefix) {
        // SIB byte required
        uint8_t sib = read_byte(ctx);
        uint8_t scale = (sib >> 6) & 3;
        uint8_t index = (sib >> 3) & 7;
        uint8_t base = sib & 7;
        
        char scale_str[8] = "";
        if (scale > 0) {
            sprintf(scale_str, "*%d", 1 << scale);
        }
        
        if (mod == 0 && base == 5) {
            // Displacement only
            uint32_t disp = read_dword(ctx);
            if (index == 4) {
                sprintf(output, "[0x%08x]", disp);
            } else {
                sprintf(output, "[%s%s+0x%08x]", reg32_names[index], scale_str, disp);
            }
        } else {
            char disp_str[16] = "";
            if (mod == 1) {
                int8_t disp = (int8_t)read_byte(ctx);
                if (disp != 0) {
                    sprintf(disp_str, "%+d", disp);
                }
            } else if (mod == 2) {
                int32_t disp = (int32_t)read_dword(ctx);
                if (disp != 0) {
                    sprintf(disp_str, "%+d", disp);
                }
            }
            
            if (index == 4) {
                sprintf(output, "[%s%s]", reg32_names[base], disp_str);
            } else {
                sprintf(output, "[%s+%s%s%s]", reg32_names[base], reg32_names[index], scale_str, disp_str);
            }
        }
    } else {
        // No SIB byte
        char disp_str[16] = "";
        
        if (mod == 0 && rm == 5) {
            // Displacement only
            uint32_t disp = read_dword(ctx);
            sprintf(output, "[0x%08x]", disp);
        } else {
            if (mod == 1) {
                int8_t disp = (int8_t)read_byte(ctx);
                if (disp != 0) {
                    sprintf(disp_str, "%+d", disp);
                }
            } else if (mod == 2) {
                int32_t disp = (int32_t)read_dword(ctx);
                if (disp != 0) {
                    sprintf(disp_str, "%+d", disp);
                }
            }
            sprintf(output, "[%s%s]", reg32_names[rm], disp_str);
        }
    }
}

// Disassemble a single instruction
static uint32_t disasm_instruction(disasm_ctx_t* ctx, char* output) {
    uint32_t start_pos = ctx->pos;
    uint32_t start_addr = ctx->address + ctx->pos;
    
    // Reset prefixes
    ctx->has_66h_prefix = false;
    ctx->has_67h_prefix = false;
    ctx->has_f2h_prefix = false;
    ctx->has_f3h_prefix = false;
    
    // Handle prefixes
    bool prefix_found = true;
    while (prefix_found && ctx->pos < ctx->max_len) {
        uint8_t byte = ctx->code[ctx->pos];
        switch (byte) {
            case 0x66: ctx->has_66h_prefix = true; ctx->pos++; break;
            case 0x67: ctx->has_67h_prefix = true; ctx->pos++; break;
            case 0xF2: ctx->has_f2h_prefix = true; ctx->pos++; break;
            case 0xF3: ctx->has_f3h_prefix = true; ctx->pos++; break;
            default: prefix_found = false; break;
        }
    }
    
    if (ctx->pos >= ctx->max_len) {
        sprintf(output, "???");
        return ctx->pos - start_pos;
    }
    
    uint8_t opcode = read_byte(ctx);
    
    switch (opcode) {
        // Single byte instructions
        case 0x90: sprintf(output, "nop"); break;
        case 0xC3: sprintf(output, "ret"); break;
        case 0xCB: sprintf(output, "retf"); break;
        case 0xCC: sprintf(output, "int3"); break;
        case 0xF4: sprintf(output, "hlt"); break;
        case 0xFA: sprintf(output, "cli"); break;
        case 0xFB: sprintf(output, "sti"); break;
        case 0x9C: sprintf(output, "pushfd"); break;
        case 0x9D: sprintf(output, "popfd"); break;
        case 0x60: sprintf(output, "pushad"); break;
        case 0x61: sprintf(output, "popad"); break;
        case 0x99: sprintf(output, "cdq"); break;
        case 0xF9: sprintf(output, "stc"); break;
        case 0xF8: sprintf(output, "clc"); break;
        case 0xFD: sprintf(output, "std"); break;
        case 0xFC: sprintf(output, "cld"); break;
        
        // Push/Pop register
        case 0x50: case 0x51: case 0x52: case 0x53:
        case 0x54: case 0x55: case 0x56: case 0x57:
            sprintf(output, "push %s", reg32_names[opcode - 0x50]);
            break;
            
        case 0x58: case 0x59: case 0x5A: case 0x5B:
        case 0x5C: case 0x5D: case 0x5E: case 0x5F:
            sprintf(output, "pop %s", reg32_names[opcode - 0x58]);
            break;
        
        // MOV immediate to register
        case 0xB0: case 0xB1: case 0xB2: case 0xB3:
        case 0xB4: case 0xB5: case 0xB6: case 0xB7: {
            uint8_t imm = read_byte(ctx);
            sprintf(output, "mov %s, 0x%02x", reg8_names[opcode - 0xB0], imm);
            break;
        }
        
        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
        case 0xBC: case 0xBD: case 0xBE: case 0xBF: {
            uint32_t imm = read_dword(ctx);
            sprintf(output, "mov %s, 0x%08x", reg32_names[opcode - 0xB8], imm);
            break;
        }
        
        // MOV r/m, r and MOV r, r/m
        case 0x88: case 0x89: case 0x8A: case 0x8B: {
            uint8_t modrm = read_byte(ctx);
            uint8_t reg = (modrm >> 3) & 7;
            char rm_str[64];
            
            bool is_32bit = (opcode & 1) && !ctx->has_66h_prefix;
            bool to_rm = (opcode & 2) == 0;
            
            decode_modrm_sib(ctx, modrm, rm_str, is_32bit);
            
            if (is_32bit) {
                if (to_rm) {
                    sprintf(output, "mov %s, %s", rm_str, reg32_names[reg]);
                } else {
                    sprintf(output, "mov %s, %s", reg32_names[reg], rm_str);
                }
            } else {
                if (to_rm) {
                    sprintf(output, "mov %s, %s", rm_str, reg8_names[reg]);
                } else {
                    sprintf(output, "mov %s, %s", reg8_names[reg], rm_str);
                }
            }
            break;
        }
        
        // ADD r/m, r and ADD r, r/m
        case 0x00: case 0x01: case 0x02: case 0x03: {
            uint8_t modrm = read_byte(ctx);
            uint8_t reg = (modrm >> 3) & 7;
            char rm_str[64];
            
            bool is_32bit = (opcode & 1) && !ctx->has_66h_prefix;
            bool to_rm = (opcode & 2) == 0;
            
            decode_modrm_sib(ctx, modrm, rm_str, is_32bit);
            
            if (is_32bit) {
                if (to_rm) {
                    sprintf(output, "add %s, %s", rm_str, reg32_names[reg]);
                } else {
                    sprintf(output, "add %s, %s", reg32_names[reg], rm_str);
                }
            } else {
                if (to_rm) {
                    sprintf(output, "add %s, %s", rm_str, reg8_names[reg]);
                } else {
                    sprintf(output, "add %s, %s", reg8_names[reg], rm_str);
                }
            }
            break;
        }
        
        // SUB r/m, r and SUB r, r/m
        case 0x28: case 0x29: case 0x2A: case 0x2B: {
            uint8_t modrm = read_byte(ctx);
            uint8_t reg = (modrm >> 3) & 7;
            char rm_str[64];
            
            bool is_32bit = (opcode & 1) && !ctx->has_66h_prefix;
            bool to_rm = (opcode & 2) == 0;
            
            decode_modrm_sib(ctx, modrm, rm_str, is_32bit);
            
            if (is_32bit) {
                if (to_rm) {
                    sprintf(output, "sub %s, %s", rm_str, reg32_names[reg]);
                } else {
                    sprintf(output, "sub %s, %s", reg32_names[reg], rm_str);
                }
            } else {
                if (to_rm) {
                    sprintf(output, "sub %s, %s", rm_str, reg8_names[reg]);
                } else {
                    sprintf(output, "sub %s, %s", reg8_names[reg], rm_str);
                }
            }
            break;
        }
        
        // CMP r/m, r and CMP r, r/m
        case 0x38: case 0x39: case 0x3A: case 0x3B: {
            uint8_t modrm = read_byte(ctx);
            uint8_t reg = (modrm >> 3) & 7;
            char rm_str[64];
            
            bool is_32bit = (opcode & 1) && !ctx->has_66h_prefix;
            bool to_rm = (opcode & 2) == 0;
            
            decode_modrm_sib(ctx, modrm, rm_str, is_32bit);
            
            if (is_32bit) {
                if (to_rm) {
                    sprintf(output, "cmp %s, %s", rm_str, reg32_names[reg]);
                } else {
                    sprintf(output, "cmp %s, %s", reg32_names[reg], rm_str);
                }
            } else {
                if (to_rm) {
                    sprintf(output, "cmp %s, %s", rm_str, reg8_names[reg]);
                } else {
                    sprintf(output, "cmp %s, %s", reg8_names[reg], rm_str);
                }
            }
            break;
        }
        
        // Conditional jumps (short)
        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
        case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F: {
            static const char* cc_names[] = {
                "jo", "jno", "jb", "jae", "je", "jne", "jbe", "ja",
                "js", "jns", "jp", "jnp", "jl", "jge", "jle", "jg"
            };
            int8_t disp = (int8_t)read_byte(ctx);
            uint32_t target = start_addr + (ctx->pos - start_pos) + disp;
            sprintf(output, "%s 0x%08x", cc_names[opcode - 0x70], target);
            break;
        }
        
        // JMP short
        case 0xEB: {
            int8_t disp = (int8_t)read_byte(ctx);
            uint32_t target = start_addr + (ctx->pos - start_pos) + disp;
            sprintf(output, "jmp 0x%08x", target);
            break;
        }
        
        // JMP near
        case 0xE9: {
            int32_t disp = (int32_t)read_dword(ctx);
            uint32_t target = start_addr + (ctx->pos - start_pos) + disp;
            sprintf(output, "jmp 0x%08x", target);
            break;
        }
        
        // CALL near
        case 0xE8: {
            int32_t disp = (int32_t)read_dword(ctx);
            uint32_t target = start_addr + (ctx->pos - start_pos) + disp;
            sprintf(output, "call 0x%08x", target);
            break;
        }
        
        // INT imm8
        case 0xCD: {
            uint8_t imm = read_byte(ctx);
            sprintf(output, "int 0x%02x", imm);
            break;
        }
        
        // Group FF (CALL/JMP indirect, PUSH, etc.)
        case 0xFF: {
            uint8_t modrm = read_byte(ctx);
            uint8_t reg = (modrm >> 3) & 7;
            char rm_str[64];
            
            decode_modrm_sib(ctx, modrm, rm_str, true);
            
            switch (reg) {
                case 0: sprintf(output, "inc %s", rm_str); break;
                case 1: sprintf(output, "dec %s", rm_str); break;
                case 2: sprintf(output, "call %s", rm_str); break;
                case 3: sprintf(output, "call far %s", rm_str); break;
                case 4: sprintf(output, "jmp %s", rm_str); break;
                case 5: sprintf(output, "jmp far %s", rm_str); break;
                case 6: sprintf(output, "push %s", rm_str); break;
                default: sprintf(output, "??? (ff /%d)", reg); break;
            }
            break;
        }
        
        // Two-byte opcodes
        case 0x0F: {
            uint8_t opcode2 = read_byte(ctx);
            switch (opcode2) {
                // Conditional moves
                case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
                case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F: {
                    static const char* cc_names[] = {
                        "cmovo", "cmovno", "cmovb", "cmovae", "cmove", "cmovne", "cmovbe", "cmova",
                        "cmovs", "cmovns", "cmovp", "cmovnp", "cmovl", "cmovge", "cmovle", "cmovg"
                    };
                    uint8_t modrm = read_byte(ctx);
                    uint8_t reg = (modrm >> 3) & 7;
                    char rm_str[64];
                    decode_modrm_sib(ctx, modrm, rm_str, true);
                    sprintf(output, "%s %s, %s", cc_names[opcode2 - 0x40], reg32_names[reg], rm_str);
                    break;
                }
                
                // Conditional jumps (near)
                case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87:
                case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8E: case 0x8F: {
                    static const char* cc_names[] = {
                        "jo", "jno", "jb", "jae", "je", "jne", "jbe", "ja",
                        "js", "jns", "jp", "jnp", "jl", "jge", "jle", "jg"
                    };
                    int32_t disp = (int32_t)read_dword(ctx);
                    uint32_t target = start_addr + (ctx->pos - start_pos) + disp;
                    sprintf(output, "%s 0x%08x", cc_names[opcode2 - 0x80], target);
                    break;
                }
                
                default:
                    sprintf(output, "??? (0f %02x)", opcode2);
                    break;
            }
            break;
        }
        
        default:
            sprintf(output, "??? (%02x)", opcode);
            break;
    }
    
    return ctx->pos - start_pos;
}

// Main disassembly function
void disasm(unsigned char *code, uint32_t codelength) {
    disasm_ctx_t ctx = {
        .code = code,
        .address = (uint32_t)code,
        .pos = 0,
        .max_len = codelength
    };
    
    printf("Disassembly at 0x%08X:\n", ctx.address);
    
    while (ctx.pos < codelength) {
        uint32_t inst_addr = ctx.address + ctx.pos;
        uint32_t start_pos = ctx.pos;
        char instruction[256];
        
        uint32_t inst_len = disasm_instruction(&ctx, instruction);
        
        // Print address and hex bytes
        printf("%08X  ", inst_addr);
        
        // Print up to 8 hex bytes
        for (uint32_t i = 0; i < 8; i++) {
            if (i < inst_len) {
                printf("%02X ", code[start_pos + i]);
            } else {
                printf("   ");
            }
        }
        
        printf(" %s\n", instruction);
        
        // If instruction was longer than 8 bytes, print continuation
        if (inst_len > 8) {
            for (uint32_t i = 8; i < inst_len; i += 8) {
                printf("         ");
                for (uint32_t j = 0; j < 8 && (i + j) < inst_len; j++) {
                    printf("%02X ", code[start_pos + i + j]);
                }
                printf("\n");
            }
        }
    }
}