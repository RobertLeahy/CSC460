#include <avr/io.h>
#include <error.h>
#include <os.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <uart.h>


struct base_station_state {
	
	int8_t x;
	int8_t y;
	bool button;
	mutex_t mutex;
	uart_t uart;
	struct timespec period;
	
};


static void adc_init (void) {
	
	ADCSRA|=(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
	ADMUX|=1<<REFS0;
	ADCSRA|=1<<ADEN;
	
}


static uint16_t adc_read (unsigned char c) {
	
	ADMUX&=0xE0;
	ADMUX|=c&7;
	ADCSRB=c&(1<<3);
	ADCSRA|=1<<ADSC;
	while (ADCSRA&(1<<ADSC));
	return ADCW;
	
}


static void joystick (void * ptr) {
	
	adc_init();
	//	Joystick on pin 52
	DDRB&=~(1<<PB1);
	//	Enable pull up
	PORTB|=1<<PB1;
	
	struct base_station_state * state=(struct base_station_state *)ptr;
	
	(void)state;
	
	for (;;) {
		
		//	A0 = X axis
		//	A1 = Y axis
		uint16_t x=adc_read(0);
		uint16_t y=adc_read(1);
		
		//	Input is 10 bit unsigned integer, we
		//	get rid of the bottom three bits to compress
		//	the range and as a form of ad hoc lowpass
		//	filtering
		x>>=3U;
		y>>=3U;
		int16_t xs=x;
		int16_t ys=y;
		xs-=128;
		ys-=128;
		if (xs==-128) xs=-127;
		if (ys==-128) ys=-127;
		
		bool j=(PINB&(1<<PB1))==0;
		
		if (mutex_lock(state->mutex)!=0) error();
		
		state->x=xs;
		state->y=ys;
		state->button=j;
		
		if (mutex_unlock(state->mutex)!=0) error();
		
	}
	
}


static void uart (void * ptr) {
	
	struct base_station_state * state=(struct base_station_state *)ptr;
	
	for (;;) {
		
		if (mutex_lock(state->mutex)!=0) error();
		
		//	Format is as follows:
		//
		//	First byte: Always 128
		//	Second byte: X value, -127 to 127, two's complement
		//	Third byte: Y value, -127 to 127, two's complement
		//	Fourth byte: Button value, 0 or 1
		unsigned char buffer [4];
		buffer[0]=128U;
		memcpy(buffer+1,&state->x,sizeof(state->y));
		memcpy(buffer+2,&state->y,sizeof(state->x));
		buffer[3]=state->button ? 1 : 0;
		
		if (mutex_unlock(state->mutex)!=0) error();
		
		if (uart_send(state->uart,buffer,sizeof(buffer),0)!=0) error();
		
		if (sleep(state->period)!=0) error();
		
	}
	
}


void a_main (void) {
	
	//	Initialize shared state
	struct base_station_state state;
	memset(&state,0,sizeof(state));
	if (mutex_create(&state.mutex)!=0) error();
	state.uart=1;
	state.period.tv_nsec=20000000ULL;
	
	//	Setup UART
	struct uart_opt uopt;
	memset(&uopt,0,sizeof(uopt));
	uopt.baud=9600;
	if (uart_init(state.uart,uopt)!=0) error();
	
	//	Begin
	if (thread_set_priority(thread_self(),0)!=0) error();
	if (thread_create(0,joystick,0,&state)!=0) error();
	uart(&state);
	
}
