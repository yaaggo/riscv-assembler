#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
//============== MACROS PARA ALTERAÇÃO DO FUNCIONAMENTO DO SAVE ==========================

// 1 para byte mais significativo primeiro
// 0 para byte menos significativo primeiro 
// (quando MIF_OUTPUT_GRANULARITY == 8)

#define MIF_PRINT_BYTES_BIG_ENDIAN 0


// 32 para a palavra inteira, 8 para imprimir de 8 em 8 bits

#define MIF_OUTPUT_GRANULARITY 8 

//========================================================================================

#define SOURCE_LINE_MAX 256
#define BASE_ADDRESS 0x00400000 // endereço base para o onde começa o bloco das palavras na memoria
#define ST_INITIAL_CAPACITY 8
#define ENCODING_ERROR_SENTINEL 0xFFFFFFFF 

// macro para verificar alocações
#define CHECK_ALLOC(p, action_on_fail)                                                  \
    do {                                                                                \
        if ((p) == NULL) {                                                              \
            fprintf(stderr, "memory allocation failed at %s:%d\n", __FILE__, __LINE__); \
            action_on_fail;                                                             \
        }                                                                               \
    } while (0)

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

    uint32_t word;
    
} encoded_fields_t;

// instrução na forma final
typedef struct {
    INST_TYPE type;
    encoded_fields_t fields; 
} encoded_instruction_t;

// remover os espaços do inicio da expressão
static inline char* ltrim(char* str) {
    while (isspace((unsigned char)*str)) str++;
    return str;
}

// analogamente só que do fim
static inline char* rtrim(char* str) {
    if (str == NULL || *str == '\0')
        return str;
    char* end = str + strlen(str) - 1;
    while (end >= str && isspace((unsigned char)*end))
        end--;
    *(end + 1) = '\0';
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


// le um arquivo e retorna um vetor de strings.

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

// converte uma string de imediato (decimal ou hexadecimal) para um valor int32_t.
static inline int32_t parse_immediate(const char* imm_str, bool* success) {
    if (imm_str == NULL) {
        *success = false;
        return 0;
    }
    char* endptr;
    long val = strtol(imm_str, &endptr, 0);
    if (endptr == imm_str || *endptr != '\0') { 
        *success = false;
        return 0;
    }
    *success = true;
    return (int32_t)val;
}


// codifica uma instrução parseada para seu formato binário de 32 bits.
static inline uint32_t encode_instruction(const instruction_t* parsed_inst, 
                                          const symbol_table_t* symbols, 
                                          uint32_t current_address) {
    if (!parsed_inst) return ENCODING_ERROR_SENTINEL;

    const instruction_entry_t* entry = find_instruction(parsed_inst->mnemonic);
    if (!entry) {
        fprintf(stderr, "erro (linha %u): mnemonico desconhecido '%s'.\n", parsed_inst->line_number, parsed_inst->mnemonic);
        return ENCODING_ERROR_SENTINEL;
    }

    encoded_instruction_t encoded_inst;
    encoded_inst.type = entry->type;
    encoded_inst.fields.word = 0;

    int rd = 0, rs1 = 0, rs2 = 0;
    int32_t imm_val = 0;
    bool imm_success = true;

    // aqui é onde a mágica acontece, dependendo do tipo, ele organiza os dados de cada forma
    switch (entry->type) {
        case INST_R:
            if (parsed_inst->operand_count != 3) {
                fprintf(stderr, "erro (linha %u): instrução '%s' (R-type) requer 3 operandos.\n", parsed_inst->line_number, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }
            rd  = get_register_number(parsed_inst->operands[0]);
            rs1 = get_register_number(parsed_inst->operands[1]);
            rs2 = get_register_number(parsed_inst->operands[2]);

            if (rd == -1 || rs1 == -1 || rs2 == -1) {
                fprintf(stderr, "erro (linha %u): Nome de registrador inválido para '%s'. rd:%s rs1:%s rs2:%s\n", 
                        parsed_inst->line_number, entry->mnemonic, parsed_inst->operands[0], parsed_inst->operands[1], parsed_inst->operands[2]);
                return ENCODING_ERROR_SENTINEL;
            }

            encoded_inst.fields.r.opcode = entry->opcode;
            encoded_inst.fields.r.rd     = (uint32_t)rd;
            encoded_inst.fields.r.funct3 = (uint32_t)entry->funct3;
            encoded_inst.fields.r.rs1    = (uint32_t)rs1;
            encoded_inst.fields.r.rs2    = (uint32_t)rs2;
            encoded_inst.fields.r.funct7 = (uint32_t)entry->funct7;
            break;

        case INST_I:
            encoded_inst.fields.i.opcode = entry->opcode;
            encoded_inst.fields.i.funct3 = (uint32_t)entry->funct3;

            // resolvendo particularidades de algumas instruções

            if (strcmp(entry->mnemonic, "lw") == 0 || strcmp(entry->mnemonic, "lb") == 0 || strcmp(entry->mnemonic, "lh") == 0 ||
                strcmp(entry->mnemonic, "lbu") == 0 || strcmp(entry->mnemonic, "lhu") == 0) { // lw rd, imm(rs1)
                if (parsed_inst->operand_count != 2) {
                    fprintf(stderr, "erro (linha %u): Instrução '%s' (I-type load) requer 2 operandos no formato rd, imm(rs1).\n", parsed_inst->line_number, entry->mnemonic);
                    return ENCODING_ERROR_SENTINEL;
                }
                rd = get_register_number(parsed_inst->operands[0]);

                // parse "imm(rs1)" (poderia ter feito no parser, mas esqueci que que tinha isso)
                char imm_str[32], rs1_str[8]; // buffers temporários
                if (sscanf(parsed_inst->operands[1], "%31[^()](%7[^)])", imm_str, rs1_str) != 2) {
                     fprintf(stderr, "erro (linha %u): formato de operando invalido para '%s'. esperado 'imm(rs1)', recebido '%s'.\n", parsed_inst->line_number, entry->mnemonic, parsed_inst->operands[1]);
                     return ENCODING_ERROR_SENTINEL;
                }
                rs1 = get_register_number(rs1_str);
                imm_val = parse_immediate(imm_str, &imm_success);

            } else if (strcmp(entry->mnemonic, "slli") == 0 || strcmp(entry->mnemonic, "srli") == 0 || strcmp(entry->mnemonic, "srai") == 0 ) { 
                 if (parsed_inst->operand_count != 3) {
                    fprintf(stderr, "erro (linha %u): Instrucao '%s' (I-type shift) requer 3 operandos.\n", parsed_inst->line_number, entry->mnemonic);
                    return ENCODING_ERROR_SENTINEL;
                }
                rd = get_register_number(parsed_inst->operands[0]);
                rs1 = get_register_number(parsed_inst->operands[1]);
                imm_val = parse_immediate(parsed_inst->operands[2], &imm_success); // shamt
                if (imm_success && (imm_val < 0 || imm_val > 0x1F)) { // shamt é de 5 bits (0-31) para RV32I
                    fprintf(stderr, "Erro (linha %u): Valor de shamt '%s' fora do range (0-31) para '%s'.\n", parsed_inst->line_number, parsed_inst->operands[2], entry->mnemonic);
                    imm_success = false;
                }

                // para shifts I-type, o campo 'imm' de 12 bits é construído:
                // imm[11:5] é o funct7 da tabela (0b0000000 para slli/srli, 0b0100000 para srai)
                // imm[4:0] é o shamt
                // a tabela já tem funct7 = 0 para slli e srli. Precisaremos de SRAI.

                uint32_t shamt_bits = (uint32_t)imm_val & 0x1F;
                uint32_t funct7_bits_for_shift = (uint32_t)entry->funct7 & 0x7F; // funct7 é 7 bits
                if (strcmp(entry->mnemonic, "srai") == 0) funct7_bits_for_shift = 0b0100000; // hardcode SRAI(não tem na tabela de max)

                imm_val = (int32_t)((funct7_bits_for_shift << 5) | shamt_bits);
            } else {

                if (parsed_inst->operand_count < 2 || parsed_inst->operand_count > 3) { // JALR pode ter 2 ou 3
                    fprintf(stderr, "erro (linha %u): instrucao '%s' (I-type) numero de operandos invalido.\n", parsed_inst->line_number, entry->mnemonic);
                    return ENCODING_ERROR_SENTINEL;
                }

                rd = get_register_number(parsed_inst->operands[0]);
                if (strcmp(entry->mnemonic, "jalr") == 0) {

                    if (parsed_inst->operand_count == 2) { // jalr rd, rs1  (imm é 0)
                        rs1 = get_register_number(parsed_inst->operands[1]);
                        imm_val = 0; // imm é opcional e default 0 para JALR sem ele
                    } else { // jalr rd, rs1, imm  OU jalr rd, imm(rs1)

                        // checar formato imm(rs1) para JALR
                        char imm_str_jalr[32], rs1_str_jalr[8];
                        if (sscanf(parsed_inst->operands[1], "%31[^()](%7[^)])", imm_str_jalr, rs1_str_jalr) == 2) {
                            // formato jalr rd, imm(rs1)
                            rs1 = get_register_number(rs1_str_jalr);
                            imm_val = parse_immediate(imm_str_jalr, &imm_success);
                        } else { // formato jalr rd, rs1, imm
                             rs1 = get_register_number(parsed_inst->operands[1]);
                             imm_val = parse_immediate(parsed_inst->operands[2], &imm_success);
                        }
                    }
                } else { // addi
                    if (parsed_inst->operand_count != 3) {
                         fprintf(stderr, "erro (linha %u): instrução '%s' (I-type arith/logic) requer 3 operandos.\n", parsed_inst->line_number, entry->mnemonic);
                         return ENCODING_ERROR_SENTINEL;
                    }
                    rs1 = get_register_number(parsed_inst->operands[1]);
                    imm_val = parse_immediate(parsed_inst->operands[2], &imm_success);
                }
            }
            
            if (rd == -1 || rs1 == -1 || !imm_success) {
                fprintf(stderr, "erro (linha %u): operando invalido para '%s'. rd:%d rs1:%d imm_ok:%d\n", parsed_inst->line_number, entry->mnemonic, rd, rs1, imm_success);
                return ENCODING_ERROR_SENTINEL;
            }
            // checagem de range do imediato de 12 bits
            if (imm_val < -2048 || imm_val > 2047) {
                 // para slli/srli/srai, imm_val já contém funct7 e shamt, não é um imediato de 12 bits puro
                if (!(strcmp(entry->mnemonic, "slli") == 0 || strcmp(entry->mnemonic, "srli") == 0 || strcmp(entry->mnemonic, "srai") == 0)) {
                    fprintf(stderr, "Erro (linha %u): Imediato '%s' fora do range (-2048 a 2047) para '%s'.\n", parsed_inst->line_number, parsed_inst->operands[parsed_inst->operand_count-1], entry->mnemonic);
                    return ENCODING_ERROR_SENTINEL;
                }
            }

            encoded_inst.fields.i.rd  = (uint32_t)rd;
            encoded_inst.fields.i.rs1 = (uint32_t)rs1;
            encoded_inst.fields.i.imm = (int32_t)imm_val; // imediato de 12 bits (com sinal)
            break;

        case INST_S: // sw rs2, imm(rs1)
            if (parsed_inst->operand_count != 2) {
                fprintf(stderr, "erro (linha %u): instrucao '%s' (S-type) requer 2 operandos no formato rs2, imm(rs1).\n", parsed_inst->line_number, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }
            rs2 = get_register_number(parsed_inst->operands[0]);
            
            char imm_s_str[32], rs1_s_str[8];
            if (sscanf(parsed_inst->operands[1], "%31[^()](%7[^)])", imm_s_str, rs1_s_str) != 2) {
                 fprintf(stderr, "erro (linha %u): formato de operando invalido para '%s'. esperado 'imm(rs1)', recebido '%s'.\n", parsed_inst->line_number, entry->mnemonic, parsed_inst->operands[1]);
                 return ENCODING_ERROR_SENTINEL;
            }
            rs1 = get_register_number(rs1_s_str);
            imm_val = parse_immediate(imm_s_str, &imm_success);

            if (rs1 == -1 || rs2 == -1 || !imm_success) {
                fprintf(stderr, "erro (linha %u): operando invalido para '%s'.\n", parsed_inst->line_number, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }
            if (imm_val < -2048 || imm_val > 2047) {
                fprintf(stderr, "erro (linha %u): imediato '%s' fora do range (-2048 a 2047) para '%s'.\n", parsed_inst->line_number, imm_s_str, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }

            encoded_inst.fields.s.opcode  = entry->opcode;
            encoded_inst.fields.s.funct3  = (uint32_t)entry->funct3;
            encoded_inst.fields.s.rs1     = (uint32_t)rs1;
            encoded_inst.fields.s.rs2     = (uint32_t)rs2;
            encoded_inst.fields.s.imm4_0  = (uint32_t)imm_val & 0x1F;       // imm[4:0]
            encoded_inst.fields.s.imm11_5 = (uint32_t)(imm_val >> 5) & 0x7F; // imm[11:5]
            break;

        case INST_B: // beq rs1, rs2, label
            if (parsed_inst->operand_count != 3) {
                fprintf(stderr, "erro (linha %u): instrucao '%s' (B-type) requer 3 operandos.\n", parsed_inst->line_number, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }
            rs1 = get_register_number(parsed_inst->operands[0]);
            rs2 = get_register_number(parsed_inst->operands[1]);
            
            int32_t target_addr_b = symbol_table_lookup(symbols, parsed_inst->operands[2]);
            if (target_addr_b == -1 && strcmp(parsed_inst->operands[2], "-1") != 0) { // checa se label não foi encontrado
                
                // tentar fazer o parser como imediato (offset direto)
                imm_val = parse_immediate(parsed_inst->operands[2], &imm_success);
                if (!imm_success) {
                    fprintf(stderr, "erro (linha %u): label '%s' nao encontrado e nao e um offset valido para '%s'.\n", parsed_inst->line_number, parsed_inst->operands[2], entry->mnemonic);
                    return ENCODING_ERROR_SENTINEL;
                }
                // se for um offset direto, ele já é o imm_val
            } else {
                 imm_val = target_addr_b - (int32_t)current_address; // offset relativo ao PC
            }


            if (rs1 == -1 || rs2 == -1) {
                fprintf(stderr, "erro (linha %u): registrador invalido para '%s'.\n", parsed_inst->line_number, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }
            
            if (imm_val < -4096 || imm_val > 4094 || (imm_val % 2 != 0)) {
                fprintf(stderr, "erro (linha %u): offset de branch '%s' (valor %d) fora do range ou nao e multiplo de 2 para '%s'.\n", parsed_inst->line_number, parsed_inst->operands[2], imm_val, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }

            encoded_inst.fields.b.opcode  = entry->opcode;
            encoded_inst.fields.b.funct3  = (uint32_t)entry->funct3;
            encoded_inst.fields.b.rs1     = (uint32_t)rs1;
            encoded_inst.fields.b.rs2     = (uint32_t)rs2;
            encoded_inst.fields.b.imm4_1  = (uint32_t)(imm_val >> 1) & 0xF;   // imm[4:1]
            encoded_inst.fields.b.imm10_5 = (uint32_t)(imm_val >> 5) & 0x3F;  // imm[10:5]
            encoded_inst.fields.b.imm11   = (uint32_t)(imm_val >> 11) & 0x1;  // imm[11]
            encoded_inst.fields.b.imm12   = (uint32_t)(imm_val >> 12) & 0x1;  // imm[12] (bit de sinal)
            break;

        case INST_U: // lui rd, imm
            if (parsed_inst->operand_count != 2) {
                fprintf(stderr, "erro (linha %u): instrucao '%s' (U-type) requer 2 operandos.\n", parsed_inst->line_number, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }
            rd = get_register_number(parsed_inst->operands[0]);
            imm_val = parse_immediate(parsed_inst->operands[1], &imm_success);

            if (rd == -1 || !imm_success) {
                fprintf(stderr, "erro (linha %u): operando invalido para '%s'.\n", parsed_inst->line_number, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }
            
            if (imm_val < 0 || imm_val > 0xFFFFF) { 
                fprintf(stderr, "erro (linha %u): imediato U-type '%s' fora do range (0 a 0xFFFFF).\n", parsed_inst->line_number, parsed_inst->operands[1], entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }
            
            encoded_inst.fields.u.opcode = entry->opcode;
            encoded_inst.fields.u.rd     = (uint32_t)rd;
            encoded_inst.fields.u.imm    = (uint32_t)imm_val;
            break;

        case INST_J: // jal rd, label  (ou jal label, que é jal ra, label)
            if (parsed_inst->operand_count < 1 || parsed_inst->operand_count > 2) {
                fprintf(stderr, "Erro (linha %u): Instrução '%s' (J-type) requer 1 ou 2 operandos.\n", parsed_inst->line_number, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }

            const char* label_str_j;

            if (parsed_inst->operand_count == 2) {
                rd = get_register_number(parsed_inst->operands[0]);
                label_str_j = parsed_inst->operands[1];
            } else { // jal label -> rd = ra (x1)
                rd = 1; // ra
                label_str_j = parsed_inst->operands[0];
            }

            int32_t target_addr_j = symbol_table_lookup(symbols, label_str_j);
             if (target_addr_j == -1 && strcmp(label_str_j, "-1") != 0) {
                imm_val = parse_immediate(label_str_j, &imm_success);
                if (!imm_success) {
                    fprintf(stderr, "erro (linha %u): label '%s' nao encontrado e nao e um offset valido para '%s'.\n", parsed_inst->line_number, label_str_j, entry->mnemonic);
                    return ENCODING_ERROR_SENTINEL;
                }
            } else {
                imm_val = target_addr_j - (int32_t)current_address;
            }

            if (rd == -1) {
                fprintf(stderr, "Eerro (linha %u): registrador invalido para '%s'.\n", parsed_inst->line_number, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }

            if (imm_val < -1048576 || imm_val > 1048574 || (imm_val % 2 != 0)) {
                fprintf(stderr, "erro (linha %u): offset de jump '%s' (valor %d) fora do range ou nao e multiplo de 2 para '%s'.\n", parsed_inst->line_number, label_str_j, imm_val, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }

            encoded_inst.fields.j.opcode   = entry->opcode;
            encoded_inst.fields.j.rd       = (uint32_t)rd;
            encoded_inst.fields.j.imm10_1  = (uint32_t)(imm_val >> 1) & 0x3FF;  // imm[10:1]
            encoded_inst.fields.j.imm11    = (uint32_t)(imm_val >> 11) & 0x1;   // imm[11]
            encoded_inst.fields.j.imm19_12 = (uint32_t)(imm_val >> 12) & 0xFF;  // imm[19:12]
            encoded_inst.fields.j.imm20    = (uint32_t)(imm_val >> 20) & 0x1;   // imm[20] (bit de sinal do offset)
            break;
            
        default:
            fprintf(stderr, "erro (linha %u): tipo de instrucao desconhecido ou nao suportado '%d' para '%s'.\n", parsed_inst->line_number, entry->type, entry->mnemonic);
            return ENCODING_ERROR_SENTINEL;
    }

    return encoded_inst.fields.word;
}

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

    char* comment_ptr = strchr(buffer, '#');
    if (comment_ptr)
        *comment_ptr = '\0';
    rtrim(buffer);

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
static inline instruction_t* parse_lines(char** lines, size_t line_count, size_t* out_count, symbol_table_t* table) {
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



int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "uso: %s <arquivo_assembly.asm> [arquivo_saida.mif]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // arquivos
    const char* input_filename = argv[1];
    char output_mif_filename[256];

    // para o caso que o arquivo é passado como parametro ou nao
    if (argc == 3) {
        strncpy(output_mif_filename, argv[2], sizeof(output_mif_filename) - 1);
        output_mif_filename[sizeof(output_mif_filename) - 1] = '\0';
    } else {
        strcpy(output_mif_filename, "memoria.mif");
    }

    // começo da lógica

    size_t line_count = 0;
    char** lines = NULL;
    symbol_table_t sym_table;
    instruction_t* instructions = NULL;
    size_t instruction_arr_count = 0;
    FILE* mif_file = NULL;

    // lê os arquivos e transforma em um vetor de strings com as linhas
    lines = read_file_lines(input_filename, &line_count);
    if (!lines) {
        fprintf(stderr, "erro: nao foi possivel ler o arquivo '%s'.\n", input_filename);
        return EXIT_FAILURE;
    }

    // inicializa a estrutura de dados que vai armazenas os simbolos
    symbol_table_init(&sym_table);

    // faz o parser das linhas (talvez eu deveria ter feito um tokenizer, mas n sei bem onde)
    // aqui gera uma lista (vetor) de instruções
    instructions = parse_lines(lines, line_count, &instruction_arr_count, &sym_table);
    
    // verificação para caso as instruções dê errado 
    // (talvez trocar essas coisas repetitivas por macros depois)
    if (!instructions) {
        fprintf(stderr, "erro durante o parsing das linhas.\n");
        symbol_table_free(&sym_table);
        if (lines) {
            for(size_t i=0; i<line_count; ++i) 
                if(lines[i]) free(lines[i]);
            free(lines);
        }
        return EXIT_FAILURE;
    }

    // finalmente abre o mif para a saida em modo de escrita
    mif_file = fopen(output_mif_filename, "w");

    // caso dê errado libera tudo
    if (!mif_file) {
        fprintf(stderr, "erro: nao foi possivel abrir o arquivo de saida mif '%s'.\n", output_mif_filename);
        if (lines) {
            for(size_t i=0; i<line_count; ++i)
                if(lines[i]) free(lines[i]);
            free(lines);
        }
        if (instructions)
            free_instructions(instructions, instruction_arr_count);
        symbol_table_free(&sym_table);
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < instruction_arr_count; ++i) {
        uint32_t current_instr_address = instructions[i].address;

        // faz o encode da instrução que está agora
        uint32_t machine_code = encode_instruction(&instructions[i], &sym_table, current_instr_address);

        // caso consiga gerar a instrução, faz o print e converte paras jogar no mif
        if (machine_code != ENCODING_ERROR_SENTINEL) {
            
            char binary_string[33];
            for (int bit_pos = 0; bit_pos < 32; ++bit_pos) {
                if ((machine_code >> (31 - bit_pos)) & 1) {
                    binary_string[bit_pos] = '1';
                } else {
                    binary_string[bit_pos] = '0';
                }
            }
            binary_string[32] = '\0';

            //ninho de rato com defines para poder verificar respostas no rars
            #if MIF_OUTPUT_GRANULARITY == 32

            fprintf(mif_file, "%s\n", binary_string);

            #elif MIF_OUTPUT_GRANULARITY == 8
                #if MIF_PRINT_BYTES_BIG_ENDIAN == 1

            fprintf(mif_file, "%.8s\n", &binary_string[0]);  // MSB byte (bits 31-24)
            fprintf(mif_file, "%.8s\n", &binary_string[8]);  // Byte 2   (bits 23-16)
            fprintf(mif_file, "%.8s\n", &binary_string[16]); // Byte 1   (bits 15-8)
            fprintf(mif_file, "%.8s\n", &binary_string[24]); // LSB byte (bits 7-0)

                #else // LSB byte primeiro

            fprintf(mif_file, "%.8s\n", &binary_string[24]); // LSB byte (bits 7-0)
            fprintf(mif_file, "%.8s\n", &binary_string[16]); // Byte 1   (bits 15-8)
            fprintf(mif_file, "%.8s\n", &binary_string[8]);  // Byte 2   (bits 23-16)
            fprintf(mif_file, "%.8s\n", &binary_string[0]);  // MSB byte (bits 31-24)

                #endif
            #else

            fprintf(mif_file, "%s\n", binary_string); 

            #endif
        } else {
            printf("0x%08x | erro encoding       | %s\n", current_instr_address, instructions[i].mnemonic);
            
            #if MIF_OUTPUT_GRANULARITY == 32

            fprintf(mif_file, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");

            #elif MIF_OUTPUT_GRANULARITY == 8

            fprintf(mif_file, "XXXXXXXX\n");
            fprintf(mif_file, "XXXXXXXX\n");
            fprintf(mif_file, "XXXXXXXX\n");
            fprintf(mif_file, "XXXXXXXX\n");

            #else

            fprintf(mif_file, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
            #endif

        }
    } 
    fclose(mif_file);

    // liberando a memoria alocada
    
    if (lines) {
        for(size_t i=0; i<line_count; ++i)
            if(lines[i]) free(lines[i]);
        free(lines);
    }

    if (instructions) 
        free_instructions(instructions, instruction_arr_count);
    symbol_table_free(&sym_table);

    return EXIT_SUCCESS;
}