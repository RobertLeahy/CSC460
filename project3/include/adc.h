/**
 *	\file
 */


#pragma once


#include <stdint.h>


/**
 *	Initializes the analog to digital conversion
 *	system.
 */
void adc_init (void);
/**
 *	Reads a 10 bit value from a particular analog
 *	to digital conversion channel.
 *
 *	\param [in] c
 *		The channel from which to read.
 *
 *	\return
 *		The value read.
 */
uint16_t adc_read (unsigned char c);
