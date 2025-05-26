#ifndef ENCODING_TABLES_H
#define ENCODING_TABLES_H

#include <stdint.h>
#include <string.h>
#include "types.h"

typedef struct {
    const char *name;
    uint8_t number;
} register_entry_t;

static const register_entry_t reg_table[] = {
    {"zero", 0}, {"x0", 0},
    {"ra", 1},   {"x1", 1},
    {"sp", 2},   {"x2", 2},
    {"gp", 3},   {"x3", 3},
    {"tp", 4},   {"x4", 4},
    {"t0", 5},   {"x5", 5},
    {"t1", 6},   {"x6", 6},
    {"t2", 7},   {"x7", 7},
    {"s0", 8},   {"fp", 8}, {"x8", 8},
    {"s1", 9},   {"x9", 9},
    {"a0", 10},  {"x10", 10},
    {"a1", 11},  {"x11", 11},
    {"a2", 12},  {"x12", 12},
    {"a3", 13},  {"x13", 13},
    {"a4", 14},  {"x14", 14},
    {"a5", 15},  {"x15", 15},
    {"a6", 16},  {"x16", 16},
    {"a7", 17},  {"x17", 17},
    {"s2", 18},  {"x18", 18},
    {"s3", 19},  {"x19", 19},
    {"s4", 20},  {"x20", 20},
    {"s5", 21},  {"x21", 21},
    {"s6", 22},  {"x22", 22},
    {"s7", 23},  {"x23", 23},
    {"s8", 24},  {"x24", 24},
    {"s9", 25},  {"x25", 25},
    {"s10", 26}, {"x26", 26},
    {"s11", 27}, {"x27", 27},
    {"t3", 28},  {"x28", 28},
    {"t4", 29},  {"x29", 29},
    {"t5", 30},  {"x30", 30},
    {"t6", 31},  {"x31", 31}
};

static inline int get_register_number(const char *name) {
    for (size_t i = 0; i < sizeof(reg_table)/sizeof(reg_table[0]); i++)
        if (strcmp(reg_table[i].name, name) == 0)
            return reg_table[i].number;
    return -1; 
}

typedef struct {
    const char *mnemonic;
    INST_TYPE type;
    uint8_t opcode;
    int8_t funct3;  // pode ser -1 se não se aplica
    int8_t funct7;  // idem
} instruction_entry_t;

static const instruction_entry_t inst_table[] = {
    {"add",  INST_R, 0b0110011, 0b000, 0b0000000},
    {"sub",  INST_R, 0b0110011, 0b000, 0b0100000},
    {"xor",  INST_R, 0b0110011, 0b100, 0b0000000},
    {"or",   INST_R, 0b0110011, 0b110, 0b0000000},
    {"and",  INST_R, 0b0110011, 0b111, 0b0000000},

    {"addi", INST_I, 0b0010011, 0b000, -1},
    {"slli", INST_I, 0b0010011, 0b001, 0b0000000},
    {"srli", INST_I, 0b0010011, 0b101, 0b0000000},
    {"jalr", INST_I, 0b1100111, 0b000, -1},
    {"lw",   INST_I, 0b0000011, 0b010, -1},

    {"sw",   INST_S, 0b0100011, 0b010, -1},

    {"beq",  INST_B, 0b1100011, 0b000, -1},
    {"bne",  INST_B, 0b1100011, 0b001, -1},

    {"lui",  INST_U, 0b0110111, -1, -1},

    {"jal",  INST_J, 0b1101111, -1, -1}
};

static inline const instruction_entry_t* find_instruction(const char *mnemonic) {
    for (size_t i = 0; i < sizeof(inst_table)/sizeof(inst_table[0]); i++)
        if (strcmp(inst_table[i].mnemonic, mnemonic) == 0)
            return &inst_table[i];
    return NULL;
}

#endif 