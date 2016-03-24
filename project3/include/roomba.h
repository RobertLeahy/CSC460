/**
 *	\file
 */


#pragma once


#include <stdbool.h>
#include <stdint.h>
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
/**
 *	Causes the Roomba to enter a clean cycle.
 *
 *	\param [in] r
 *		A pointer to a roomba structure upon which to operate.
 *
 *	\return
 *		0 if the call succeeded, -1 otherwise.
 */
int roomba_clean (struct roomba * r);
/**
 *	Places the Roomba in safe mode.
 *
 *	\param [in] r
 *		A pointer to the roomba structure upon which to
 *		operate.
 *
 *	\return
 *		0 if the call succeeded, -1 otherwise.
 */
int roomba_safe (struct roomba * r);
/**
 *	Starts the Roomba driving.
 *
 *	\param [in] r
 *		A pointer to the roomba structure upon which to
 *		operate.
 *	\param [in] velocity
 *		The average velocity at which the Roomba should
 *		travel in millimetres per second.
 *	\param [in] radius
 *		The radius of the turn in millimetres.  If a radius
 *		of zero is specified the Roomba will drive straight.
 *		Positive radii cause the Roomba to turn left whereas
 *		negative radii cause the Roomba to turn right.
 *
 *	\return
 *		0 if the call succeeded, -1 otherwise.
 */
int roomba_drive (struct roomba * r, int16_t velocity, int16_t radius);
/**
 *	Causes the Roomba to turn in place.
 *
 *	\param [in] r
 *		A pointer to the roomba structure upon which to
 *		operate.
 *	\param [in] velocity
 *		The average velocity at which the Roomba should
 *		travel in millimetres per second.
 *	\param [in] left
 *		\em true if the Roomba should turn left, \em false
 *		otherwise.
 *
 *	\return
 *		0 if the call succeeded, -1 otherwise.
 */
int roomba_turn (struct roomba * r, int16_t velocity, bool left);
