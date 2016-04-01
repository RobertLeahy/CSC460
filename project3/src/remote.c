#include <error.h>
#include <roomba.h>
#include <os.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <uart.h>


struct remote_state {
	
	struct timespec period;
	mutex_t mutex;
	int8_t x;
	int8_t y;
	bool button;
	uart_t uart;
	
};


static void uart (void * ptr) {
	
	struct remote_state * state=(struct remote_state *)ptr;
	
	for (;;) {
		
		for (;;) {
			
			unsigned char b;
			size_t num;
			if (uart_recv(state->uart,&b,1,&num,0)!=0) error();
			if (num!=1) continue;
			if (b!=128U) continue;
			
			break;
			
		}
		
		unsigned char buffer [3];
		size_t num;
		if (uart_recv(state->uart,buffer,sizeof(buffer),&num,0)!=0) error();
		if (num!=sizeof(buffer)) continue;
		
		int8_t x;
		memcpy(&x,buffer,1);
		int8_t y;
		memcpy(&y,buffer+1,1);
		bool button=buffer[2]!=0;
		
		if (mutex_lock(state->mutex)!=0) error();
		
		state->x=x;
		state->y=y;
		state->button=button;
		
		if (mutex_unlock(state->mutex)!=0) error();
		
		//	DEBUG
		{
			
			unsigned char buffer [4];
			buffer[0]='|';
			buffer[1]=(x==0) ? '0' : ((x<0) ? '-' : '+');
			buffer[2]=(y==0) ? '0' : ((y<0) ? '-' : '+');
			buffer[3]=button ? '.' : ' ';
			
			if (uart_send(0,buffer,sizeof(buffer),0)!=0) error();
			
		}
		
		if (sleep(state->period)!=0) error();
		
	}
	
}


void a_main (void) {
	
	struct remote_state state;
	memset(&state,0,sizeof(state));
	state.uart=1;
	state.period.tv_nsec=20000000ULL;
	if (mutex_create(&state.mutex)!=0) error();
	
	struct uart_opt opt;
	memset(&opt,0,sizeof(opt));
	opt.baud=9600;
	if (uart_init(state.uart,opt)!=0) error();
	
	//	DEBUGGING
	if (uart_init(0,opt)!=0) error();
	
	uart(&state);
	
}
