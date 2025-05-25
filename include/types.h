#ifndef TYPES_H
#define TYPES_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define SOURCE_LINE_MAX 256
#define BASE_ADDRESS 0x00400000

// enum para o tipo da instrução
typedef enum {
    INST_R, INST_I,
    INST_S, INST_B,
    INST_U, INST_J,
    INST_INVALID
} INST_TYPE;

// para a tablela de simbolos (nao sei se vou fazer usando a tabela)
typedef struct {
    char* label;
    uint32_t address;
} symbol_t;

typedef struct {
    symbol_t* entries;
    size_t count;
    size_t capacity;
} symbol_table_t;


// struct parar representar a instrução depois do parser
typedef struct {
    char *label;             // label associada (ou NULL)
    char mnemonic[8];        // nome da instrução
    char *operands[4];       // até 4 operandos
    int operand_count;       // qt. de operandos
    uint32_t line_number;    // linha no source code
    uint32_t address;
} instruction_t;

// union utilizando bitfields para guardar as instruções 'encoded' 
// (espero que dê certo, n sei como isso funciona direito)
typedef union {
    struct {
        uint32_t opcode  : 7;
        uint32_t rd      : 5;
        uint32_t funct3  : 3;
        uint32_t rs1     : 5;
        uint32_t rs2     : 5;
        uint32_t funct7  : 7;
    } r;

    struct {
        uint32_t opcode  : 7;
        uint32_t rd      : 5;
        uint32_t funct3  : 3;
        uint32_t rs1     : 5;
        int32_t  imm     : 12;
    } i;

    struct {
        uint32_t opcode  : 7;
        uint32_t imm4_0  : 5;
        uint32_t funct3  : 3;
        uint32_t rs1     : 5;
        uint32_t rs2     : 5;
        int32_t  imm11_5 : 7;
    } s;

    struct {
        uint32_t opcode  : 7;
        uint32_t imm11   : 1;
        uint32_t imm4_1  : 4;
        uint32_t funct3  : 3;
        uint32_t rs1     : 5;
        uint32_t rs2     : 5;
        uint32_t imm10_5 : 6;
        uint32_t imm12   : 1;
    } b;

    struct {
        uint32_t opcode  : 7;
        uint32_t rd      : 5;
        int32_t  imm     : 20;
    } u;

    struct {
        uint32_t opcode  : 7;
        uint32_t rd      : 5;
        uint32_t imm19_12: 8;
        uint32_t imm11   : 1;
        uint32_t imm10_1 : 10;
        uint32_t imm20   : 1;
    } j;
} encoded_fields_t;

// instrução na forma final
typedef struct {
    INST_TYPE type;
    encoded_fields_t fields; 
} encoded_instruction_t;


#endif 