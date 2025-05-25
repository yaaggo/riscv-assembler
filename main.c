// main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/utils.h"
#include "include/tokenizer.h"
#include "include/parser.h"
#include "include/encoder.h"
#include "include/symbol_table.h"

#define MAX_LINES 1024

int main() {
    size_t line_count;
    char** lines = read_file_lines("programa.asm", &line_count);

    size_t inst_count;
    instruction_t* instructions = parse_lines(lines, line_count, &inst_count);

    for (size_t i = 0; i < inst_count; i++) {
        printf("linha %u: %s", instructions[i].line_number, instructions[i].mnemonic);
        for (int j = 0; j < instructions[i].operand_count; j++)
            printf(" %s", instructions[i].operands[j]);
        if (instructions[i].label)
            printf(" (label: %s)", instructions[i].label);
        puts("");
    }

    free_instructions(instructions, inst_count);
    free_lines(lines);
}
