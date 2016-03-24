/**
 *	\file
 */


#pragma once


#include <stdbool.h>
#include <stddef.h>


/**
 *	An integer type which indicates the
 *	number of a UART unit.
 */
typedef unsigned char uart_t;
/**
 *	A structure which specifies options for
 *	a UART.
 *
 *	For forward compatibility make sure to
 *	always zero this structure before use.
 */
struct uart_opt {
	
	/**
	 *	The baud rate of the UART.
	 */
	unsigned int baud;
	
};
/**
 *	A structure which contains all errors
 *	which may occur during a UART receive.
 */
struct uart_recv_error {
	
	/**
	 *	Data was lost.
	 */
	bool overrun;
	/**
	 *	There was a frame error.
	 */
	bool frame_error;
	/**
	 *	There was an error with the parity
	 *	bit of a frame.
	 */
	bool parity_error;
	
};


/**
 *	Initializes a UART.
 *
 *	Once this function has returned successfully calling
 *	this function with the same \em uart before calling
 *	uart_cleanup results in undefined behaviour.
 *
 *	\param [in] uart
 *		The UART to initialize.
 *	\param [in] opt
 *		A structure indicating the options with which
 *		\em uart shall be initialized.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int uart_init (uart_t uart, struct uart_opt opt);
/**
 *	Cleans up a UART.
 *
 *	If this function is invoked on a UART for which
 *	uart_init has not returned successfully, or which
 *	has been cleaned up but not reinitialized, the
 *	behaviour is undefined.
 *
 *	This call shall fail only if \em uart is invalid.
 *
 *	\param [in] uart
 *		The UART to clean up.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int uart_cleanup (uart_t uart);
/**
 *	Sends binary data over a UART.
 *
 *	If uart_init has not been invoked for this UART,
 *	or uart_cleanup has been invoked for this UART since
 *	the last invocation of uart_init, then the results
 *	of this function call are undefined.
 *
 *	If the UART is in synchronous mode this call will
 *	not return until all bytes have been sent (or buffered
 *	to be sent).
 *
 *	Calling this function with the same \em uart on multiple
 *	threads simultaneously results in undefined behaviour.
 *
 *	\param [in] uart
 *		The UART to send on.
 *	\param [in] buf
 *		A pointer to binary data to send.
 *	\param [in] buflen
 *		The number of bytes which shall be sent from
 *		\em buf.
 *	\param [out] sent
 *		An integer which shall be set to the number of bytes
 *		actually sent.  This value shall be set whether the
 *		call succeeds or not.  \em NULL may be provided in
 *		which case this parameter is ignored.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int uart_send (uart_t uart, const void * buf, size_t buflen, size_t * sent);
/**
 *	Receives binary data over a UART.
 *
 *	If uart_init has not been invoked for this UART, or
 *	uart_cleanup has been invoked for this UART since
 *	the last invocation of uart_init, then the results
 *	of this function call are undefined.
 *
 *	If the UART is in synchronous mode this call will not
 *	return until all requested bytes have been received.
 *
 *	Calling this function with the same \em uart on
 *	multiple threads simultaneously results in undefined
 *	behaviour.
 *
 *	\param [in] uart
 *		The UART to receive on.
 *	\param [in] buf
 *		A pointer to a buffer into which data shall be
 *		received.
 *	\param [in] buflen
 *		The number of bytes which shall be received into
 *		\em buf.
 *	\param [out] recvd
 *		An integer which shall be set to the number of
 *		bytes actually received.  This value shall be
 *		set whether the call succeds or not.  \em NULL
 *		may be provided in which case this parameter
 *		is ignored.
 *	\param [out] err
 *		A pointer to a structure which receives the
 *		error conditions (if any) set on \em uart.
 *		All members of this structure shall be set
 *		whether the call succeeds or not.  \em NULL
 *		may be provided in which case this parameter
 *		is ignored.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int uart_recv (uart_t uart, void * buf, size_t buflen, size_t * recvd, struct uart_recv_error * err);
