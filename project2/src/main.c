#include <avr/io.h>
#include <os.h>
#include <string.h>
#include <uart.h>


static void on (void) {
	
	PORTB|=1<<PB7;
	
}


static void off (void) {
	
	PORTB&=~(1<<PB7);
	
}


static void wait (unsigned int num) {
	
	for (unsigned int n=0;n<num;++n) for (unsigned int i=0;i<32000U;++i) asm volatile ("nop"::);
	
}


static void blink_number (unsigned int num) {
	
	for (unsigned int n=0;n<num;++n) {
		
		on();
		wait(4);
		off();
		wait(4);
		
	}
	
}


static void error (void) {
	
	asm volatile ("cli"::);
	
	unsigned int err=errno;
	for (;;) {
		
		wait(25);
		
		blink_number(err);
		#ifdef DEBUG
		wait(10);
		blink_number(get_last_syscall());
		#endif
		
	}
	
}


static void thread (void * ptr) {
	
	uart_t uart=(uart_t)(uintptr_t)ptr;
	
	struct uart_opt opt;
	memset(&opt,0,sizeof(opt));
	opt.baud=9600;
	
	if (uart_init(uart,opt)!=0) error();
	
	char in;
	for (;;) if ((uart_recv(uart,&in,1,0,0)!=0) || (uart_send(uart,&in,1,0)!=0)) error();
	
}


void rtos_main (void) {
	
	DDRB|=1<<PB7;
	
	for (uart_t u=0;u<4;++u) if (thread_create(0,thread,0,(void *)(uintptr_t)u)!=0) error();
	
}
