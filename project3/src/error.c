#include <avr/io.h>
#include <error.h>
#include <interrupt.h>
#include <os.h>


void error (void) {
	
	//	Disabling interrupts freezes the kernel
	//	and allows us to run forever without being
	//	pre-empted et cetera
	disable_interrupt();
	
	unsigned int e=errno;
	unsigned int s=get_last_syscall();
	
	DDRB|=1<<PB7;
	DDRB&=~(1<<PB7);
	
	for (;;) {
		
		//	TODO
		(void)e;
		(void)s;
		
	}
	
}
