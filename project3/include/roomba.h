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


enum roomba_packet_id {
	
	ROOMBA_BUMPS_AND_WHEEL_DROPS=7,
	ROOMBA_VIRTUAL_WALL=13
	
};


/**
 *	Represents a Roomba "Bumps and Wheel Drops"
 *	packet.
 */
struct roomba_bumps_and_wheel_drops {
	
	bool wheel_drop_left;
	bool wheel_drop_right;
	bool bump_left;
	bool bump_right;
	
};


/**
 *	Represents a Roomba "Virtual Wall" packet.
 */
struct roomba_virtual_wall {
	
	bool virtual_wall;
	
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
/**
 *	Controls the speed of the Roomba's wheels directly.
 *
 *	\param [in] r
 *		A pointer to the roomba structure upon which to
 *		operate.
 *	\param [in] r_velocity
 *		The velocity of the right wheel in millimetres per
 *		second.
 *	\param [in] l_velocity
 *		The velocity of the left wheel in millimetres per
 *		second.
 *
 *	\return
 *		0 if the call succeeded, -1 otherwise.
 */
int roomba_drive_direct (struct roomba * r, int16_t r_velocity, int16_t l_velocity);
/**
 *	Requests a sensor packet from the Roomba.
 *
 *	\param [in] r
 *		A pointer to the roomba structure upon which to
 *		operate.
 *	\param [in] id
 *		The ID of the packet to request.
 *	\param [out] ptr
 *		A pointer to the structure which shall receive sensor
 *		data.  If this does not point to a structure which
 *		correlates with \em id then the behaviour is undefined.
 *
 *	\return
 *		0 if the call succeeded, -1 otherwise.
 */
int roomba_sensors (struct roomba * r, enum roomba_packet_id id, void * ptr);
