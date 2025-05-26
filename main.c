// main.c
#include "include/utils.h"
#include "include/tokenizer.h"
#include "include/parser.h"
#include "include/encoder.h"
#include "include/symbol_table.h"

#define MAX_LINES 1024

int main() {
    size_t line_count;
    char** lines = read_file_lines("programa.asm", &line_count);
    if (!lines) {
        fprintf(stderr, "erro ao ler arquivo.\n");
        return 1;
    }

    symbol_table_t table;
    symbol_table_init(&table);  // inicializa a tabela

    size_t inst_count;
    instruction_t* instructions = parse_lines(lines, line_count, &inst_count, &table);
    if (!instructions) {
        fprintf(stderr, "erro ao parsear instruções.\n");
        free_lines(lines);
        symbol_table_free(&table);
        return 1;
    } 

    printf("=== instructions ===\n");
    for (size_t i = 0; i < inst_count; i++) {
        instruction_dump(&instructions[i]);
    }
    puts("");
    puts("");
    symbol_table_dump(&table);

    // libera tudo (la ele)
    free_instructions(instructions, inst_count);
    free_lines(lines);
    symbol_table_free(&table);

    return 0;
}
