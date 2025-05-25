#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "types.h"
#include "utils.h"

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

    char buffer[LINE_MAX];
    strncpy(buffer, line, LINE_MAX - 1);
    buffer[LINE_MAX - 1] = '\0';

    char* token = strtok(buffer, " \t");

    // caso tenha label
    if (token && strchr(token, ':')) {
        inst.label = strip_label(token);
        token = strtok(NULL, " \t"); // próximo token (possível mnemonic)
    }

    // caso não tenha instrucao (só label)
    if (!token) return inst;

    strncpy(inst.mnemonic, token, sizeof(inst.mnemonic) - 1);
    inst.mnemonic[sizeof(inst.mnemonic) - 1] = '\0';

    // restante da linha são os operandos
    char* operand_str = strtok(NULL, "\n");
    if (operand_str) {
        inst.operand_count = split_operands(operand_str, inst.operands);
    }

    return inst;
}

// parse um vetor de linhas em vetor de instruções
static inline instruction_t* parse_lines(char** lines, size_t line_count, size_t* out_count) {
    instruction_t* instructions = malloc(line_count * sizeof(instruction_t));
    CHECK_ALLOC(instructions, return NULL);

    char* pending_label = NULL;
    size_t count = 0;

    for (size_t i = 0; i < line_count; i++) {
        char* line = lines[i];
        if (is_label_only(line)) {
            free(pending_label); // caso tenha alguma anterior
            pending_label = strip_label(line);
            continue;
        }

        instruction_t inst = parse_line(line, (uint32_t)i + 1);
        if (pending_label) {
            inst.label = pending_label;
            pending_label = NULL;
        }

        instructions[count++] = inst;
    }

    free(pending_label);
    *out_count = count;
    return instructions;
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