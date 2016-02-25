/**
 *	\file
 */


#pragma once


#include <stddef.h>


/**
 *	The type of a handle to a thread.
 */
typedef size_t thread_t;
/**
 *	The type of a mutex.
 */
typedef size_t mutex_t;
/**
 *	An integer type which represents a thread's
 *	priority.
 */
typedef unsigned char priority_t;
/**
 *	The type of an event.
 */
typedef size_t event_t;
/**
 *	An integer type which represents ticks.
 */
typedef unsigned int tick_t;


/**
 *	The type which represents an error.
 */
typedef enum {
	ENONE=0,
	EAGAIN,
	EINVAL,
	EPERM,
	EDEADLK
} error_t;
error_t * get_last_error (void);
/**
 *	The current thread's last error.
 */
#define errno (*get_last_error())


#ifdef DEBUG
unsigned int get_last_syscall (void);
#endif


/**
 *	Creates and starts a new thread.
 *
 *	\param [out] thread
 *		A pointer to a thread handle which will
 *		be initialized if the call succeeds.  If
 *		the call fails this shall not be modified.
 *		\em NULL may be provided in which case the
 *		handle will not be returned to the caller.
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
/**
 *	Retrieves a handle for the currently executing
 *	thread.
 *
 *	\return
 *		A handle for the currently executing thread.
 */
thread_t thread_self (void);
/**
 *	Sets the priority of a particuler thread.
 *
 *	\param [in] thread
 *		A handle to the thread-in-question.
 *	\param [in] prio
 *		The new priority of \em thread.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int thread_set_priority (thread_t thread, priority_t prio);


/**
 *	Creates a mutex.
 *
 *	\param [out] mutex
 *		A pointer to a mutex handle which will be
 *		initialized in the call succeeds.  If the call
 *		fails this shall not be modified.  May not be
 *		\em NULL.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int mutex_create (mutex_t * mutex);
/**
 *	Destroys a mutex.
 *
 *	If the mutex currently has threads waiting on it
 *	the behaviour is undefined.
 *
 *	This call shall only fail if \em mutex does not
 *	represent a valid mutex.
 *
 *	\param [in] mutex
 *		A mutex handle representing the mutex to be
 *		destroyed.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int mutex_destroy (mutex_t mutex);
/**
 *	Locks a mutex.
 *
 *	This call shall only fail if \em mutex does not
 *	represent a valid mutex.
 *
 *	\param [in] mutex
 *		A mutex handle representing the mutex to lock.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int mutex_lock (mutex_t mutex);
/**
 *	Unlocks a mutex.
 *
 *	This call shall only fail if \em mutex does not
 *	represent a valid mutex or if the calling thread
 *	does not own the mutex indicated by \em mutex.
 *
 *	\param [in] mutex
 *		A mutex handle representing the mutex to lock.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int mutex_unlock (mutex_t mutex);


/**
 *	Yields control of the CPU back to the kernel for
 *	rescheduling.
 */
void yield (void);
