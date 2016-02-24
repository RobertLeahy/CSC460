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
	
	thread_t pi;
	thread_create(&pi,ping,0,0);
	pong(0);
	
}
