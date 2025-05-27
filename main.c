#include "include/utils.h"
#include "include/parser.h"
#include "include/encoder.h"
#include "include/symbol_table.h"
#include "include/encoding_table.h"

// 1 para byte mais significativo primeiro
// 0 para byte menos significativo primeiro 
// (quando MIF_OUTPUT_GRANULARITY == 8)
#define MIF_PRINT_BYTES_BIG_ENDIAN 0

// 32 para a palavra inteira, 8 para imprimir de 8 em 8 bits
#define MIF_OUTPUT_GRANULARITY 32 

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

    // print para debug
    printf("--- iniciando segunda passagem (codificacao) ---\n");
    printf("endereco   | codigo maq. (hex) | mnemonico\n");
    printf("--------------------------------------------------\n");

    for (size_t i = 0; i < instruction_arr_count; ++i) {
        uint32_t current_instr_address = instructions[i].address;

        // faz o encode da instrução que está agora
        uint32_t machine_code = encode_instruction(&instructions[i], &sym_table, current_instr_address);

        // caso consiga gerar a instrução, faz o print e converte paras jogar no mif
        if (machine_code != ENCODING_ERROR_SENTINEL) {
            printf("0x%08x | 0x%08x        | %s\n", current_instr_address, machine_code, instructions[i].mnemonic);
            
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
    printf("--------------------------------------------------\n");
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