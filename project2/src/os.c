#include <kernel.h>
#include <os.h>


error_t * get_last_error (void) {
	
	if (current_thread) return &current_thread->last_error;
	
	return &last_error;
	
}


int thread_create (thread_t * thread, void (*f) (void *), priority_t prio, void * arg) {
	
	return kthread_create(thread,f,prio,arg);
	
}
