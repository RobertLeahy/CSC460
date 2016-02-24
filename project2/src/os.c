#include <kernel.h>
#include <os.h>
#include <stddef.h>
#include <string.h>


#define SYSCALL_PUSH(var,buffer,i) do { memcpy(&(buffer)[i],&(var),sizeof((var)));i+=sizeof((var)); } while (0)


error_t * get_last_error (void) {
	
	if (current_thread) return &current_thread->last_error;
	
	return &last_error;
	
}


int thread_create (thread_t * thread, void (*f) (void *), priority_t prio, void * arg) {
	
	unsigned char buffer [sizeof(thread)+sizeof(f)+sizeof(prio)+sizeof(arg)];
	size_t i=0;
	SYSCALL_PUSH(thread,buffer,i);
	SYSCALL_PUSH(f,buffer,i);
	SYSCALL_PUSH(prio,buffer,i);
	SYSCALL_PUSH(arg,buffer,i);
	
	return syscall(SYSCALL_THREAD_CREATE,buffer,sizeof(buffer));
	
}


void yield (void) {
	
	syscall(SYSCALL_YIELD,0,0);
	
}
