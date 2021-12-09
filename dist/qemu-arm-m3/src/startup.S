.syntax unified
.cpu cortex-m3
.thumb

.global _ivt
.section .startup, "a"
_ivt:
	.word stack_top
	.word _start			// reset
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq
	.word _unknown_irq

.global _unknown_irq
.section .text
.thumb_func
_unknown_irq:
	b .

.global _start
.section .text
.thumb_func
_start:
	// Set stack
	mov r0, _ivt
	ldr sp, [r0, #0]

	// Go to CRT
	bl _crt

	// There we go!
	bl main
	b .

