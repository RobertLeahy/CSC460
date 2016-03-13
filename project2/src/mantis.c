#include <interrupt.h>
#include <mantis.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <os.h>


#ifdef PROVIDED_API


//	This is a hack: There's no actual way
//	to shut the kernel down
void OS_Abort (void) {
	
	disable_interrupt();
	
	for (;;) asm volatile ("sleep");
	
}


struct task_create_state {
	
	void (*f) (void);
	int arg;
	event_t start;
	event_t handshake;
	bool success;
	error_t err;
	thread_t handle;
	
};


static PID pid_from_thread (thread_t handle) {
	
	return handle;
	
}


static thread_t thread_from_pid (PID handle) {
	
	return handle;
	
}


static int args [MAXTHREAD];


static void task_start (void * ptr) {
	
	struct task_create_state * s=(struct task_create_state *)ptr;
	
	//	Wait for the structure to be fully populated
	//	by the thread that created us
	if (event_wait(s->start)!=0) return;
	
	//	If there was some failure in the creating thread
	//	abandon ship
	if (!s->success) return;
	
	do {
		
		size_t i=s->handle-1U;
		if (i>=MAXTHREAD) {
			
			s->success=false;
			//	Should this be EINVAL, EAGAIN, or
			//	ERANGE?
			s->err=EAGAIN;
			
			break;
			
		}
		
		//	Copy the argument into the global array
		//	that holds them
		args[i]=s->arg;
		
		//	Copy out the function pointer (the pointed
		//	to structure will vanish (hypothetically/in
		//	the worst case) as soon as we signal the thread
		//	that created us)
		void (*f) (void)=s->f;
		
		//	Allow the creating thread to continue, we don't
		//	check the return value because there's nothing
		//	we can do if it fails at this point (and it
		//	shouldn't fail since assumedly the handle is
		//	valid)
		event_signal(s->handshake);
		
		f();
		
	} while (0);
	
	//	Wake up the thread that created us
	event_signal(s->handshake);
	
}


PID Task_Create (void (*f) (void), PRIORITY py, int arg) {
	
	struct task_create_state s;
	memset(&s,0,sizeof(s));
	s.f=f;
	s.arg=arg;
	if (event_create(&s.start)!=0) return 0;
	
	PID retr=0;
	do {
		
		if (event_create(&s.handshake)!=0) break;
		
		do {
			
			thread_t handle;
			if (thread_create(&handle,task_start,py,&s)!=0) break;
			
			s.handle=handle;
			s.success=true;
			//	If this happens the thread is created but there's
			//	no reliable way to communicate that failure to it:
			//	Fortunately this can only fail if the handle is
			//	invalid, which shouldn't happen
			if (event_signal(s.start)!=0) break;
			
			//	We wait for the created thread to be finished with
			//	the structure, same deal as above applies to error
			//	checking etc.
			if (event_wait(s.handshake)!=0) break;
			
			if (s.success) retr=pid_from_thread(handle);
			
		} while (0);
		
		event_destroy(s.handshake);
		
	} while (0);
	
	event_destroy(s.start);
	
	return retr;
	
}


void Task_Terminate (void) {
	
	//	I hate the concept of this function, it encourages
	//	shoddy programming
	for (;;) yield();
	
}


void Task_Yield (void) {
	
	yield();
	
}


int Task_GetArg (void) {
	
	size_t i=thread_self()-1U;
	//	There's no way to actually report errors
	//	from these functions so we'll just return
	//	negative one in the case of an error
	if (i>=MAXTHREAD) return -1;
	
	return args[i];
	
}


void Task_Suspend (PID p) {
	
	//	No way to report errors so don't
	thread_suspend(thread_from_pid(p));
	
}


void Task_Resume (PID p) {
	
	//	No way to report errors so don't
	thread_resume(thread_from_pid(p));
	
}


void Task_Sleep (TICK t) {
	
	struct timespec ts;
	memset(&ts,0,sizeof(ts));
	ts.tv_sec=t/(1000U/MSECPERTICK);
	ts.tv_nsec=t%(1000U/MSECPERTICK);
	ts.tv_nsec*=MSECPERTICK;
	ts.tv_nsec*=1000000ULL;
	
	sleep(ts);
	
}


static MUTEX from_mutex (mutex_t m) {
	
	return m+1U;
	
}


static mutex_t to_mutex (MUTEX m) {
	
	return m-1U;
	
}


MUTEX Mutex_Init (void) {
	
	mutex_t handle;
	if (mutex_create(&handle)!=0) return 0;
	
	return from_mutex(handle);
	
}


void Mutex_Lock (MUTEX m) {
	
	//	No way to report an error so just
	//	ignore one if it occurs I guess?
	mutex_lock(to_mutex(m));
	
}


void Mutex_Unlock (MUTEX m) {
	
	//	No way to report an error so just
	//	ignore one if it occurs I guess?
	mutex_unlock(to_mutex(m));	
	
}


static EVENT from_event (event_t e) {
	
	return e+1U;
	
}


static event_t to_event (EVENT e) {
	
	return e-1U;
	
}


EVENT Event_Init (void) {
	
	event_t handle;
	if (event_create(&handle)!=0) return 0;
	
	return from_event(handle);
	
}


void Event_Wait (EVENT e) {
	
	//	No way to report an error so just
	//	ignore one if it occurs I guess?
	event_wait(to_event(e));
	
}


void Event_Signal (EVENT e) {
	
	//	No way to report an error so just
	//	ignore one if it occurs I guess?
	//
	//	Use the reentrant version so this
	//	function can be called from an ISR
	event_signal_r(to_event(e),0);
	
}


#endif
