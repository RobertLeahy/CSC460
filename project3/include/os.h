/**
 *	\file
 */


#pragma once


#include <stddef.h>


#ifdef PROVIDED_API
#include <mantis.h>
#endif


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
	EAGAIN=1,
	EINVAL=2,
	EPERM=3,
	EDEADLK=4,
	EBUSY=5,
	EOPNOTSUPP=6,
	EALREADY=7
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
 *	Gets the priority of a particular thread.
 *
 *	\param [in] thread
 *		A handle to the thread-in-question.
 *	\param [out] prio
 *		A pointer to an object which shall be initialized
 *		with the priority of \em thread.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int thread_get_priority (thread_t thread, priority_t * prio);
/**
 *	Gets the effective priority of a particular thread.
 *
 *	The effective priority differs from the priority of a
 *	thread when the kernel has seen fit to boost the priority
 *	of a thread temporarily.
 *
 *	\param [in] thread
 *		A handle to the thread-in-question.
 *	\param [out] prio
 *		A pointer to an object which shall be initialized
 *		with the effective priority of \em thread.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int thread_get_effective_priority (thread_t thread, priority_t * prio);
/**
 *	Suspends a thread.
 *
 *	A suspended thread will be descheduled and will
 *	not be rescheduled until it is resumed.
 *
 *	This function will fail if \em thread is an invalid
 *	thread handle or if \em thread is already suspended.
 *
 *	\param [in] thread
 *		A handle to the thread-in-question.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int thread_suspend (thread_t thread);
/**
 *	Resumes a thread.
 *
 *	This function will fail if \em thread is an invalid
 *	thread handle or if \em thread has not been suspended.
 *
 *	\param [in] thread
 *		A handle to the thread-in-question.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int thread_resume (thread_t thread);


/**
 *	Creates a mutex.
 *
 *	\param [out] mutex
 *		A pointer to a mutex handle which will be
 *		initialized if the call succeeds.  If the call
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
 *	Creates an event.
 *
 *	\param [out] event
 *		A pointer to an event handle which will be
 *		initialized if the call succeeds.  If the call
 *		fails this shall not be modified.  May not be
 *		\em NULL.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int event_create (event_t * event);
/**
 *	Destroys an event.
 *
 *	If the event currently has a thread waiting on it
 *	the behaviour is undefined.
 *
 *	This call shall only fail if \em event does not
 *	represent a valid event.
 *
 *	\param [in] event
 *		An event handle representing the event to be
 *		destroyed.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int event_destroy (event_t event);
/**
 *	Waits for an event.
 *
 *	If the event has already occurred control will
 *	return to the calling thread immediately,
 *	otherwise the thread will block until the event
 *	occurs.
 *
 *	This call shall only fail if \em event does not
 *	represent a valid event or if some other thread
 *	is already waiting on the event.
 *
 *	\param [in] event
 *		An event handle representing the event to
 *		wait for.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int event_wait (event_t event);
/**
 *	Signals an event.
 *
 *	This call shall only fail if \em event does not
 *	represent a valid event.
 *
 *	\param [in] event
 *		The event to signal.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int event_signal (event_t event);
/**
 *	Signals an event.
 *
 *	This call shall only fail if \em event does not
 *	represent a valid event.
 *
 *	This function is reentrant and as such does not
 *	set \em errno.
 *
 *	\param [in] event
 *		The event to signal.
 *	\param [out] err
 *		A pointer to an error code which shall receive
 *		an error code indicating the result of the
 *		function call.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int event_signal_r (event_t event, error_t * err);


/**
 *	A structure indicating a high precision amount
 *	of time.
 */
struct timespec {
	
	/**
	 *	Seconds.
	 */
	unsigned int tv_sec;
	_Static_assert(sizeof(unsigned long)==4,"unsigned long not exactly 32 bits");
	/**
	 *	Nanoseconds.
	 */
	unsigned long tv_nsec;
	
};
/**
 *	Puts the current thread to sleep for at least
 *	the amount of time indicates by \em ts.
 *
 *	Note that there is no guarantee that exactly
 *	\em ts will pass until this thread runs again,
 *	only that at least \em ts will pass.
 *
 *	\param [in] ts
 *		A structure representing the minimum amount
 *		of time to sleep for.
 *
 *	\return
 *		0 if this call succeeds, -1 otherwise.
 */
int sleep (struct timespec ts);


/**
 *	Yields control of the CPU back to the kernel for
 *	rescheduling.
 */
void yield (void);
