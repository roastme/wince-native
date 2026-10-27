@ Bring-up aid: vectors for undefined/prefetch/data abort that dump the CPU state to the log ring and stop.
@ Enabled with -DDSI_EXC_TRAP (see Makefile / ExVector.S). The CE handlers (UndefException etc.) are not used then.
    .syntax divided
    .arm
    .text
    .macro TRAPENTRY name, id
    .global \name
\name:
    stmfd sp!, {r0-r12, lr}
    mrs r1, spsr
    mov r0, sp
    mov r2, #\id
    ldr r12, =DsiExcDump
    bx r12
    .endm
    TRAPENTRY DsiTrapUndef, 1
    TRAPENTRY DsiTrapPabt, 3
    TRAPENTRY DsiTrapDabt, 4
    .ltorg
