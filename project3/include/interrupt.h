/**
 *	\file
 */


#pragma once


#include <avr/common.h>
#include <avr/interrupt.h>
#include <stdbool.h>


/**
 *	Determines the current state of the global
 *	interrupt flag.
 *
 *	\return
 *		\em true if the global interrupt flag is
 *		set, \em false otherwise.
 */
static inline bool interrupt_enabled (void) {
	
	return (SREG&128U)!=0;
	
}


/**
 *	Clears the global interrupt flag.
 */
static inline void disable_interrupt (void) {
	
	cli();
	
}


/**
 *	Sets the global interrupt flag.
 */
static inline void enable_interrupt (void) {
	
	sei();
	
}


/**
 *	Clears the global interrupt flag and returns
 *	the result of interrupt_enabled as obtained before
 *	clearing the global interrupt flag (i.e. whether
 *	interrupts were enabled or not when this function
 *	was entered).
 *
 *	\return
 *		\em true if interrupts were enabled when this
 *		function was called, \em false otherwise.
 */
static inline bool disable_and_push_interrupt (void) {
	
	bool retr=interrupt_enabled();
	
	disable_interrupt();
	
	return retr;
	
}


/**
 *	Sets the global interrupt flag and returns
 *	the result of interrupt_enabled as obtained
 *	before setting the global interrupt flag (i.e.
 *	whether interrupts were enabled or not when
 *	this function was entered).
 *
 *	\return
 *		\em true if interrupts were enabled when
 *		this function was called, \em false otherwise.
 */
static inline bool enable_and_push_interrupt (void) {
	
	bool retr=interrupt_enabled();
	
	enable_interrupt();
	
	return retr;
	
}


/**
 *	Sets the global interrupt flag if its argument
 *	is \em true, clears the global interrupt flag
 *	otherwise.
 *
 *	\param [in] state
 *		The new state of the global interrupt flag.
 */
static inline void pop_interrupt (bool state) {
	
	if (state) enable_interrupt();
	else disable_interrupt();
	
}
