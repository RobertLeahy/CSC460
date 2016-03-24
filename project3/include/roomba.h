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


/**
 *	Initializes a roomba structure and sets up all
 *	appropriate hardware to communicate with a Roomba.
 *
 *	\param [out] r
 *		A pointer to a roomba structure which shall be
 *		initialized.
 *	\param [in] opt
 *		A roomba_opt structure which specifies the details
 *		of the initialization.
 *
 *	\return
 *		0 if the call succeeded, -1 otherwise.
 */
int roomba_create (struct roomba * r, struct roomba_opt opt);
/**
 *	Cleans up a roomba structure and releases all hardware
 *	resources associated with it.
 *
 *	\param [in] r
 *		A pointer to a roomba structure which shall be cleaned
 *		up.
 *
 *	\return
 *		0 if the call succeeded, -1 otherwise.
 */
int roomba_destroy (struct roomba * r);
