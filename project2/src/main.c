#include <avr/io.h>
#include <os.h>


static void pong (void * arg) {
	
	(void)arg;
	
	for (;;) PORTB|=1<<PB7;
	
}


static void ping (void * arg) {
	
	(void)arg;
	
	for (;;) PORTB&=~(1<<PB7);
	
}


void rtos_main (void) {
	
	DDRB|=1<<PB7;
	
	thread_set_priority(thread_self(),14);
	thread_create(0,ping,14,0);
	pong(0);
	
}
