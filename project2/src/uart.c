#include <avr/interrupt.h>
#include <avr/io.h>
#include <debug.h>
#include <interrupt.h>
#include <os.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <uart.h>


//	This is the number of UARTs that ATmega2560
//	actually has
#define MAX_UARTS (4U)
#define BUFFER_SIZE (32U)


struct uart_channel {
	
	event_t e;
	unsigned char buf [BUFFER_SIZE];
	size_t buflen;
	bool async;
	
};


struct uart_state {
	
	uart_t num;
	struct uart_channel send;
	size_t sent;
	struct uart_channel recv;
	struct uart_recv_error recv_error;
	
};
static struct uart_state uarts [MAX_UARTS];


static struct uart_state * uart_get (uart_t uart) {
	
	if (uart>=MAX_UARTS) return 0;
	
	return &uarts[uart];
	
}


static int uart_channel_init (struct uart_channel * c) {
	
	//	We don't need an event handle from
	//	the OS if it's asynchronous: There's
	//	no blocking and therefore no need for
	//	an event
	if (c->async) return 0;
	
	return event_create(&c->e);
	
}


static int uart_channel_cleanup (struct uart_channel * c) {
	
	if (c->async) return 0;
	
	return event_destroy(c->e);
	
}


static void uart_setup (uart_t uart, struct uart_opt opt) {
	
	//	TODO: Use this
	(void)opt;
	
	//	Just assume 9600 baud
	uint16_t ubrr=103;
	
	#define uart_setup_impl(num) do {	\
		UBRR ## num=ubrr;	\
		UCSR ## num ## A=0;	\
		UCSR ## num ## C=(1<<UCSZ ## num ## 1)|(1<<UCSZ ## num ## 0);	\
		UCSR ## num ## B=(1<<TXEN ## num)|(1<<RXEN ## num)|(1<<RXCIE ## num);	\
	} while (0)
	
	switch (uart) {
		
		default:
			break;
		case 0:
			uart_setup_impl(0);
			break;
		case 1:
			uart_setup_impl(1);
			break;
		case 2:
			uart_setup_impl(2);
			break;
		case 3:
			uart_setup_impl(3);
			break;
		
	}
	
	#undef uart_setup_impl
	
}


int uart_init (uart_t uart, struct uart_opt opt) {
	
	struct uart_state * s=uart_get(uart);
	if (!s) return -1;
	
	memset(s,0,sizeof(*s));
	s->num=uart;
	//	TODO: Choose blocking or non-blocking based
	//	on uart_opt (currently always blocking)
	
	if (uart_channel_init(&s->send)!=0) return -1;
	
	do {
		
		if (uart_channel_init(&s->recv)!=0) break;
		
		uart_setup(uart,opt);
		
		return 0;
		
	} while (0);
	
	uart_channel_cleanup(&s->send);
	
	return -1;
	
}


int uart_cleanup_impl (struct uart_state * s) {
	
	#define uart_cleanup_impl(num) do {	\
		UCSR ## num ## C=0;	\
		UBRR ## num=0;	\
		UCSR ## num ## A=0;	\
		UCSR ## num ## B=0;	\
	} while (0)
	
	switch (s->num) {
		
		default:
			break;
		case 0:
			uart_cleanup_impl(0);
			break;
		case 1:
			uart_cleanup_impl(1);
			break;
		case 2:
			uart_cleanup_impl(2);
			break;
		case 3:
			uart_cleanup_impl(3);
			break;
		
	}
	
	#undef uart_cleanup_impl
	
	int retr=uart_channel_cleanup(&s->send);
	int retr2=uart_channel_cleanup(&s->recv);
	return ((retr==0) && (retr2==0)) ? 0 : -1;
	
}


int uart_cleanup (uart_t uart) {
	
	struct uart_state * s=uart_get(uart);
	if (!s) return -1;
	
	return uart_cleanup_impl(s);
	
}


static void uart_enable_send (struct uart_state * s) {
	
	#define uart_enable_send_impl(num) (UCSR ## num ## B|=1<<UDRIE ## num)
	
	switch (s->num) {
		
		default:
			break;
		case 0:
			uart_enable_send_impl(0);
			break;
		case 1:
			uart_enable_send_impl(1);
			break;
		case 2:
			uart_enable_send_impl(2);
			break;
		case 3:
			uart_enable_send_impl(3);
			break;
		
	}
	
	#undef uart_enable_send_impl
	
}


static void uart_send_buffer_cleanup (struct uart_state * s) {
	
	if (s->sent==0) return;
	
	size_t unsent=s->send.buflen-s->sent;
	if (unsent!=0) memmove(s->send.buf,s->send.buf+s->sent,unsent);
	
	s->sent=0;
	s->send.buflen=unsent;
	
}


static size_t uart_to_send_buffer (struct uart_state * s, const unsigned char * buf, size_t buflen, size_t * sent) {
	
	uart_send_buffer_cleanup(s);
	
	size_t curr=s->send.buflen;
	if (curr==BUFFER_SIZE) return 0;
	
	//	Adjust pointer and number of bytes
	//	to be sent based on how many bytes
	//	we've already sent
	buf+=*sent;
	buflen-=*sent;
	
	size_t cpy=BUFFER_SIZE-curr;
	if (cpy>buflen) cpy=buflen;
	
	memcpy(s->send.buf+curr,buf,cpy);
	
	s->send.buflen+=cpy;
	*sent+=cpy;
	
	uart_enable_send(s);
	
	return cpy;
	
}


static int uart_async_send_impl (struct uart_state * s, const unsigned char * buf, size_t buflen, size_t * sent) {
	
	uart_to_send_buffer(s,buf,buflen,sent);
	
	return 0;
	
}


static int uart_sync_send_impl (struct uart_state * s, const unsigned char * buf, size_t buflen, size_t * sent) {
	
	for (;;) {
		
		uart_to_send_buffer(s,buf,buflen,sent);
		
		//	Done sending
		if (*sent==buflen) break;
		
		//	We filled the buffer, wait for there
		//	to be space
		if (event_wait(s->send.e)!=0) return -1;
		
	}
	
	return 0;
	
}


static int uart_send_impl (uart_t uart, const void * buf, size_t buflen, size_t * sent) {
	
	size_t se;
	if (!sent) sent=&se;
	*sent=0;
	
	if (buflen==0) return 0;
	
	struct uart_state * s=uart_get(uart);
	if (!s) return -1;
	
	const unsigned char * ptr=(const unsigned char *)buf;
	
	bool i=disable_and_push_interrupt();
	int retr=s->send.async ? uart_async_send_impl(s,ptr,buflen,sent) : uart_sync_send_impl(s,ptr,buflen,sent);
	pop_interrupt(i);
	
	return retr;
	
}


int uart_send (uart_t uart, const void * buf, size_t buflen, size_t * sent) {
	
	debug_uart_send_begin();
	
	int retr=uart_send_impl(uart,buf,buflen,sent);
	
	debug_uart_send_end();
	
	return retr;
	
}


#define UART_UDRE_ISR(num) \
ISR(USART ## num ## _UDRE_vect,ISR_BLOCK) {	\
	debug_uart_udre_begin();	\
	struct uart_state * s=&uarts[num];	\
	if (s->send.buflen==s->sent) {	\
		UCSR ## num ## B&=~(1<<UDRIE ## num);	\
	} else {	\
		UDR ## num=s->send.buf[s->sent++];	\
		if (!s->send.async) event_signal_r(s->send.e,0);	\
	}	\
	debug_uart_udre_end();	\
}


UART_UDRE_ISR(0)
UART_UDRE_ISR(1)
UART_UDRE_ISR(2)
UART_UDRE_ISR(3)


#undef UART_UDRE_ISR


static void uart_enable_recv (struct uart_state * s) {
	
	#define uart_enable_recv_impl(num) (UCSR ## num ## B|=1<<RXCIE ## num)
	
	switch (s->num) {
		
		default:
			break;
		case 0:
			uart_enable_recv_impl(0);
			break;
		case 1:
			uart_enable_recv_impl(1);
			break;
		case 2:
			uart_enable_recv_impl(2);
			break;
		case 3:
			uart_enable_recv_impl(3);
			break;
		
	}
	
	#undef uart_enable_recv_impl
	
}


static size_t uart_from_recv_buffer (struct uart_state * s, unsigned char * buf, size_t buflen, size_t * recvd) {
	
	size_t avail=s->recv.buflen;
	if (avail==0) return 0;
	
	//	Adjust pointer and number of bytes
	//	available based on how many have
	//	already been read
	buf+=*recvd;
	buflen-=*recvd;
	
	if (buflen<avail) avail=buflen;
	
	memcpy(buf,s->recv.buf,avail);
	
	s->recv.buflen-=avail;
	*recvd+=avail;
	
	memmove(s->recv.buf,s->recv.buf+avail,s->recv.buflen);
	
	uart_enable_recv(s);
	
	return avail;
	
}


static int uart_async_recv_impl (struct uart_state * s, unsigned char * buf, size_t buflen, size_t * recvd) {
	
	uart_from_recv_buffer(s,buf,buflen,recvd);
	
	return 0;
	
}


static int uart_sync_recv_impl (struct uart_state * s, unsigned char * buf, size_t buflen, size_t * recvd) {
	
	for (;;) {
		
		uart_from_recv_buffer(s,buf,buflen,recvd);
		
		//	Done receiving
		if (*recvd==buflen) break;
		
		//	Buffer empty, wait for more bytes
		if (event_wait(s->recv.e)!=0) return -1;
		
	}
	
	return 0;
	
}


static int uart_recv_impl (uart_t uart, void * buf, size_t buflen, size_t * recvd, struct uart_recv_error * err) {
	
	if (err) memset(err,0,sizeof(*err));
	
	size_t re;
	if (!recvd) recvd=&re;
	*recvd=0;
	
	if (buflen==0) return 0;
	
	struct uart_state * s=uart_get(uart);
	if (!s) return -1;
	
	unsigned char * ptr=(unsigned char *)buf;
	
	bool i=disable_and_push_interrupt();
	
	int retr=s->recv.async ? uart_async_recv_impl(s,ptr,buflen,recvd) : uart_sync_recv_impl(s,ptr,buflen,recvd);
	
	if (err) *err=s->recv_error;
	memset(&s->recv_error,0,sizeof(s->recv_error));
	
	pop_interrupt(i);
	
	return retr;
	
}


int uart_recv (uart_t uart, void * buf, size_t buflen, size_t * recvd, struct uart_recv_error * err) {
	
	debug_uart_recv_begin();
	
	int retr=uart_recv_impl(uart,buf,buflen,recvd,err);
	
	debug_uart_recv_end();
	
	return retr;
	
}


#define UART_RX_ISR(num) \
ISR(USART ## num ## _RX_vect,ISR_BLOCK) {	\
	debug_uart_rx_begin();	\
	struct uart_state * s=&uarts[num];	\
	if ((UCSR ## num ## A&FE ## num)!=0) s->recv_error.frame_error=true;	\
	if ((UCSR ## num ## A&DOR ## num)!=0) s->recv_error.overrun=true;	\
	if ((UCSR ## num ## A&UPE ## num)!=0) s->recv_error.parity_error=true;	\
	if (s->recv.buflen==BUFFER_SIZE) {	\
		UCSR ## num ## B&=~(1<<RXCIE ## num);	\
	} else {	\
		s->recv.buf[s->recv.buflen++]=UDR ## num;	\
		if (!s->recv.async) event_signal_r(s->recv.e,0);	\
	}	\
	debug_uart_rx_end();	\
}


UART_RX_ISR(0)
UART_RX_ISR(1)
UART_RX_ISR(2)
UART_RX_ISR(3)


#undef UART_RX_ISR
