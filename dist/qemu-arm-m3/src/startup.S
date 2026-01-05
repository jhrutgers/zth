/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

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

.global abort
.global _unknown_irq
.section .text
.thumb_func
abort:
_unknown_irq:
	ldr r1, .L_ADP_Stopped_RunTimeErrorUnknown
	mov r0, #0x18	// angel_SWIreason_ReportException
	bkpt #0xab	// semihosting
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
	mov r0, #0	// argc
	mov r1, #0	// argv
	mov r2, #0	// envp
	bl main

	// Ask QEMU to quit, which requires semihosting.
	cmp r0, #0
	ite eq
	ldreq r1, .L_ADP_Stopped_ApplicationExit
	ldrne r1, .L_ADP_Stopped_RunTimeErrorUnknown
	mov r0, #0x18	// angel_SWIreason_ReportException
	bkpt #0xab	// semihosting

	// Halt, in case the bkpt didn't work.
	b .

.L_ADP_Stopped_RunTimeErrorUnknown:
	.word 0x20023
.L_ADP_Stopped_ApplicationExit:
	.word 0x20026

