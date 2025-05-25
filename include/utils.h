// utils.h
#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "types.h"

// macro para verificar alocações
#define CHECK_ALLOC(p, action_on_fail)                                                  \
    do {                                                                                \
        if ((p) == NULL) {                                                              \
            fprintf(stderr, "memory allocation failed at %s:%d\n", __FILE__, __LINE__); \
            action_on_fail;                                                             \
        }                                                                               \
    } while (0)

// remover os espaços do inicio da expressão
static inline char* ltrim(char* str) {
    while (isspace((unsigned char)*str)) str++;
    return str;
}

// implementação propria para a strdup, por ser uma extensão posix
static inline char* my_strdup(const char* s) {
    size_t len = strlen(s);
    char* copy = (char *)malloc(len + 1);
    CHECK_ALLOC(copy, return NULL);
    strcpy(copy, s);
    return copy;
}

/**
 * le um arquivo e retorna um vetor de strings.
 * 
 * parametros:
 *   - filename: nome do arquivo;
 *   - line_count: ponteiro para armazenar o número de linhas lidas.
 */
static inline char** read_file_lines(const char* filename, size_t* line_count) {  
    FILE* f = fopen(filename, "rb"); // abrindo para leitura de arquivo binário, pensando na compatibilidade

    if (!f) return NULL;

    int max_lines = 0;
    char **lines = NULL;
    char buffer[SOURCE_LINE_MAX];

    *line_count = 0;

    while (fgets(buffer, SOURCE_LINE_MAX, f)) {
        buffer[strcspn(buffer, "\r\n")] = '\0';

        char *line = ltrim(buffer); // tira espaços iniciais nas linhas
        
        if (line[0] == '\0' || line[0] == '#') // ignora linhas vazias
            continue;

        if (*line_count >= (size_t)max_lines) {
            max_lines = max_lines == 0 ? 8 : max_lines * 2;
            lines = (char **) realloc(lines, max_lines * sizeof(char *));
            CHECK_ALLOC(lines, { fclose(f); return NULL; });
        }

        // salva a linha copiando da própria memória
        lines[*line_count] = my_strdup(line);
        CHECK_ALLOC(lines[*line_count], { fclose(f); return NULL; });

        (*line_count)++;
    }

    fclose(f);
    return lines;
}

// libera a memória alocada por read_file_lines().
static inline void free_lines(char** lines) {
    if (!lines) return;
    for (size_t i = 0; lines[i]; i++) {
        free(lines[i]);
    }
    free(lines);
}

// para debug
static inline void print_lines(char** lines, int line_count) {
    if (!lines) return;
    
    if (line_count <= 0) {
        for (size_t i = 0; lines[i]; i++) printf("%s\n", lines[i]);
    } else {
        for (int i = 0; i < line_count; i++) printf("%s\n", lines[i]);
    }
}

#endif // UTILS_H
