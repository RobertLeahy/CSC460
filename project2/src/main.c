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


static void spin (void * ptr) {
	
	(void)ptr;
	
	for (;;) asm volatile ("nop");
	
}


static void acquire (void * ptr) {
	
	if (mutex_lock(*(mutex_t *)ptr)!=0) error();
	
	for (;;) on();
	
}


void rtos_main (void) {
	
	DDRB|=1<<PB7;
	off();
	
	thread_t me=thread_self();
	
	mutex_t m;
	if (mutex_create(&m)!=0) error();
	if (mutex_lock(m)!=0) error();
	
	if (thread_set_priority(me,0)!=0) error();
	
	if (thread_create(0,acquire,3,0)!=0) error();
	
	priority_t prio;
	priority_t eff;
	if ((thread_get_priority(me,&prio)!=0) || (thread_get_effective_priority(me,&eff)!=0)) error();
	
	if (thread_create(0,spin,3,0)!=0) error();
	
	if (mutex_unlock(m)!=0) error();
	
}
