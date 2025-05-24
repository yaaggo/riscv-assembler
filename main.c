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

int main(int argc, char **argv) {

    // erro caso não seja passado o o arquivo como parametro para leitura
    if (argc < 2) {
        fprintf(stderr, "usage: %s <filename>\n", argv[0]);
        return 1;
    }

    size_t line_count = 0;
    char** lines = read_file_lines(argv[1], &line_count);

    // zu é para printar numeros do tipo size_t, sem sinal
    printf("read %zu lines (excluding empty and comment lines):\n\n", sizeof(encoded_fields_t), line_count);
    print_lines(lines, line_count);
    free_lines(lines);

}
