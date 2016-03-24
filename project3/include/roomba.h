/**
 *	\file
 */


#pragma once


#include <stdbool.h>
#include <uart.h>


/**
 *	An opaque type which represents a Roomba.
 */
struct roomba {
	
	uart_t uart;
	
};


struct roomba_opt {
	
	/**
	 *	Indicates the UART over which the Arduino
	 *	shall communicate with the Roomba.
	 */
	uart_t uart;
	/**
	 *	If set to \em true communication with the
	 *	Roomba shall occur at 115200 baud, if set
	 *	to \em false communication with the Roomba
	 *	shall occur at 19200 baud.
	 */
	bool high_baud;
	
};


int roomba_create (struct roomba * r, struct roomba_opt opt);
int roomba_destroy (struct roomba * r);
