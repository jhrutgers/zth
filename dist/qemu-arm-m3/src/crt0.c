/*
 * SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

enum {
	UART1_ADDR = 0x40011000,
};

#define UART_DR(baseaddr) (*(((unsigned int volatile*)(baseaddr)) + 1))

int _close(int file)
{
	return -1;
}

int _fstat(int file, struct stat* st)
{
	st->st_mode = S_IFCHR;
	return 0;
}

int _isatty(int file)
{
	return 1;
}

int _lseek(int file, int ptr, int dir)
{
	return 0;
}

int _open(const char* name, int flags, int mode)
{
	return -1;
}

int _read(int file, char* ptr, int len)
{
	errno = EAGAIN;
	return -1;
}

int _unlink(const char* __path)
{
	return 0;
}

// See startup.S.
void abort(void);

void _exit(int __status)
{
	abort();
}

int _kill(pid_t p, int sig)
{
	return 0;
}

pid_t _getpid(void)
{
	return 0;
}

int _gettimeofday(struct timeval* __p, void* __tz)
{
	if(__p)
		memset(__p, 0, sizeof(*__p));
	return 0;
}

caddr_t _sbrk(int incr)
{
	extern char heap_low; /* Defined by the linker */
	extern char heap_top; /* Defined by the linker */
	char* prev_heap_end;

	static char* heap_end = 0;

	if(heap_end == 0)
		heap_end = &heap_low;

	prev_heap_end = heap_end;

	if(heap_end + incr > &heap_top) {
		/* Heap and stack collision */
		errno = ENOMEM;
		return (caddr_t)-1;
	}

	heap_end += incr;
	return (caddr_t)prev_heap_end;
}

int _write(int file, char* ptr, int len)
{
	int todo;

	for(todo = 0; todo < len; todo++)
		UART_DR(UART1_ADDR) = *ptr++;
	return len;
}

#ifndef CLOCK_MONOTONIC
#  define CLOCK_MONOTONIC 1
#endif

int clock_gettime(int clk_id, struct timespec* res)
{
	if(!res)
		return EFAULT;
	if(clk_id != CLOCK_MONOTONIC)
		return EINVAL;

	// The CYCCNT register is not implemented in Qemu's Cortex-M3.  We use
	// TIM2 for at. It runs at 1 GHz. At this speed, it would overflow
	// every 4.2 s.  This is not a perfect implementation. A counter
	// overflow it not handled correctly if the calls between
	// clock_gettime() is more than this interval. To reduce the chance
	// that it happens, prescale it by 16.  This is good enough for this
	// example.
	static uint32_t cnt_prev = 0;
	static uint32_t cnt_wraps = 0;

	if(cnt_wraps == 0) {
		// Initialization needed.
		*((uint32_t volatile*)0x40000000) = 1;	// TIM2_CR1: enable counter
		*((uint32_t volatile*)0x40000028) = 15; // TIM2_PSC: set prescaler to 16
		cnt_wraps = 1;
	}

	uint32_t cnt = *((uint32_t volatile*)0x40000024); // TIM2_CNT

	if(cnt < cnt_prev)
		cnt_wraps++;

	cnt_prev = cnt;

	uint64_t ns = (((uint64_t)cnt_wraps << 32U) | (uint64_t)cnt) * 16U;
	res->tv_sec = ns / (uint64_t)1000000000UL;
	res->tv_nsec = ns % (uint64_t)1000000000UL;
	return 0;
}

void _crt()
{
	extern char __bss_start__;
	extern char __bss_end__;
	memset(&__bss_start__, 0, (uintptr_t)&__bss_end__ - (uintptr_t)&__bss_start__);

	extern char _data_flash;
	extern char __data_start;
	extern char _edata;
	memcpy(&__data_start, &_data_flash, (uintptr_t)&_edata - (uintptr_t)&__data_start);

	extern char __init_array_start;
	extern char __init_array_end;
	for(void** f = (void**)&__init_array_start; (uintptr_t)f < (uintptr_t)&__init_array_end;
	    f++)
		((void (*)(void))(*f))();
}

void* __dso_handle __attribute__((weak));
