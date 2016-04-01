#include <error.h>
#include <roomba.h>
#include <os.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <uart.h>


struct remote_state {
	
	mutex_t mutex;
	int8_t x;
	int8_t y;
	bool button;
	uart_t uart;
	struct roomba roomba;
	event_t event;
	
};


static void roomba (void * ptr) {
	
	struct remote_state * state=(struct remote_state *)ptr;
	
	for (;;) {
		
		if (event_wait(state->event)!=0) error();
		
		if (mutex_lock(state->mutex)!=0) error();
		
		int16_t y=state->y;
		int16_t x=state->x;
		
		if (mutex_unlock(state->mutex)!=0) error();
		
		y*=4;
		x*=4;
		int16_t r=y-x;
		int16_t l=y+x;
		if (roomba_drive_direct(&state->roomba,r,l)!=0) error();
		
	}
	
}


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
		
		if ((state->x!=x) || (state->y!=y) || (state->button!=button)) {
			
			if (event_signal(state->event)!=0) error();
			
		}
		
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
		
	}
	
}


void a_main (void) {
	
	struct remote_state state;
	memset(&state,0,sizeof(state));
	state.uart=2;
	if (mutex_create(&state.mutex)!=0) error();
	if (event_create(&state.event)!=0) error();
	
	struct uart_opt opt;
	memset(&opt,0,sizeof(opt));
	opt.baud=9600;
	if (uart_init(state.uart,opt)!=0) error();
	
	struct roomba_opt ropt;
	ropt.uart=1;
	if (roomba_create(&state.roomba,ropt)!=0) error();
	if (roomba_safe(&state.roomba)!=0) error();
	
	//	DEBUGGING
	if (uart_init(0,opt)!=0) error();
	
	if (thread_set_priority(thread_self(),0)!=0) error();
	if (thread_create(0,roomba,0,&state)!=0) error();
	
	uart(&state);
	
}
