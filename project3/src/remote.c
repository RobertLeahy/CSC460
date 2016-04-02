#include <adc.h>
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
	struct roomba_bumps_and_wheel_drops bumps_and_wheel_drops;
	struct roomba_virtual_wall virtual_wall;
	
};


static void die (struct remote_state * state) {
	
	if (mutex_lock(state->mutex)!=0) error();
	
	state->die=true;
	
	if (mutex_unlock(state->mutex)!=0) error();
	
}


static void sensors (void * ptr) {
	
	adc_init();
	//	Get initial light sensor value
	//
	//	Light sensor is on channel 0
	uint16_t ls=adc_read(0);
	
	struct remote_state * state=(struct remote_state *)ptr;
	
	struct roomba_bumps_and_wheel_drops s;
	struct roomba_virtual_wall v;
	for (;;) {
		
		if (mutex_lock(state->roomba_lock)!=0) error();
		
		if (roomba_sensors(&state->roomba,ROOMBA_BUMPS_AND_WHEEL_DROPS,&s)!=0) error();
		if (roomba_sensors(&state->roomba,ROOMBA_VIRTUAL_WALL,&v)!=0) error();
		
		if (mutex_unlock(state->roomba_lock)!=0) error();
		
		if (mutex_lock(state->mutex)!=0) error();
		
		state->bumps_and_wheel_drops=s;
		state->virtual_wall=v;
		
		if (mutex_unlock(state->mutex)!=0) error();
		
		//	Read light sensor
		uint16_t lsc=adc_read(0);
		//	Threshold of 20 is adjustable
		if (lsc>(ls+20)) die(state);
		//	Adaptable default value
		else ls=lsc;
		
		if (sleep(state->period)!=0) error();
		
	}
	
}


enum ai_behaviour {
	
	AI_BEHAVIOUR_FORWARD,
	AI_BEHAVIOUR_RETREAT,
	AI_BEHAVIOUR_CHANGE_DIRECTION
	
};


struct ai_state {
	
	enum ai_behaviour behaviour;
	unsigned int remaining_ticks;
	bool hit_right;
	
};


static void ai_state_clear (struct ai_state * state) {
	
	state->behaviour=AI_BEHAVIOUR_FORWARD;
	state->remaining_ticks=0;
	state->hit_right=false;
	
}


static void do_ai (struct ai_state * ai, struct remote_state * state) {
	
	bool hit_right;
	bool hit_left;
	
	if (mutex_lock(state->mutex)!=0) error();
	hit_right=state->bumps_and_wheel_drops.bump_right;
	hit_left=state->bumps_and_wheel_drops.bump_left || state->virtual_wall.virtual_wall;
	if (mutex_unlock(state->mutex)!=0) error();
	
	if (hit_right || hit_left) {
		
		ai->behaviour=AI_BEHAVIOUR_RETREAT;
		ai->remaining_ticks=35;
		ai->hit_right=hit_right;
		
	}
	
	int16_t r;
	int16_t l;
	
	for (;;) {
		
		enum ai_behaviour orig=ai->behaviour;
		
		switch (ai->behaviour) {
			
			case AI_BEHAVIOUR_FORWARD:
				r=350;
				l=350;
				break;
			case AI_BEHAVIOUR_RETREAT:
				if (ai->remaining_ticks==0) {
					
					ai->behaviour=AI_BEHAVIOUR_CHANGE_DIRECTION;
					ai->remaining_ticks=20;
					break;
					
				}
				
				r=-200;
				l=-200;
				--ai->remaining_ticks;
				break;
			case AI_BEHAVIOUR_CHANGE_DIRECTION:
				if (ai->remaining_ticks==0) {
					
					ai->behaviour=AI_BEHAVIOUR_FORWARD;
					break;
					
				}
				l=200;
				r=-200;
				if (ai->hit_right) {
					
					l*=-1;
					r*=-1;
					
				}
				--ai->remaining_ticks;
				break;
			
		}
		
		if (orig==ai->behaviour) break;
		
	}
	
	if (mutex_lock(state->roomba_lock)!=0) error();
	if (roomba_drive_direct(&state->roomba,r,l)!=0) error();
	if (mutex_unlock(state->roomba_lock)!=0) error();	
	
}


static void roomba (void * ptr) {
	
	struct remote_state * state=(struct remote_state *)ptr;
	struct ai_state ai;
	ai_state_clear(&ai);
	
	for (;;) {
		
		if (mutex_lock(state->mutex)!=0) error();
		
		int16_t y=state->y;
		int16_t x=state->x;
		bool button=state->button;
		bool die=state->die;
		
		if (mutex_unlock(state->mutex)!=0) error();
		
		if (die) {
			
			if (mutex_lock(state->roomba_lock)!=0) error();
			if (roomba_drive_direct(&state->roomba,0,0)!=0) error();
			if (mutex_unlock(state->roomba_lock)!=0) error();
			
			return;
			
		}
		
		//	Laser
		if (button) PORTB|=1<<PB0;
		else PORTB&=~(1<<PB0);
		
		//	If the other end isn't sending anything
		//	interesting defer to the AI
		if ((x==0) && (y==0)) {
			
			do_ai(&ai,state);
			
		//	Otherwise execute the commands from the
		//	other end and clear the AI state (so that
		//	the robot starts "fresh")
		} else {
			
			ai_state_clear(&ai);
			
			//	Translate values from base station
			//	to right and left wheel speeds
			y*=4;
			x*=4;
			int16_t r=y-x;
			int16_t l=y+x;
			
			//	Send speed to Roomba
			if (mutex_lock(state->roomba_lock)!=0) error();
			if (roomba_drive_direct(&state->roomba,r,l)!=0) error();
			if (mutex_unlock(state->roomba_lock)!=0) error();
			
		}
		
		if (sleep(state->period)!=0) error();
		
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
	if (mutex_create(&state.roomba_lock)!=0) error();
	state.period.tv_nsec=20000000ULL;
	
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
