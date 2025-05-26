main:
    # lendo um valor inteiro do console
    addi    s0,    zero,  5
    # ajustando variaveis para a contagem e comparaÃ§Ã£o no for
    addi    t0,    zero,  1       # inicializando a variavel que serÃ¡ incrementada com 1
for:
    # estrutura que servirÃ¡ de base para o for
    beq     t0,    t1,    enter   # compara se o valor Ã© igual ao parametro, se for, salta para fora do for
    add     a0,    zero,  s0     # coloco o valor inteiro de $t0 dentro de $a0, para que ele seja posto no terminal
    addi    s0,    zero,  1       # coloco em $v0 o valor que representa a operaÃ§Ã£o de exibir um inteiro no terminal
    lui     a0,    0x1001          # atribuo o endereÃ§o de onde estÃ£o as minhas strings declaradas (estas sequencialmente em memoria) para $a0
    addi    s0,    zero,  4       # coloco em $v0 o valor que representa a operaÃ§Ã£o de exibir uma string no terminal
    addi    t0,    t0,    1       # incremento 1 a $t0
enter:
    #printar quebra de linha
    lui     a0,    0x1001          # atribuindo a $a0 o endereÃ§o de onde estÃ£o as strings
    addi    a0,    a0,    2       # deslocando para o byte 2, onde se encontra o caracter quebra de linha
    addi    s0,    zero,  4       # operaÃ§Ã£o de printar string
exit:
