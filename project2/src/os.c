#include <kernel.h>
#include <os.h>
#include <string.h>


error_t * get_last_error (void) {
	
	if (current_thread) return &current_thread->last_error;
	
	return &last_error;
	
}


int thread_create (thread_t * thread, void (*f) (void *), priority_t prio, void * arg) {
	
	unsigned char buffer [sizeof(thread)+sizeof(f)+sizeof(prio)+sizeof(arg)];
	memcpy(buffer,&thread,sizeof(thread));
	memcpy(&buffer[sizeof(thread)],&f,sizeof(f));
	memcpy(&buffer[sizeof(thread)+sizeof(f)],&prio,sizeof(prio));
	memcpy(&buffer[sizeof(thread)+sizeof(f)+sizeof(prio)],&arg,sizeof(arg));
	
	return syscall(SYSCALL_THREAD_CREATE,buffer,sizeof(buffer));
	
}


void yield (void) {
	
	syscall(SYSCALL_YIELD,0,0);
	
}
