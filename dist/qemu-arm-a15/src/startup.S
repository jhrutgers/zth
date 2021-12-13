.global _ivt
.section .startup, "ax"
_ivt:
	b _start			// reset
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq
	b _unknown_irq

.global _unknown_irq
.section .text
_unknown_irq:
	b .

.global _start
.section .text
_start:
	// Set stack
	ldr sp, =stack_top

	// Set interrupt vector table
	ldr r0, =_ivt
	MCR p15, 0, r0, c12, c0, 0

	// Enable VFP
	LDR r0, =(0xF << 20)
	MCR p15, 0, r0, c1, c0, 2
	// Set the FPEXC EN bit to enable the FPU:
	MOV r3, #0x40000000
	VMSR FPEXC, r3

	// Go to CRT
	bl _crt

	// There we go!
	bl main

	// Ask QEMU to quit, which requires semihosting.
	cmp r0, #0
	ite eq
	ldreq r1, .L_ADP_Stopped_ApplicationExit
	ldrne r1, .L_ADP_Stopped_RunTimeErrorUnknown
	mov r0, #0x18	// angel_SWIreason_ReportException
	svc #0x00123456	// semihosting

	// Halt, in case the svc didn't work.
	b .

.L_ADP_Stopped_RunTimeErrorUnknown:
	.word 0x20023
.L_ADP_Stopped_ApplicationExit:
	.word 0x20026

