#include <avr/io.h>
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
	mutex_t roomba_lock;
	int8_t x;
	int8_t y;
	bool button;
	bool die;
	uart_t uart;
	struct roomba roomba;
	event_t event;
	
};


static void sensors (void * ptr) {
	
	struct remote_state * state=(struct remote_state *)ptr;
	
	struct roomba_bumps_and_wheel_drops s;
	struct roomba_virtual_wall v;
	for (;;) {
		
		if (mutex_lock(state->roomba_lock)!=0) error();
		
		if (roomba_sensors(&state->roomba,ROOMBA_BUMPS_AND_WHEEL_DROPS,&s)!=0) error();
		if (roomba_sensors(&state->roomba,ROOMBA_VIRTUAL_WALL,&v)!=0) error();
		
		if (mutex_unlock(state->roomba_lock)!=0) error();
		
		if (s.bump_left || s.bump_right || v.virtual_wall) {
			
			if (mutex_lock(state->mutex)!=0) error();
			state->die=true;
			if (event_signal(state->event)!=0) error();
			if (mutex_unlock(state->mutex)!=0) error();
			
		} else {
			
			
		}
		
		if (sleep(state->period)!=0) error();
		
	}
	
}


static void roomba (void * ptr) {
	
	struct remote_state * state=(struct remote_state *)ptr;
	
	for (;;) {
		
		if (event_wait(state->event)!=0) error();
		
		if (mutex_lock(state->mutex)!=0) error();
		
		int16_t y=state->y;
		int16_t x=state->x;
		bool button=state->button;
		bool die=state->die;
		
		if (mutex_unlock(state->mutex)!=0) error();
		
		y*=4;
		x*=4;
		int16_t r=y-x;
		int16_t l=y+x;
		if (mutex_lock(state->roomba_lock)!=0) error();
		if (roomba_drive_direct(&state->roomba,die ? 0 : r,die ? 0 : l)!=0) error();
		if (mutex_unlock(state->roomba_lock)!=0) error();
		
		if (button) PORTB|=1<<PB0;
		else PORTB&=~(1<<PB0);
		
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
	
	//	Laser is on pin 53
	DDRB|=1<<PB0;
	PORTB&=~(1<<PB0);
	
	struct remote_state state;
	memset(&state,0,sizeof(state));
	state.uart=2;
	if (mutex_create(&state.mutex)!=0) error();
	if (event_create(&state.event)!=0) error();
	if (mutex_create(&state.roomba_lock)!=0) error();
	state.period.tv_nsec=500000000ULL;
	
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
	if (thread_create(0,sensors,0,&state)!=0) error();
	
	uart(&state);
	
}
