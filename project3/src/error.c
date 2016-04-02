#include <avr/io.h>
#include <error.h>
#include <interrupt.h>
#include <os.h>


static void on (void) {
	
	PORTB|=1<<PB7;
	
}


static void off (void) {
	
	PORTB&=~(1<<PB7);
	
}


static void wait (unsigned int num) {
	
	for (unsigned int n=0;n<num;++n) for (unsigned int i=0;i<32000U;++i) asm volatile ("nop");
	
}


static void blink_number (unsigned int num) {
	
	for (unsigned int n=0;n<num;++n) {
		
		on();
		wait(4);
		off();
		wait(4);
		
	}
	
}


void error (void) {
	
	//	Disabling interrupts freezes the kernel
	//	and allows us to run forever without being
	//	pre-empted et cetera
	disable_interrupt();
	
	unsigned int e=errno;
	unsigned int s=get_last_syscall();
	
	DDRB|=1<<PB7;
	PORTB&=~(1<<PB7);
	
	for (;;) {
		
		wait(25);
		
		blink_number(e);
		wait(10);
		blink_number(s);
		
	}
	
}
