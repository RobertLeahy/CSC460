#include <avr/io.h>
#include <os.h>
#include <roomba.h>
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
		//	TODO: Check number of bytes sent
		if (uart_send(r->uart,&payload,1,0)!=0) break;
		
		return 0;
		
	} while (0);
	
	uart_cleanup(r->uart);
	
	return -1;
	
}


int roomba_destroy (struct roomba * r) {
	
	//	TODO: Change baud rate back to default????
	
	if (uart_cleanup(r->uart)!=0) return -1;
	
	return 0;
	
}
