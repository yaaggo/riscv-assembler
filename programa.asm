# codigo gerado pelo gpt para testar funcionalidade do programa
_start:
    # U-type: LUI
    lui x1, 0x12345       # x1 = 0x12345000

    # J-type: JAL
    jal x2, label_jal     # x2 = endere�o de retorno (pr�xima instru��o)

label_jal:
    # I-type: JALR
    addi x3, x0, 4
    jalr x4, x3, 0        # x4 = PC+4, salta para endere�o em x3 (4)

    # B-type: BEQ e BNE
    addi x5, x0, 1
    addi x6, x0, 1
    beq x5, x6, label_beq # salta se x5 == x6

    addi x7, x0, 0        # esta instru��o ser� pulada se BEQ for tomada

label_beq:
    addi x8, x0, 2
    bne x5, x8, label_bne # salta se x5 != x8

    addi x9, x0, 0        # esta instru��o ser� pulada se BNE for tomada

label_bne:
    # I-type: LW
    lui x10, 0x10000
    sw x5, 0(x10)         # salva x5 na mem�ria
    lw x11, 0(x10)        # carrega de volta em x11

    # S-type: SW j� testado acima

    # I-type: ADDI
    addi x12, x0, 10      # x12 = 10

    # I-type: SLLI, SRLI
    slli x13, x12, 1      # x13 = x12 << 1 = 20
    srli x14, x13, 2      # x14 = x13 >> 2 = 5

    # R-type: ADD, SUB, XOR, OR, AND
    add x15, x5, x6       # x15 = 1 + 1 = 2
    sub x16, x15, x5      # x16 = 2 - 1 = 1
    xor x17, x15, x16     # x17 = 2 ^ 1 = 3
    or  x18, x17, x5      # x18 = 3 | 1 = 3
    and x19, x18, x6      # x19 = 3 & 1 = 1

    # Final: loop infinito
end:
    jal x0, end
