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


int syscall (enum syscall num, unsigned char * args, size_t len);
