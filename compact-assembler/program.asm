start:
    # U-Type
    lui t0, 0x10000       # t0 = 0x10000000 (EndereÃ§o base)

    # I-Type (Arith/Logic)
    addi t1, zero, 100     # t1 = 100
    addi t2, zero, 50      # t2 = 50
    slli t1, t1, 1         # t1 = 100 << 1 = 200
    srli t2, t2, 1         # t2 = 50 >> 1 = 25

    # S-Type e I-Type (Memory)
    sw t1, 0(t0)           # Salva 200 em 0x10000000
    lw t3, 0(t0)           # Carrega de 0x10000000 para t3 (t3 = 200)

    # R-Type
    add t4, t3, t2         # t4 = 200 + 25 = 225
    sub t5, t3, t2         # t5 = 200 - 25 = 175
    xor t6, t3, t3         # t6 = 200 ^ 200 = 0
    or a0, t3, t6          # a0 = 200 | 0 = 200
    and a1, t3, t6         # a1 = 200 & 0 = 0

    # B-Type
    beq t6, a1, label_eq   # 0 == 0 -> Salta para label_eq
    addi a0, a0, 99        # NÃ£o executa

label_eq:
    bne t4, t5, label_ne   # 225 != 175 -> Salta para label_ne
    addi a1, a1, 99        # NÃ£o executa

label_ne:
    # J-Type
    jal ra, sub_routine    # Salta para sub_routine, ra = PC + 4

    # I-Type (JALR) - Retorna da sub_routine para cÃ¡
    addi a0, a0, 1         # a0 = 201

end_loop:
    # Loop infinito para terminar
    beq zero, zero, end_loop # Salta para si mesmo

sub_routine:
    addi t0, t0, 4         # Apenas uma instruÃ§Ã£o de exemplo
    jalr x1, x1, 0       # Retorna para o endereÃ§o em 'ra'
