#include <avr/io.h>
#include <os.h>
#include <roomba.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <uart.h>


static void roomba_brc_setup (struct roomba * r, struct roomba_opt opt) {
	
	(void)r;
	(void)opt;
	
	DDRB|=1<<PB1;
	
}


static void roomba_brc_on (struct roomba * r, struct roomba_opt opt) {
	
	(void)r;
	(void)opt;
	
	PORTB|=1<<PB1;
	
}


static void roomba_brc_off (struct roomba * r, struct roomba_opt opt) {
	
	(void)r;
	(void)opt;
	
	PORTB&=~(1<<PB1);
	
}


static int roomba_init (struct roomba * r, struct roomba_opt opt) {
	
	(void)r;
	(void)opt;
	
	struct timespec ts;
	memset(&ts,0,sizeof(ts));
	ts.tv_nsec=500000000UL;
	
	roomba_brc_off(r,opt);
	if (sleep(ts)!=0) return -1;
	roomba_brc_on(r,opt);
	
	ts.tv_nsec=0;
	ts.tv_sec=2;
	if (sleep(ts)!=0) return -1;
	
	return 0;
	
}


static int roomba_init_baud_rate (struct roomba * r, struct roomba_opt opt) {
	
	(void)r;
	
	//	TODO: Figure out how to handle this
	//	situation with respect to the fact that
	//	the Roomba may be "stuck" in a different
	//	baud rate
	if (opt.high_baud) return 0;
	
	struct timespec ts;
	memset(&ts,0,sizeof(ts));
	ts.tv_nsec=50000000ULL;
	for (int i=0;i<3;++i) {
		
		roomba_brc_off(r,opt);
		if (sleep(ts)!=0) return -1;
		roomba_brc_on(r,opt);
		if (sleep(ts)!=0) return -1;
		
	}
	
	return 0;
	
}


static int roomba_send (struct roomba * r, const void * ptr, size_t num) {
	
	size_t sent;
	if (uart_send(r->uart,ptr,num,&sent)!=0) return -1;
	
	//	TODO: Check number of bytes
	
	return 0;
	
}


static int roomba_recv (struct roomba * r, void * ptr, size_t num) {
	
	size_t recvd;
	if (uart_recv(r->uart,ptr,num,&recvd,0)!=0) return -1;
	
	//	TODO: Check number of bytes
	
	//	TODO: Handle receive error codes
	
	return 0;
	
}


int roomba_create (struct roomba * r, struct roomba_opt opt) {
	
	memset(r,0,sizeof(*r));
	
	r->uart=opt.uart;
	
	roomba_brc_setup(r,opt);
	
	if (roomba_init(r,opt)!=0) return -1;
	
	if (roomba_init_baud_rate(r,opt)!=0) return -1;
	
	struct uart_opt uopt;
	memset(&uopt,0,sizeof(uopt));
	uopt.baud=opt.high_baud ? 115200 : 19200;
	
	if (uart_init(r->uart,uopt)!=0) return -1;
	
	do {
		
		unsigned char payload=128U;
		if (roomba_send(r,&payload,1)!=0) break;
		
		return 0;
		
	} while (0);
	
	uart_cleanup(r->uart);
	
	return -1;
	
}


int roomba_destroy (struct roomba * r) {
	
	//	TODO: Change baud rate back to default????
	
	//	TODO: Reset Roomba????
	
	if (uart_cleanup(r->uart)!=0) return -1;
	
	return 0;
	
}


int roomba_clean (struct roomba * r) {
	
	unsigned char payload=135U;
	return roomba_send(r,&payload,1);
	
}


int roomba_safe (struct roomba * r) {
	
	unsigned char payload=131U;
	return roomba_send(r,&payload,1);
	
}


static void roomba_signed_16_push (int16_t v, unsigned char * ptr) {
	
	memcpy(ptr,&v,sizeof(v));
	
	//	NOTE: We assume that the ATmega 2560 is two's
	//	complement which seems to be supported by
	//	documentation
	
	#if __BYTE_ORDER__==__ORDER_BIG_ENDIAN__
	
	#elif __BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__
	unsigned char tmp=ptr[0];
	ptr[0]=ptr[1];
	ptr[1]=tmp;
	#else
	#error PDP endian
	#endif
	
}


static int roomba_drive_impl (struct roomba * r, int16_t velocity, int16_t radius) {
	
	unsigned char buffer [5];
	buffer[0]=137U;
	
	roomba_signed_16_push(velocity,buffer+1);
	roomba_signed_16_push(radius,buffer+3);
	
	return roomba_send(r,buffer,sizeof(buffer));
	
}


int roomba_drive (struct roomba * r, int16_t velocity, int16_t radius) {
	
	switch (radius) {
		
		case 0:
			radius=32267;
			break;
		case 1:
			++radius;
			break;
		case -1:
			--radius;
		default:
			if (radius<-2000) radius=-2000;
			else if (radius>2000) radius=2000;
			break;
		
	}
	
	if (velocity<-500) velocity=-500;
	if (velocity>500) velocity=500;
	
	return roomba_drive_impl(r,velocity,radius);
	
}


int roomba_turn (struct roomba * r, int16_t velocity, bool left) {
	
	return roomba_drive_impl(r,velocity,left ? 1 : -1);
	
}


static void roomba_drive_direct_clamp (int16_t * value) {
	
	if (*value<-500) {
		
		*value=-500;
		return;
		
	}
	
	if (*value>500) {
		
		*value=500;
		return;
		
	}
	
}


int roomba_drive_direct (struct roomba * r, int16_t r_velocity, int16_t l_velocity) {
	
	roomba_drive_direct_clamp(&r_velocity);
	roomba_drive_direct_clamp(&l_velocity);
	
	unsigned char buffer [5];
	buffer[0]=145U;
	roomba_signed_16_push(r_velocity,buffer+1);
	roomba_signed_16_push(l_velocity,buffer+3);
	
	return roomba_send(r,buffer,sizeof(buffer));
	
}


int roomba_sensors (struct roomba * r, enum roomba_packet_id id, void * ptr) {
	
	//	Request sensor data
	unsigned char buffer [2];
	buffer[0]=142;
	buffer[1]=(unsigned char)id;
	if (roomba_send(r,buffer,sizeof(buffer))!=0) return -1;
	
	switch (id) {
		
		case ROOMBA_BUMPS_AND_WHEEL_DROPS:{
			
			struct roomba_bumps_and_wheel_drops * p=(struct roomba_bumps_and_wheel_drops *)ptr;
			
			unsigned char b;
			if (roomba_recv(r,&b,1)!=0) return -1;
			
			p->bump_right=(b&1)!=0;
			p->bump_left=(b&2)!=0;
			p->wheel_drop_right=(b&4)!=0;
			p->wheel_drop_left=(b&8)!=0;
			
		}break;
		
	}
	
	return 0;
	
}
