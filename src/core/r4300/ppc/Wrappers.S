#include "DynaWrappers.h"

.extern r4300
.extern rdram

.text
.align 2
.globl .dyna_run

/* dyna_run(PowerPC_func* func, unsigned int (*code)(void))
   r3 = puntero a la estructura func
   r4 = puntero al código ejecutable
*/
.dyna_run:
    # Creamos el stack frame (estándar PS3)
    # Guardamos todos los registros no volátiles (r14-r31)
    stdu  1, -256(1)
    mflr  0
    std   0, 272(1)
    
    std   14, 80(1)
    std   15, 88(1)
    std   16, 96(1)
    std   17, 104(1)
    std   18, 112(1)
    std   19, 120(1)
    std   20, 128(1)
    std   21, 136(1)
    std   22, 144(1)
    std   23, 152(1)
    std   24, 160(1)
    std   25, 168(1)
    std   26, 176(1)
    std   27, 184(1)
    std   28, 192(1)
    std   29, 200(1)
    std   30, 208(1)
    std   31, 216(1)

    # Inicializamos el entorno del Recompilador (Mapeo de registros MIPS a PowerPC)
    # Obtenemos la dirección base de la estructura r4300 y rdram desde el TOC (r2)
    ld    11, r4300@got(2)
    ld    19, rdram@got(2)    # r19 = DYNAREG_RDRAM

    addi  14, 11, 8           # r14 = DYNAREG_REG (&r4300.gpr[0])
    addi  15, 11, 296         # r15 = DYNAREG_COP0 (&r4300.reg_cop0[0])
    addi  16, 11, 936         # r16 = DYNAREG_FPR_32 (&r4300.fpr_single[0])
    addi  17, 11, 680         # r17 = DYNAREG_FPR_64 (&r4300.fpr_double[0])
    addi  18, 11, 1196        # r18 = DYNAREG_FCR31 (&r4300.fcr31)
    addi  20, 11, 4           # r20 = DYNAREG_LADDR (&r4300.last_pc)
    addi  21, 11, 1200        # r21 = DYNAREG_NINTR (&r4300.next_interrupt)
    mr    22, 3               # r22 = DYNAREG_FUNC (puntero a func)
    li    23, 0               # r23 = DYNAREG_ZERO (Constante 0)

    # Cargamos la dirección del código ejecutable y saltamos
    mtctr 4            

    # Saltamos al código recompilado
    bctrl

    # Al volver, r3 contiene la dirección del siguiente PC (naddr)
    # Restauramos todo
    ld    14, 80(1)
    ld    15, 88(1)
    ld    16, 96(1)
    ld    17, 104(1)
    ld    18, 112(1)
    ld    19, 120(1)
    ld    20, 128(1)
    ld    21, 136(1)
    ld    22, 144(1)
    ld    23, 152(1)
    ld    24, 160(1)
    ld    25, 168(1)
    ld    26, 176(1)
    ld    27, 184(1)
    ld    28, 192(1)
    ld    29, 200(1)
    ld    30, 208(1)
    ld    31, 216(1)
    
    ld    0, 272(1)
    mtlr  0
    addi  1, 1, 256
    blr