#include "Wrappers.h"

.text
.align 2
.globl dyna_run

/* dyna_run(PowerPC_func* func, unsigned int (*code)(void))
   r3 = puntero a la estructura func
   r4 = puntero al código ejecutable
*/
dyna_run:
    # Creamos el stack frame (estándar PS3)
    # Guardamos todos los registros no volátiles (r14-r31)
    stdu  1, -160(1)
    mflr  0
    std   0, 176(1)
    
    std   14, 48(1)
    std   15, 56(1)
    std   16, 64(1)
    std   17, 72(1)
    std   18, 80(1)
    std   19, 88(1)
    std   20, 96(1)
    std   21, 104(1)
    std   22, 112(1)
    std   23, 120(1)
    std   24, 128(1)
    std   25, 136(1)
    std   26, 144(1)
    std   27, 152(1)
    std   28, 160(1)
    std   29, 168(1)
    std   30, 176(1)
    std   31, 184(1)

    # Preparamos los registros de mapeo para el código recompilado
    # Estos punteros deben estar listos antes de saltar
    mr    22, 3        # DYNAREG_FUNC = func
    mtctr r4           # Cargamos la dirección del código en el CTR

    # Saltamos al código recompilado
    bctrl

    # Al volver, r3 contiene la dirección del siguiente PC (naddr)
    # Restauramos todo
    ld    14, 48(1)
    ld    15, 56(1)
    ld    16, 64(1)
    ld    17, 72(1)
    ld    18, 80(1)
    ld    19, 88(1)
    ld    20, 96(1)
    ld    21, 104(1)
    ld    22, 112(1)
    ld    23, 120(1)
    ld    24, 128(1)
    ld    25, 136(1)
    ld    26, 144(1)
    ld    27, 152(1)
    ld    28, 160(1)
    ld    29, 168(1)
    ld    30, 176(1)
    ld    31, 184(1)
    
    ld    0, 176(1)
    mtlr  0
    addi  1, 1, 160
    blr