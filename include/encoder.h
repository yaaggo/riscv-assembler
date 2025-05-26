#ifndef ENCODER_H
#define ENCODER_H

#include <stdbool.h>

#include "types.h"
#include "encoding_table.h"
#include "symbol_table.h"

#define ENCODING_ERROR_SENTINEL 0xFFFFFFFF 

// --- Funções Auxiliares Estáticas ---

/**
 * @brief Converte uma string de imediato (decimal ou hexadecimal) para um valor int32_t.
 * Suporta "0x" para hexadecimais.
 * @param imm_str String do imediato.
 * @param success Ponteiro para bool, setado para true se sucesso, false se erro.
 * @return int32_t Valor do imediato.
 */
static inline int32_t parse_immediate(const char* imm_str, bool* success) {
    if (imm_str == NULL) {
        *success = false;
        return 0;
    }
    char* endptr;
    long val = strtol(imm_str, &endptr, 0); // Base 0 auto-detecta 0x para hex

    if (endptr == imm_str || *endptr != '\0') { // Nenhum dígito foi lido ou string não consumida totalmente
        *success = false;
        return 0;
    }
    *success = true;
    return (int32_t)val;
}

// --- Função Principal de Codificação ---

/**
 * @brief Codifica uma instrução parseada para seu formato binário de 32 bits.
 * * @param parsed_inst Ponteiro para a instrução parseada (de parser.h).
 * @param symbols Ponteiro para a tabela de símbolos (para resolver labels).
 * @param current_address O endereço atual da instrução sendo codificada (PC).
 * @return uint32_t O código de máquina de 32 bits ou ENCODING_ERROR_SENTINEL em caso de erro.
 */
static inline uint32_t encode_instruction(const instruction_t* parsed_inst, 
                                          const symbol_table_t* symbols, 
                                          uint32_t current_address) {
    if (!parsed_inst) return ENCODING_ERROR_SENTINEL;

    const instruction_entry_t* entry = find_instruction(parsed_inst->mnemonic);
    if (!entry) {
        fprintf(stderr, "Erro (linha %u): Mnemônico desconhecido '%s'.\n", parsed_inst->line_number, parsed_inst->mnemonic);
        return ENCODING_ERROR_SENTINEL;
    }

    encoded_instruction_t encoded_inst; // encoded_instruction_t de types.h
    encoded_inst.type = entry->type;
    encoded_inst.fields.word = 0; // Importante: inicializar para evitar bits aleatórios. (Assumindo que encoded_fields_t tem um membro 'word')

    int rd = 0, rs1 = 0, rs2 = 0;
    int32_t imm_val = 0;
    bool imm_success = true;

    // Pré-processar operandos comuns (registros)
    // Validação do número de operandos é crucial aqui, caso a caso.

    switch (entry->type) {
        case INST_R: // add rd, rs1, rs2
            if (parsed_inst->operand_count != 3) {
                fprintf(stderr, "Erro (linha %u): Instrução '%s' (R-type) requer 3 operandos.\n", parsed_inst->line_number, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }
            rd  = get_register_number(parsed_inst->operands[0]);
            rs1 = get_register_number(parsed_inst->operands[1]);
            rs2 = get_register_number(parsed_inst->operands[2]);

            if (rd == -1 || rs1 == -1 || rs2 == -1) {
                fprintf(stderr, "Erro (linha %u): Nome de registrador inválido para '%s'. rd:%s rs1:%s rs2:%s\n", 
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

        case INST_I: // addi rd, rs1, imm; lw rd, imm(rs1); jalr rd, rs1, imm; slli rd, rs1, shamt
            encoded_inst.fields.i.opcode = entry->opcode;
            encoded_inst.fields.i.funct3 = (uint32_t)entry->funct3;

            if (strcmp(entry->mnemonic, "lw") == 0 || strcmp(entry->mnemonic, "lb") == 0 || strcmp(entry->mnemonic, "lh") == 0 ||
                strcmp(entry->mnemonic, "lbu") == 0 || strcmp(entry->mnemonic, "lhu") == 0) { // lw rd, imm(rs1)
                if (parsed_inst->operand_count != 2) {
                    fprintf(stderr, "Erro (linha %u): Instrução '%s' (I-type load) requer 2 operandos no formato rd, imm(rs1).\n", parsed_inst->line_number, entry->mnemonic);
                    return ENCODING_ERROR_SENTINEL;
                }
                rd = get_register_number(parsed_inst->operands[0]);

                // Parse "imm(rs1)"
                char imm_str[32], rs1_str[8]; // Buffers temporários
                if (sscanf(parsed_inst->operands[1], "%31[^()](%7[^)])", imm_str, rs1_str) != 2) {
                     fprintf(stderr, "Erro (linha %u): Formato de operando inválido para '%s'. Esperado 'imm(rs1)', recebido '%s'.\n", parsed_inst->line_number, entry->mnemonic, parsed_inst->operands[1]);
                     return ENCODING_ERROR_SENTINEL;
                }
                rs1 = get_register_number(rs1_str);
                imm_val = parse_immediate(imm_str, &imm_success);

            } else if (strcmp(entry->mnemonic, "slli") == 0 || strcmp(entry->mnemonic, "srli") == 0 || strcmp(entry->mnemonic, "srai") == 0 ) { // slli rd, rs1, shamt
                 if (parsed_inst->operand_count != 3) {
                    fprintf(stderr, "Erro (linha %u): Instrução '%s' (I-type shift) requer 3 operandos.\n", parsed_inst->line_number, entry->mnemonic);
                    return ENCODING_ERROR_SENTINEL;
                }
                rd = get_register_number(parsed_inst->operands[0]);
                rs1 = get_register_number(parsed_inst->operands[1]);
                imm_val = parse_immediate(parsed_inst->operands[2], &imm_success); // shamt
                if (imm_success && (imm_val < 0 || imm_val > 0x1F)) { // shamt é de 5 bits (0-31) para RV32I
                    fprintf(stderr, "Erro (linha %u): Valor de shamt '%s' fora do range (0-31) para '%s'.\n", parsed_inst->line_number, parsed_inst->operands[2], entry->mnemonic);
                    imm_success = false;
                }
                // Para shifts I-type, o campo 'imm' de 12 bits é construído:
                // imm[11:5] é o funct7 da tabela (0b0000000 para slli/srli, 0b0100000 para srai)
                // imm[4:0] é o shamt
                // A tabela já tem funct7 = 0 para slli e srli. Precisaremos de SRAI.
                // Se o seu 'types.h'->encoded_fields_t.i.imm é um campo único de 12 bits:
                uint32_t shamt_bits = (uint32_t)imm_val & 0x1F;
                uint32_t funct7_bits_for_shift = (uint32_t)entry->funct7 & 0x7F; // funct7 é 7 bits
                if (strcmp(entry->mnemonic, "srai") == 0) funct7_bits_for_shift = 0b0100000; // Hardcode SRAI se não estiver na tabela

                imm_val = (int32_t)((funct7_bits_for_shift << 5) | shamt_bits);
                // Note: A tabela 'instruction_entry_t' para 'slli' tem funct7=0b0000000.
                // Se a entrada da tabela para 'slli' tiver funct7 = -1, então o shamt vai direto pro imm.
                // A tabela fornecida tem 'slli' funct7=0b0000000, o que é correto para RV32I.
                // Isso significa que o campo rs2 (shamt) já está coberto.
            } else { // addi rd, rs1, imm; jalr rd, rs1, imm (ou rd, imm(rs1))
                if (parsed_inst->operand_count < 2 || parsed_inst->operand_count > 3) { // JALR pode ter 2 ou 3
                    fprintf(stderr, "Erro (linha %u): Instrução '%s' (I-type) número de operandos inválido.\n", parsed_inst->line_number, entry->mnemonic);
                    return ENCODING_ERROR_SENTINEL;
                }
                rd = get_register_number(parsed_inst->operands[0]);
                if (strcmp(entry->mnemonic, "jalr") == 0) {
                    if (parsed_inst->operand_count == 2) { // jalr rd, rs1  (imm é 0)
                        rs1 = get_register_number(parsed_inst->operands[1]);
                        imm_val = 0; // imm é opcional e default 0 para JALR sem ele
                    } else { // jalr rd, rs1, imm  OU jalr rd, imm(rs1)
                        // Checar formato imm(rs1) para JALR
                        char imm_str_jalr[32], rs1_str_jalr[8];
                        if (sscanf(parsed_inst->operands[1], "%31[^()](%7[^)])", imm_str_jalr, rs1_str_jalr) == 2) {
                            // Formato jalr rd, imm(rs1)
                            rs1 = get_register_number(rs1_str_jalr);
                            imm_val = parse_immediate(imm_str_jalr, &imm_success);
                        } else { // Formato jalr rd, rs1, imm
                             rs1 = get_register_number(parsed_inst->operands[1]);
                             imm_val = parse_immediate(parsed_inst->operands[2], &imm_success);
                        }
                    }
                } else { // addi, xori, etc.
                    if (parsed_inst->operand_count != 3) {
                         fprintf(stderr, "Erro (linha %u): Instrução '%s' (I-type arith/logic) requer 3 operandos.\n", parsed_inst->line_number, entry->mnemonic);
                         return ENCODING_ERROR_SENTINEL;
                    }
                    rs1 = get_register_number(parsed_inst->operands[1]);
                    imm_val = parse_immediate(parsed_inst->operands[2], &imm_success);
                }
            }
            
            if (rd == -1 || rs1 == -1 || !imm_success) {
                fprintf(stderr, "Erro (linha %u): Operando inválido para '%s'. rd:%d rs1:%d imm_ok:%d\n", parsed_inst->line_number, entry->mnemonic, rd, rs1, imm_success);
                return ENCODING_ERROR_SENTINEL;
            }
            // Checagem de range do imediato de 12 bits (-2048 a 2047)
            if (imm_val < -2048 || imm_val > 2047) {
                 // Exceção: para slli/srli/srai, imm_val já contém funct7 e shamt, não é um imediato de 12 bits puro.
                if (!(strcmp(entry->mnemonic, "slli") == 0 || strcmp(entry->mnemonic, "srli") == 0 || strcmp(entry->mnemonic, "srai") == 0)) {
                    fprintf(stderr, "Erro (linha %u): Imediato '%s' fora do range (-2048 a 2047) para '%s'.\n", parsed_inst->line_number, parsed_inst->operands[parsed_inst->operand_count-1], entry->mnemonic);
                    return ENCODING_ERROR_SENTINEL;
                }
            }

            encoded_inst.fields.i.rd  = (uint32_t)rd;
            encoded_inst.fields.i.rs1 = (uint32_t)rs1;
            encoded_inst.fields.i.imm = (int32_t)imm_val; // Imediato de 12 bits (com sinal)
            break;

        case INST_S: // sw rs2, imm(rs1)
            if (parsed_inst->operand_count != 2) {
                fprintf(stderr, "Erro (linha %u): Instrução '%s' (S-type) requer 2 operandos no formato rs2, imm(rs1).\n", parsed_inst->line_number, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }
            rs2 = get_register_number(parsed_inst->operands[0]);
            
            char imm_s_str[32], rs1_s_str[8];
            if (sscanf(parsed_inst->operands[1], "%31[^()](%7[^)])", imm_s_str, rs1_s_str) != 2) {
                 fprintf(stderr, "Erro (linha %u): Formato de operando inválido para '%s'. Esperado 'imm(rs1)', recebido '%s'.\n", parsed_inst->line_number, entry->mnemonic, parsed_inst->operands[1]);
                 return ENCODING_ERROR_SENTINEL;
            }
            rs1 = get_register_number(rs1_s_str);
            imm_val = parse_immediate(imm_s_str, &imm_success);

            if (rs1 == -1 || rs2 == -1 || !imm_success) {
                fprintf(stderr, "Erro (linha %u): Operando inválido para '%s'.\n", parsed_inst->line_number, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }
            if (imm_val < -2048 || imm_val > 2047) {
                fprintf(stderr, "Erro (linha %u): Imediato '%s' fora do range (-2048 a 2047) para '%s'.\n", parsed_inst->line_number, imm_s_str, entry->mnemonic);
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
                fprintf(stderr, "Erro (linha %u): Instrução '%s' (B-type) requer 3 operandos.\n", parsed_inst->line_number, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }
            rs1 = get_register_number(parsed_inst->operands[0]);
            rs2 = get_register_number(parsed_inst->operands[1]);
            
            int32_t target_addr_b = symbol_table_lookup(symbols, parsed_inst->operands[2]);
            if (target_addr_b == -1 && strcmp(parsed_inst->operands[2], "-1") != 0) { // Checa se label não foi encontrado
                // Tentar parsear como imediato (offset direto)
                imm_val = parse_immediate(parsed_inst->operands[2], &imm_success);
                if (!imm_success) {
                    fprintf(stderr, "Erro (linha %u): Label '%s' não encontrado e não é um offset válido para '%s'.\n", parsed_inst->line_number, parsed_inst->operands[2], entry->mnemonic);
                    return ENCODING_ERROR_SENTINEL;
                }
                // se for um offset direto, ele já é o imm_val
            } else {
                 imm_val = target_addr_b - (int32_t)current_address; // Offset relativo ao PC
            }


            if (rs1 == -1 || rs2 == -1) {
                fprintf(stderr, "Erro (linha %u): Registrador inválido para '%s'.\n", parsed_inst->line_number, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }
            // Offset do tipo B é de 13 bits com sinal (multiplo de 2), de -4096 a +4094
            if (imm_val < -4096 || imm_val > 4094 || (imm_val % 2 != 0)) {
                fprintf(stderr, "Erro (linha %u): Offset de branch '%s' (valor %d) fora do range ou não é múltiplo de 2 para '%s'.\n", parsed_inst->line_number, parsed_inst->operands[2], imm_val, entry->mnemonic);
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
                fprintf(stderr, "Erro (linha %u): Instrução '%s' (U-type) requer 2 operandos.\n", parsed_inst->line_number, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }
            rd = get_register_number(parsed_inst->operands[0]);
            imm_val = parse_immediate(parsed_inst->operands[1], &imm_success);

            if (rd == -1 || !imm_success) {
                fprintf(stderr, "Erro (linha %u): Operando inválido para '%s'.\n", parsed_inst->line_number, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }
            // Imediato U-type é de 20 bits (carregado nos bits 31-12 do registrador)
            // O valor do imediato na instrução é imm[31:12]
            // strtol resultará num valor. Precisamos pegar os 20 bits superiores se for para alinhar com 0xVVVVV000.
            // Ou, se o usuário escreve o valor que vai nos bits superiores. ex: lui t0, 0xDEADB (coloca 0xDEADB000 em t0)
            // A instrução lui rd, imm armazena imm << 12 em rd. O campo imm na instrução são os bits 31-12.
            // Então, se o usuário escreveu `lui a0, 0x12345`, o valor 0x12345 vai para o campo imm da instrução.
            if (imm_val < 0 || imm_val > 0xFFFFF) { // 20 bits (0 a 1048575)
                 // Se for AUIPC, o imediato pode ser interpretado como um offset com sinal, mas LUI é unsigned.
                // A especificação diz "LUI places the U-immediate value in the destination register rd,
                // filling the lowest 12 bits with zeros." O valor do U-immediate é 20 bits.
                fprintf(stderr, "Erro (linha %u): Imediato U-type '%s' fora do range (0 a 0xFFFFF).\n", parsed_inst->line_number, parsed_inst->operands[1], entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }
            
            encoded_inst.fields.u.opcode = entry->opcode;
            encoded_inst.fields.u.rd     = (uint32_t)rd;
            encoded_inst.fields.u.imm    = (uint32_t)imm_val; // Os 20 bits do valor do imediato
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
                    fprintf(stderr, "Erro (linha %u): Label '%s' não encontrado e não é um offset válido para '%s'.\n", parsed_inst->line_number, label_str_j, entry->mnemonic);
                    return ENCODING_ERROR_SENTINEL;
                }
            } else {
                imm_val = target_addr_j - (int32_t)current_address;
            }


            if (rd == -1) {
                fprintf(stderr, "Erro (linha %u): Registrador inválido para '%s'.\n", parsed_inst->line_number, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }
            // Offset J-type é de 21 bits com sinal (multiplo de 2), de -1MiB a +1MiB-2
            // imm_val representa o byte offset.
            if (imm_val < -1048576 || imm_val > 1048574 || (imm_val % 2 != 0)) {
                fprintf(stderr, "Erro (linha %u): Offset de jump '%s' (valor %d) fora do range ou não é múltiplo de 2 para '%s'.\n", parsed_inst->line_number, label_str_j, imm_val, entry->mnemonic);
                return ENCODING_ERROR_SENTINEL;
            }
            
            // O imediato da instrução J (20 bits) é o offset >> 1.
            // A especificação da união encoded_fields_t.j já espera os bits do offset de 20 bits (após >>1 e com sinal)
            // int32_t j_imm20 = imm_val; // O offset já é o que precisa ser codificado nos campos da instrução J.
                                      // As masks e shifts abaixo pegam os bits corretos do offset.

            encoded_inst.fields.j.opcode   = entry->opcode;
            encoded_inst.fields.j.rd       = (uint32_t)rd;
            encoded_inst.fields.j.imm10_1  = (uint32_t)(imm_val >> 1) & 0x3FF;  // imm[10:1]
            encoded_inst.fields.j.imm11    = (uint32_t)(imm_val >> 11) & 0x1;   // imm[11]
            encoded_inst.fields.j.imm19_12 = (uint32_t)(imm_val >> 12) & 0xFF;  // imm[19:12]
            encoded_inst.fields.j.imm20    = (uint32_t)(imm_val >> 20) & 0x1;   // imm[20] (bit de sinal do offset)
            break;
            
        default:
            fprintf(stderr, "Erro (linha %u): Tipo de instrução desconhecido ou não suportado '%d' para '%s'.\n", parsed_inst->line_number, entry->type, entry->mnemonic);
            return ENCODING_ERROR_SENTINEL;
    }

    // Assumindo que encoded_fields_t tem um membro `uint32_t word;`
    // Se não, você precisaria fazer: return *((uint32_t*)&encoded_inst.fields);
    return encoded_inst.fields.word;
}


#endif 