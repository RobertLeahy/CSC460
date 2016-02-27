#include <avr/io.h>
#include <os.h>
#include <string.h>


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


void rtos_main (void) {
	
	DDRB|=1<<PB7;
	
	struct timespec ts;
	ts.tv_sec=0;
	ts.tv_nsec=900000000ULL;
	
	for (;;) {
		
		off();
		
		if (sleep(ts)!=0) error();
		
		on();
		
		if (sleep(ts)!=0) error();
		
	}
	
}
