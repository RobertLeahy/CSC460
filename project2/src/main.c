#include <avr/io.h>
#include <os.h>


static void error (void) {
	
	asm volatile ("cli"::);
	for (;;);
	
}


static void wait (void) {
	
	for (unsigned int n=0;n<3;++n) for (unsigned int i=0;i<32000U;++i) asm volatile ("nop"::);
	
}


static void pong (void * arg) {
	
	mutex_t mutex=*((const mutex_t *)arg);
	for (;;) {
		
		if (mutex_lock(mutex)!=0) error();
		
		PORTB|=1<<PB7;
		wait();
		
		if (mutex_unlock(mutex)!=0) error();
		yield();
		
	}
	
}


static void ping (void * arg) {
	
	mutex_t mutex=*((const mutex_t *)arg);
	for (;;) {
		
		if (mutex_lock(mutex)!=0) error();
		
		PORTB&=~(1<<PB7);
		wait();
		
		if (mutex_unlock(mutex)!=0) error();
		yield();
		
	}
	
}


void rtos_main (void) {
	
	DDRB|=1<<PB7;
	
	mutex_t mutex;
	if (
		(mutex_create(&mutex)!=0) ||
		(thread_set_priority(thread_self(),14)!=0) ||
		(thread_create(0,ping,14,&mutex)!=0)
	) error();
	
	pong(&mutex);
	
}
