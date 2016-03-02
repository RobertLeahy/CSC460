#include <kernel.h>
#include <os.h>
#include <stddef.h>
#include <string.h>


#define SYSCALL_PUSH(var,buffer,i) do { memcpy(((unsigned char *)(buffer))+i,&(var),sizeof((var)));i+=sizeof((var)); } while (0)


error_t * get_last_error (void) {
	
	return &current_thread->last_error;
	
}


#ifdef DEBUG
unsigned int get_last_syscall (void) {
	
	return current_thread->last_syscall;
	
}
#endif


int thread_create (thread_t * thread, void (*f) (void *), priority_t prio, void * arg) {
	
	unsigned char buffer [sizeof(thread)+sizeof(f)+sizeof(prio)+sizeof(arg)];
	size_t i=0;
	SYSCALL_PUSH(thread,buffer,i);
	SYSCALL_PUSH(f,buffer,i);
	SYSCALL_PUSH(prio,buffer,i);
	SYSCALL_PUSH(arg,buffer,i);
	
	return syscall(SYSCALL_THREAD_CREATE,buffer,sizeof(buffer));
	
}


thread_t thread_self (void) {
	
	thread_t retr;
	thread_t * ptr=&retr;
	unsigned char buffer [sizeof(thread_t *)];
	size_t i=0;
	SYSCALL_PUSH(ptr,buffer,i);
	
	syscall(SYSCALL_THREAD_SELF,buffer,sizeof(buffer));
	
	return retr;
	
}


int thread_set_priority (thread_t thread, priority_t prio) {
	
	unsigned char buffer [sizeof(thread)+sizeof(prio)];
	size_t i=0;
	SYSCALL_PUSH(thread,buffer,i);
	SYSCALL_PUSH(prio,buffer,i);
	
	return syscall(SYSCALL_THREAD_SET_PRIORITY,buffer,sizeof(buffer));
	
}


static int thread_helper (enum syscall sc, thread_t thread) {
	
	unsigned char buffer [sizeof(thread)];
	size_t i=0;
	SYSCALL_PUSH(thread,buffer,i);
	
	return syscall(sc,buffer,sizeof(buffer));
	
}


int thread_suspend (thread_t thread) {
	
	return thread_helper(SYSCALL_THREAD_SUSPEND,thread);
	
}


int thread_resume (thread_t thread) {
	
	return thread_helper(SYSCALL_THREAD_RESUME,thread);
	
}


int mutex_create (mutex_t * mutex) {
	
	unsigned char buffer [sizeof(mutex)];
	size_t i=0;
	SYSCALL_PUSH(mutex,buffer,i);
	
	return syscall(SYSCALL_MUTEX_CREATE,buffer,sizeof(buffer));
	
}


static int mutex_helper (enum syscall sc, mutex_t mutex) {
	
	unsigned char buffer [sizeof(mutex)];
	size_t i=0;
	SYSCALL_PUSH(mutex,buffer,i);
	
	return syscall(sc,buffer,sizeof(buffer));
	
}


int mutex_destroy (mutex_t mutex) {
	
	return mutex_helper(SYSCALL_MUTEX_DESTROY,mutex);
	
}


int mutex_lock (mutex_t mutex) {
	
	return mutex_helper(SYSCALL_MUTEX_LOCK,mutex);
	
}


int mutex_unlock (mutex_t mutex) {
	
	return mutex_helper(SYSCALL_MUTEX_UNLOCK,mutex);
	
}


int event_create (event_t * event) {
	
	unsigned char buffer [sizeof(event)];
	size_t i=0;
	SYSCALL_PUSH(event,buffer,i);
	
	return syscall(SYSCALL_EVENT_CREATE,buffer,sizeof(buffer));
	
}


static int event_helper (enum syscall sc, event_t event) {
	
	unsigned char buffer [sizeof(event)];
	size_t i=0;
	SYSCALL_PUSH(event,buffer,i);
	
	return syscall(sc,buffer,sizeof(buffer));
	
}


int event_destroy (event_t event) {
	
	return event_helper(SYSCALL_EVENT_DESTROY,event);
	
}


int event_wait (event_t event) {
	
	return event_helper(SYSCALL_EVENT_WAIT,event);
	
}


int event_signal (event_t event) {
	
	error_t err;
	int retr=event_signal_r(event,&err);
	errno=err;
	
	return retr;
	
}


int event_signal_r (event_t event, error_t * err) {
	
	unsigned char buffer [sizeof(event)+sizeof(err)];
	size_t i=0;
	SYSCALL_PUSH(event,buffer,i);
	SYSCALL_PUSH(err,buffer,i);
	
	return syscall(SYSCALL_EVENT_SIGNAL,buffer,sizeof(buffer));
	
}


int sleep (struct timespec ts) {
	
	unsigned char buffer [sizeof(ts)];
	size_t i=0;
	SYSCALL_PUSH(ts,buffer,i);
	
	return syscall(SYSCALL_SLEEP,buffer,sizeof(buffer));
	
}


void yield (void) {
	
	syscall(SYSCALL_YIELD,0,0);
	
}
