#ifndef PARSER_H
#define PARSER_H

#include "types.h"
#include "utils.h"
#include "symbol_table.h"

// verificar se a linha contém apenas uma label
static inline int is_label_only(const char* line) {
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == ':') return 1;
    return 0;
}

// remove o : do final da label e retorna uma copia
static inline char* strip_label(char* token) {
    size_t len = strlen(token);
    if (len > 0 && token[len - 1] == ':') token[len - 1] = '\0';
    return my_strdup(token);
}

// faz split dos operandos por virgula
static inline int split_operands(char* str, char* operands[4]) {
    int count = 0;
    char* token = strtok(str, ",");
    while (token && count < 4) {
        operands[count++] = my_strdup(ltrim(token));
        token = strtok(NULL, ",");
    }
    return count;
}

// parse uma linha e retorna um instruction_t
static inline instruction_t parse_line(const char* line, uint32_t line_number) {
    instruction_t inst = {0};
    inst.line_number = line_number;

    char buffer[SOURCE_LINE_MAX];
    strncpy(buffer, line, SOURCE_LINE_MAX - 1);
    buffer[SOURCE_LINE_MAX - 1] = '\0';

    char* token = strtok(buffer, " \t");

    // caso tenha label
    if (token && strchr(token, ':')) {
        inst.label = strip_label(token);
        token = strtok(NULL, " \t"); // proximo token (possivel mnemonic)
    }

    // caso nao tenha instrucao (so label)
    if (!token) return inst;

    strncpy(inst.mnemonic, token, sizeof(inst.mnemonic) - 1);
    inst.mnemonic[sizeof(inst.mnemonic) - 1] = '\0';

    // restante da linha sao os operandos
    char* operand_str = strtok(NULL, "\n");
    if (operand_str) {
        inst.operand_count = split_operands(operand_str, inst.operands);
    }

    return inst;
}

// parse um vetor de linhas em vetor de instruções
instruction_t* parse_lines(char** lines, size_t line_count, size_t* out_count, symbol_table_t* table) {
    instruction_t* instructions = malloc(line_count * sizeof(instruction_t));
    CHECK_ALLOC(instructions, return NULL);

    char* pending_label = NULL;
    size_t count = 0;

    for (size_t i = 0; i < line_count; i++) {
        char* line = lines[i];

        // caso a linha seja apenas uma label
        if (is_label_only(line)) {
            free(pending_label);
            pending_label = strip_label(line);
            continue;
        }

        instruction_t inst = parse_line(line, (uint32_t)i + 1);
        inst.address = BASE_ADDRESS + 4 * count;

        // se havia uma label pendente
        if (pending_label) {
            inst.label = pending_label;
            symbol_table_add(table, pending_label, inst.address);
            pending_label = NULL;
        }

        // se a instrução possui label inline, como `main2: beq` (tive problema com isso)
        else if (inst.label) {
            symbol_table_add(table, inst.label, inst.address);
        }

        instructions[count++] = inst;
    }

    // se sobrou uma label no final
    if (pending_label) {
        symbol_table_add(table, pending_label, BASE_ADDRESS + 4 * count);
        free(pending_label);
    }

    *out_count = count;
    return instructions;
}


static inline void instruction_dump(const instruction_t* instr) {
    printf("instruction at 0x%08X (line %u):\n", instr->address, instr->line_number);
    if (instr->label)
        printf("  label:    %s\n", instr->label);
    else
        printf("  label:    (none)\n");

    printf("  mnemonic: %s\n", instr->mnemonic);
    printf("  operands: ");
    for (int i = 0; i < instr->operand_count; ++i) {
        printf("%s", instr->operands[i]);
        if (i < instr->operand_count - 1)
            printf(", ");
    }
    if (instr->operand_count == 0)
        printf("(none)");
    printf("\n");
    printf("-----------------------------------\n");
}


// libera vetor de instructions (kkk caso eu lembre de usar essa porra)
static inline void free_instructions(instruction_t* instructions, size_t count) {
    if (!instructions) return;
    for (size_t i = 0; i < count; i++) {
        free(instructions[i].label);
        for (int j = 0; j < instructions[i].operand_count; j++) {
            free(instructions[i].operands[j]);
        }
    }
    free(instructions);
}

#endif