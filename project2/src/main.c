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


struct state {
	
	event_t ping;
	event_t pong;
	
};


static void pong (void * arg) {
	
	const struct state * s=(const struct state *)arg;
	for (;;) {
		
		on();
		wait(3);
		
		if (
			(event_signal(s->pong)!=0) ||
			(event_wait(s->ping)!=0)
		) error();
		
	}
	
}


static void ping (void * arg) {
	
	const struct state * s=(const struct state *)arg;
	for (;;) {
		
		if (event_wait(s->pong)!=0) error();
		
		off();
		wait(3);
		
		if (event_signal(s->ping)!=0) error();
		
	}
	
}


void rtos_main (void) {
	
	DDRB|=1<<PB7;
	off();
	
	struct state s;
	if (
		(event_create(&s.ping)!=0) ||
		(event_create(&s.pong)!=0) ||
		(thread_set_priority(thread_self(),14)!=0) ||
		(thread_create(0,ping,14,&s)!=0)
	) error();
	
	pong(&s);
	
}
