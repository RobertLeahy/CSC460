#include <avr/io.h>
#include <kernel.h>
#include <os.h>


static void pong (void * arg) {
	
	(void)arg;
	
	for (;;) {
		
		PORTB|=1<<PB7;
		
		for (unsigned int i=0;i<32000;++i) asm volatile ("nop"::);
		for (unsigned int i=0;i<32000;++i) asm volatile ("nop"::);
		for (unsigned int i=0;i<32000;++i) asm volatile ("nop"::);
		
		kyield();
		
	}
	
}


static void ping (void * arg) {
	
	(void)arg;
	
	for (;;) {
		
		PORTB&=~(1<<PB7);
		
		for (unsigned int i=0;i<32000;++i) asm volatile ("nop"::);
		for (unsigned int i=0;i<32000;++i) asm volatile ("nop"::);
		for (unsigned int i=0;i<32000;++i) asm volatile ("nop"::);
		
		kyield();
		
	}
	
}


int main (void) {
	
	DDRB|=1<<PB7;
	
	thread_t pi;
	thread_create(&pi,ping,0,0);
	thread_t po;
	thread_create(&po,pong,0,0);
	
	kstart();
	
}
