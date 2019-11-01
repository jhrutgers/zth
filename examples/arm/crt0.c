#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>

// qemu virt pl011 uart
enum {
	UART_FR_RXFE = 0x10,
	UART_FR_TXFF = 0x20,
	UART0_ADDR = 0x09000000,
};
 
#define UART_DR(baseaddr) (*(unsigned int volatile *)(baseaddr))
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
	while(UART_FR(UART0_ADDR) & UART_FR_RXFE);
	*ptr++ = UART_DR(UART0_ADDR);
	for(todo = 1; todo < len; todo++) {
		if(UART_FR(UART0_ADDR) & UART_FR_RXFE)
			break;
		*ptr++ = UART_DR(UART0_ADDR);
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
		UART_DR(UART0_ADDR) = *ptr++;
	return len;
}

#ifndef CLOCK_MONOTONIC
#  define CLOCK_MONOTONIC 1
#endif
static uint64_t volatile systicks_ns = 1;
int clock_gettime(int clk_id, struct timespec* res) {
	if(!res)
		return EFAULT;
	if(clk_id != CLOCK_MONOTONIC)
		return EINVAL;

	unsigned int freq, cntlow, cnthigh;
	__asm__ (
			"mrc p15, 0, %0, c14, c0, 0\n"
			"mrrc p15, 0, %1, %2, c14\n"
			: "=r"(freq), "=r"(cntlow), "=r"(cnthigh)
	);
	int ns_per_tick = (int)(1e9f / (float)freq + 0.5f);
	uint64_t ns = ((uint64_t)cnthigh << 32 | (uint64_t)cntlow) * ns_per_tick;
	res->tv_sec = ns / (uint64_t)1000000000UL;
	res->tv_nsec = ns % (uint64_t)1000000000UL;
	return 0;
}

__attribute__((interrupt)) void systick_handler() {
	// Ticks are configured to be fired every ms.
	systicks_ns += (uint64_t)1000000;
}

void _crt()
{
	extern char __bss_start__;
	extern char __bss_end__;
	memset(&__bss_start__, 0, &__bss_end__ - &__bss_start__);

	extern char __init_array_start;
	extern char __init_array_end;
	for(void** f = (void**)&__init_array_start; (char*)f < &__init_array_end; f++)
		((void(*)(void))(*f))();
}

void* __dso_handle __attribute__((weak));

