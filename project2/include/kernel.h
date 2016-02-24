/**
 *	\file
 */


#pragma once


#include <os.h>
#include <stddef.h>


/**
 *	The size of a thread's stack.
 */
#define STACK_SIZE (256U)


/**
 *	Represents the various states a thread
 *	may be in.
 */
enum thread_state {
	
	DEAD=0,
	READY,
	RUNNING
	
};


/**
 *	An enumeration of valid syscalls.
 */
enum syscall {
	
	SYSCALL_YIELD,
	SYSCALL_THREAD_CREATE
	
};


/**
 *	Represents a thread.
 */
struct kthread {
	
	/**
	 *	This thread's stack pointer.
	 */
	unsigned char * sp;
	/**
	 *	This thread's stack.
	 */
	unsigned char stack [STACK_SIZE];
	/**
	 *	This thread's state.
	 */
	enum thread_state state;
	/**
	 *	The function to invoke in the thread.
	 */
	void (*start_routine) (void *);
	/**
	 *	The argument to pass through to
	 *	\em start_routine when it is invoked.
	 */
	void * arg;
	/**
	 *	Indicates the status of the last syscall
	 *	on this thread.
	 */
	error_t last_error;
	/**
	 *	Indicates the priority of this thread.
	 */
	priority_t priority;
	
};


/**
 *	A pointer to the current thread, or \em NULL
 *	if there is no current thread.
 */
extern struct kthread * current_thread;
/**
 *	The address of the lvalue yielded by the \em errno
 *	macro when \em current_thread is \em NULL.
 */
extern error_t last_error;


/**
 *	Performs a syscall.
 *
 *	\param [in] num
 *		The syscall to perform.
 *	\param [in] args
 *		A pointer to a buffer which contains the
 *		arguments to the syscall.
 *	\param [in] len
 *		The length of \em args in bytes.
 *
 *	\return
 *		0 if the syscall succeeded, -1 otherwise.
 */
int syscall (enum syscall num, void * args, size_t len);
