#include <avr/io.h>
#include <os.h>


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


static void pong (void * arg) {
	
	mutex_t mutex=*((const mutex_t *)arg);
	for (;;) {
		
		if (mutex_lock(mutex)!=0) error();
		
		on();
		wait(3);
		
		if (mutex_unlock(mutex)!=0) error();
		yield();
		
	}
	
}


static void ping (void * arg) {
	
	mutex_t mutex=*((const mutex_t *)arg);
	for (;;) {
		
		if (mutex_lock(mutex)!=0) error();
		
		off();
		wait(3);
		
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
