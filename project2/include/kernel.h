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
	RUNNING,
	BLOCKED
	
};


/**
 *	An enumeration of valid syscalls.
 */
enum syscall {
	
	SYSCALL_YIELD,
	SYSCALL_THREAD_CREATE,
	SYSCALL_THREAD_SELF,
	SYSCALL_THREAD_SET_PRIORITY,
	SYSCALL_MUTEX_CREATE,
	SYSCALL_MUTEX_DESTROY,
	SYSCALL_MUTEX_LOCK,
	SYSCALL_MUTEX_UNLOCK,
	SYSCALL_EVENT_CREATE,
	SYSCALL_EVENT_DESTROY,
	SYSCALL_EVENT_WAIT,
	SYSCALL_EVENT_SIGNAL
	
};


struct kthread;
//	Implementation detail
struct kthread_node {
	
	struct kthread * next;
	
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
	 *	Indicates the priority of this thread.
	 */
	priority_t priority;
	/**
	 *	The last error which occurred on the thread.
	 */
	error_t last_error;
	/**
	 *	The last syscall which was made on this thread.
	 */
	enum syscall last_syscall;
	
	//	Implementation details
	struct kthread_node queue;
	struct kthread_node wait;
	
};


/**
 *	A pointer to the current thread, or \em NULL
 *	if there is no current thread.
 */
extern struct kthread * current_thread;


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
