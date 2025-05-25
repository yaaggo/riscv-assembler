#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "types.h"
#include "utils.h"

#define ST_INITIAL_CAPACITY 8

// inicializa a tabela
static inline void symbol_table_init(symbol_table_t* table) {
    table->count = 0;
    table->capacity = ST_INITIAL_CAPACITY;
    table->entries = (symbol_t*)malloc(sizeof(symbol_t) * table->capacity);
    CHECK_ALLOC(table->entries, exit(EXIT_FAILURE));
}

// libera memoria da tabela
static inline void symbol_table_free(symbol_table_t* table) {
    for (size_t i = 0; i < table->count; ++i)
        free(table->entries[i].label);
    free(table->entries);
}

// adiciona uma nova label com endereço
static inline void symbol_table_add(symbol_table_t* table, const char* label, uint32_t address) {
    // verifica se já existe
    for (size_t i = 0; i < table->count; ++i) {
        if (strcmp(table->entries[i].label, label) == 0) {
            fprintf(stderr, "symbol '%s' already defined\n", label);
            exit(EXIT_FAILURE);
        }
    }

    if (table->count >= table->capacity) {
        table->capacity *= 2;
        symbol_t* new_entries = (symbol_t*)realloc(table->entries, table->capacity * sizeof(symbol_t));
        CHECK_ALLOC(new_entries, exit(EXIT_FAILURE));
        table->entries = new_entries;
    }

    table->entries[table->count].label = my_strdup(label);
    table->entries[table->count].address = address;
    table->count++;
}

// busca uma label, retorna o endereço. Se não encontrar, retorna -1 (0xFFFFFFFF)
static inline int32_t symbol_table_lookup(const symbol_table_t* table, const char* label) {
    for (size_t i = 0; i < table->count; ++i) {
        if (strcmp(table->entries[i].label, label) == 0)
            return table->entries[i].address;
    }
    return -1;  // (talvez fazer códigos especiais de erro, mas isso dai vai ficar para o yago do futuro)
}

// debug: imprime todos os símbolos
static inline void symbol_table_dump(const symbol_table_t* table) {
    printf("=== symbol table ===\n");
    for (size_t i = 0; i < table->count; ++i)
        printf("  %s -> 0x%08X\n", table->entries[i].label, table->entries[i].address);
}

#endif
