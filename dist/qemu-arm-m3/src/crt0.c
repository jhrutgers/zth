#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>

// qemu virt pl011 uart
enum {
	UART_FR_RXFE = 0x10,
	UART_FR_TXFF = 0x20,
	UART1_ADDR = 0x40011000,
};

#define UART_DR(baseaddr) (*(((unsigned int volatile *)(baseaddr))+1))
#define UART_FR(baseaddr) (*(((unsigned int volatile *)(baseaddr))+6))

int _close(int file) { return -1; }

int _fstat(int file, struct stat *st) {
	st->st_mode = S_IFCHR;
	return 0;
}

int _isatty(int file) { return 1; }

int _lseek(int file, int ptr, int dir) { return 0; }

int _open(const char *name, int flags, int mode) { return -1; }

int _read(int file, char *ptr, int len) {
	int todo;
	if(len == 0)
		return 0;
	while(UART_FR(UART1_ADDR) & UART_FR_RXFE);
	*ptr++ = UART_DR(UART1_ADDR);
	for(todo = 1; todo < len; todo++) {
		if(UART_FR(UART1_ADDR) & UART_FR_RXFE)
			break;
		*ptr++ = UART_DR(UART1_ADDR);
	}
	return todo;
}

int _unlink (const char *__path) { return 0; }
void _exit (int __status) { while(1); }
void abort (void) { while(1); }
int _kill (pid_t p, int sig) { return 0; }
pid_t _getpid (void) { return 0; }
int _gettimeofday (struct timeval *__p, void *__tz) {
	if(__p)
		memset(__p, 0, sizeof(*__p));
	return 0;
}

char *heap_end = 0;
caddr_t _sbrk(int incr) {
	extern char heap_low; /* Defined by the linker */
	extern char heap_top; /* Defined by the linker */
	char *prev_heap_end;

	if (heap_end == 0)
		heap_end = &heap_low;

	prev_heap_end = heap_end;

	if (heap_end + incr > &heap_top) {
		/* Heap and stack collision */
		return (caddr_t)0;
	}

	heap_end += incr;
	return (caddr_t) prev_heap_end;
}

int _write(int file, char *ptr, int len) {
	int todo;

	for (todo = 0; todo < len; todo++)
		UART_DR(UART1_ADDR) = *ptr++;
	return len;
}

#ifndef CLOCK_MONOTONIC
#  define CLOCK_MONOTONIC 1
#endif
int clock_gettime(int clk_id, struct timespec* res) {
	if(!res)
		return EFAULT;
	if(clk_id != CLOCK_MONOTONIC)
		return EINVAL;

	// This is not a perfect implementation. A counter overflow it not
	// handled correctly if the calls between clock_gettime() is more than
	// 35 s. However, that is good enough for this example.
	unsigned int const ns_per_tick = 50;

	static uint32_t cnt_prev = 0;
	static uint32_t cnt_wraps = 0;

	if(cnt_wraps == 0) {
		// Initialization needed.
		*((uint32_t volatile*)0x40000000) = 1; // TIM2_CR1: enable counter
		*((uint32_t volatile*)0x40000028) = 5; // TIM2_PSC: set prescaler to have 50 ns per tick.
		cnt_wraps = 1;
	}

	uint32_t cnt = *((uint32_t volatile*)0x40000024); // TIM2_CNT

	if(cnt < cnt_prev)
		cnt_wraps++;

	cnt_prev = cnt;

	uint64_t ns = (((uint64_t)cnt_wraps << 32u) | (uint64_t)cnt) * ns_per_tick;
	res->tv_sec = ns / (uint64_t)1000000000UL;
	res->tv_nsec = ns % (uint64_t)1000000000UL;
	return 0;
}

void _crt()
{
	extern char __bss_start__;
	extern char __bss_end__;
	memset(&__bss_start__, 0, (uintptr_t)&__bss_end__ - (uintptr_t)&__bss_start__);

	extern char __init_array_start;
	extern char __init_array_end;
	for(void** f = (void**)&__init_array_start; (uintptr_t)f < (uintptr_t)&__init_array_end; f++)
		((void(*)(void))(*f))();
}

void* __dso_handle __attribute__((weak));

