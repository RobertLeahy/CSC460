/**
 *	\file
 */


#pragma once


/**
 *	The type of a handle to a thread.
 */
typedef unsigned int thread_t;
/**
 *	The type of a mutex.
 */
typedef unsigned int mutex_t;
/**
 *	An integer type which represents a thread's
 *	priority.
 */
typedef unsigned char priority_t;
/**
 *	The type of an event.
 */
typedef unsigned int event_t;
/**
 *	An integer type which represents ticks.
 */
typedef unsigned int tick_t;


/**
 *	The type which represents an error.
 */
typedef enum {
	ENONE=0,
	EAGAIN
} error_t;
error_t * get_last_error (void);
/**
 *	The current thread's last error.
 */
#define errno (*get_last_error())


/**
 *	Creates and starts a new thread.
 *
 *	\param [out] thread
 *		A pointer to a thread handle which will
 *		be initialized if the call succeeds.  If
 *		the call fails this shall not be modified.
 *	\param [in] f
 *		The function to invoke in the created
 *		thread.
 *	\param [in] prio
 *		The priority of the newly created thread.
 *	\param [in] arg
 *		A pointer which shall be passed as the sole
 *		argument to \em f.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int thread_create (thread_t * thread, void (*f) (void *), priority_t prio, void * arg);
