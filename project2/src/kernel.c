#include <avr/common.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <kernel.h>
#include <limits.h>
#include <os.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>


#define MAX_THREADS (17U)	//	16 plus 1 for the dummy idle thread
#define MAX_MUTEXES (8U)
#define MAX_EVENTS (8U)
//	This is the number of bytes that are
//	popped when restoring a context:
//
//	-	32 8 bit general purpose registers
//	-	1 8 bit status register
//	-	RAMPZ (8 bits)
//	-	EIND (8 bits)
#define RESTORE_POP (35U)
//	The priority of the idle task
#define IDLE_PRIO (0U)
//	The priority of the main thread
#define MAIN_PRIO (0U)


//	Disables/enables interrupts
#define	disable_interrupt() asm volatile ("cli"::)
#define enable_interrupt() asm volatile ("sei"::)


static struct kthread threads [MAX_THREADS];
struct kthread * current_thread;
static error_t last_error;
static void * __attribute__((used)) ksp;
static bool quantum_expired;
static struct {
	
	enum syscall num;
	void * args;
	size_t len;
	
} syscall_state;


struct kthread_linked_list {
	
	struct kthread * head;
	struct kthread * tail;
	
};


static struct kthread_linked_list thread_queue;


struct kmutex {
	
	bool valid;
	struct kthread * owner;
	unsigned int count;
	struct kthread_linked_list queue;
	
};
static struct kmutex mutexes [MAX_MUTEXES];


struct kevent {
	
	bool valid;
	bool set;
	struct kthread * wait;
	
};
static struct kevent events [MAX_EVENTS];


//	_A suffix because otherwise collides
//	with AVR provided macros (why "_A"
//	specifically?  Because these are the
//	addresses if the registers in question
#define SREG_A "0x3F"
#define SPH_A "0x3E"
#define SPL_A "0x3D"
#define RAMPZ_A "0x3B"
#define EIND_A "0x3C"
#define SAVE_CTX asm volatile (	\
	"push r0\r\n"	\
	"push r1\r\n"	\
	"push r2\r\n"	\
	"push r3\r\n"	\
	"push r4\r\n"	\
	"push r5\r\n"	\
	"push r6\r\n"	\
	"push r7\r\n"	\
	"push r8\r\n"	\
	"push r9\r\n"	\
	"push r10\r\n"	\
	"push r11\r\n"	\
	"push r12\r\n"	\
	"push r13\r\n"	\
	"push r14\r\n"	\
	"push r15\r\n"	\
	"push r16\r\n"	\
	"push r17\r\n"	\
	"push r18\r\n"	\
	"push r19\r\n"	\
	"push r20\r\n"	\
	"push r21\r\n"	\
	"push r22\r\n"	\
	"push r23\r\n"	\
	"push r24\r\n"	\
	"push r25\r\n"	\
	"push r26\r\n"	\
	"push r27\r\n"	\
	"push r28\r\n"	\
	"push r29\r\n"	\
	"push r30\r\n"	\
	"push r31\r\n"	\
	"in r16," SREG_A "\r\n"	\
	"push r16\r\n"	\
	"in r16," RAMPZ_A "\r\n"	\
	"push r16\r\n"	\
	"in r16," EIND_A "\r\n"	\
	"push r16"	\
)
#define RESTORE_CTX asm volatile (	\
	"pop r16\r\n"	\
	"out " EIND_A ",r16\r\n"	\
	"pop r16\r\n"	\
	"out " RAMPZ_A ",r16\r\n"	\
	"pop r16\r\n"	\
	"out " SREG_A ",r16\r\n"	\
	"pop r31\r\n"	\
	"pop r30\r\n"	\
	"pop r29\r\n"	\
	"pop r28\r\n"	\
	"pop r27\r\n"	\
	"pop r26\r\n"	\
	"pop r25\r\n"	\
	"pop r24\r\n"	\
	"pop r23\r\n"	\
	"pop r22\r\n"	\
	"pop r21\r\n"	\
	"pop r20\r\n"	\
	"pop r19\r\n"	\
	"pop r18\r\n"	\
	"pop r17\r\n"	\
	"pop r16\r\n"	\
	"pop r15\r\n"	\
	"pop r14\r\n"	\
	"pop r13\r\n"	\
	"pop r12\r\n"	\
	"pop r11\r\n"	\
	"pop r10\r\n"	\
	"pop r9\r\n"	\
	"pop r8\r\n"	\
	"pop r7\r\n"	\
	"pop r6\r\n"	\
	"pop r5\r\n"	\
	"pop r4\r\n"	\
	"pop r3\r\n"	\
	"pop r2\r\n"	\
	"pop r1\r\n"	\
	"pop r0"	\
)
#define CURRENT_SP asm volatile (	\
	"lds r30, current_thread\r\n"	\
	"lds r31, current_thread+1"	\
)
#define IN_SP asm volatile (	\
	"in r28," SPL_A "\r\n"	\
	"in r29," SPH_A	\
)
#define OUT_SP asm volatile (	\
	"out " SPL_A ",r28\r\n"	\
	"out " SPH_A ",r29"	\
)


__attribute__((naked))
static void kenter_impl (void) {
	
	SAVE_CTX;
	CURRENT_SP;
	IN_SP;
	asm volatile ("st Z+,r28\r\nst Z+,r29");
	
	asm volatile ("lds r28,ksp\r\nlds r29,ksp+1");
	OUT_SP;
	RESTORE_CTX;
	
	asm volatile ("ret");
	
}


__attribute__((naked))
static void kexit_impl (void) {
	
	SAVE_CTX;
	IN_SP;
	asm volatile ("sts ksp,r28\r\nsts ksp+1,r29");
	
	CURRENT_SP;
	asm volatile ("ld r28,Z+\r\nld r29,Z+");
	OUT_SP;
	RESTORE_CTX;
	
	asm volatile ("ret");
	
}


static unsigned char push_interrupt (void) {
	
	unsigned char retr=SREG;
	disable_interrupt();
	
	return retr;
	
}


static void pop_interrupt (unsigned char sreg) {
	
	if ((sreg&128U)!=0) enable_interrupt();
	
}


//	These functions allows instrumenting calls
//	to/returns from kenter/kexit (since they're
//	not naked)
static void kenter (void) {
	
	#ifdef DEBUG
	unsigned char i=push_interrupt();
	//	About to leave user space, drag
	//	pin 23 low
	PORTA&=~(1<<PA1);
	#endif
	
	kenter_impl();
	
	#ifdef DEBUG
	pop_interrupt(i);
	//	Entered user space, drag pin 23
	//	high
	PORTA|=1<<PA1;
	#endif
	
}


static void kexit (void) {
	
	#ifdef DEBUG
	//	About to leave the kernel, drag
	//	pin 22 low
	PORTA&=~(1<<PA0);
	//	About to leave the kernel, that
	//	means the ISR/context switch is
	//	over
	PORTA&=~(1<<PA2);
	#endif
	
	kexit_impl();
	
	#ifdef DEBUG
	//	Entered the kernel, drag pin 22
	//	high
	PORTA|=1<<PA0;
	#endif
	
}


static void kidle (void * arg) {
	
	//	Unused: Always NULL
	(void)arg;
	
	//	Put the CPU to sleep
	for (;;) asm volatile ("sleep"::);
	
}


//	Initializes last_error to ENONE
static int klast_error_init (void) {
	
	last_error=ENONE;
	
	return 0;
	
}


//	Initializes global collection of mutexes
static int kmutex_init (void) {
	
	memset(mutexes,0,sizeof(mutexes));
	
	return 0;
	
}


//	Initializes events
static int kevent_init (void) {
	
	memset(events,0,sizeof(events));
	
	return 0;
	
}


//	Sets up timer for scheduling
static int ktimer_init (void) {
	
	TCCR1A=0;
	TCCR1B=0;
	TCNT1=0;
	
	#ifdef LARGE_QUANTUM
	OCR1A=15624;
	#else
	OCR1A=624;
	#endif
	
	TCCR1B|=(1<<WGM12);
	
	TCCR1B|=(1<<CS12);
	TCCR1B&=~(1<<CS11);
	#ifdef LARGE_QUANTUM
	TCCR1B|=(1<<CS10);
	#else
	TCCR1B&=~(1<<CS10);
	#endif
	
	TIMSK1|=(1<<OCIE1A);
	
	return 0;
	
}


static bool kthread_lower_priority (struct kthread * a, struct kthread * b) {
	
	if (a->priority<b->priority) return true;
	
	if (a->start_routine==&kidle) return true;
	
	return false;
	
}


static bool kthread_higher_priority (struct kthread * a, struct kthread * b) {
	
	if (a->priority>b->priority) return true;
	
	if (b->start_routine==&kidle) return true;
	
	return false;
	
}


static bool kthread_equal_priority (struct kthread * a, struct kthread * b) {
	
	if (a->priority!=b->priority) return false;
	
	if ((a->start_routine==&kidle) || (b->start_routine==&kidle)) return false;
	
	return true;
	
}


static void kthread_enqueue_impl (struct kthread * t) {
	
	//	Ignore threads that are not ready to
	//	run
	if (t->state!=READY) return;
	
	//	No threads in the queue add this one as the
	//	only thread
	if (!thread_queue.head) {
		
		thread_queue.head=t;
		return;
		
	}
	
	//	If this thread is already in the queue
	//	we need to make sure it's in the correct
	//	location
	if (t->queue.next || (thread_queue.tail==t)) {
		
		bool high_before=true;
		bool low_after=true;
		bool after=false;
		struct kthread * prev=0;
		for (struct kthread * loc=thread_queue.head;loc!=0;loc=loc->queue.next) {
			
			if (loc==t) {
				
				after=true;
				continue;
				
			}
			
			if (loc->queue.next==t) prev=loc;
			
			if (kthread_equal_priority(loc,t)) continue;
			if (after) low_after=kthread_lower_priority(loc,t);
			else high_before=kthread_higher_priority(loc,t);
			
		}
		
		//	In the correct place, we're done
		if (high_before && low_after) return;
		
		//	Wrong place, remove and re-add
		if (prev) {
			
			prev->queue.next=t->queue.next;
			if (!prev->queue.next) thread_queue.tail=prev;
			
		} else {
			
			thread_queue.head=t->queue.next;
			if (!thread_queue.head) thread_queue.tail=0;
			
		}
		
		t->queue.next=0;
		
		kthread_enqueue_impl(t);
		
		return;
		
	}
	
	//	New head
	if (kthread_lower_priority(thread_queue.head,t)) {
		
		t->queue.next=thread_queue.head;
		thread_queue.head=t;
		
		return;
		
	}
	
	//	Somewhere in the body
	struct kthread * after=0;
	for (struct kthread * loc=thread_queue.head;loc!=0;loc=loc->queue.next) {
		
		if (kthread_lower_priority(loc,t)) break;
		after=loc;
		
	}
	
	t->queue.next=after->queue.next;
	after->queue.next=t;
	if (!t->queue.next) thread_queue.tail=t;
	
}


static void kthread_enqueue (struct kthread * t) {
	
	kthread_enqueue_impl(t);
	current_thread=thread_queue.head;
	
}


static void kdispatch (void) {
	
	//	Loop until we find a viable thread to run
	do {
		
		//	Pop
		struct kthread * curr=thread_queue.head;
		current_thread=thread_queue.head=curr->queue.next;
		if (!thread_queue.head) thread_queue.tail=0;
		curr->queue.next=0;
		
		//	Re-enqueue previously running thread
		kthread_enqueue(curr);
		
	} while (current_thread->state!=READY);
	
}


static void kthread_start (void) {
	
	#ifdef DEBUG
	//	Now in user space, drag pin 23
	//	high
	PORTA|=1<<PA1;
	#endif
	
	//	Interrupts are enabled in non-kernel
	//	threads
	enable_interrupt();
	
	//	Run the thread
	current_thread->start_routine(current_thread->arg);
	
	//	Terminate the thread
	disable_interrupt();
	current_thread->state=DEAD;
	syscall(SYSCALL_YIELD,0,0);
	
}


static void kthread_create (thread_t * thread, void (*f) (void *), priority_t prio, void * arg) {
	
	//	Find an available thread control block
	thread_t avail;
	for (avail=0;avail<MAX_THREADS;++avail) if (threads[avail].state==DEAD) break;
	
	//	No available thread control block: There
	//	are MAX_THREADS threads running
	if (avail==MAX_THREADS) {
		
		last_error=EAGAIN;
		return;
		
	}
	
	//	Initialize thread control block
	struct kthread * t=&threads[avail];
	memset(t,0,sizeof(*t));
	t->state=READY;
	t->start_routine=f;
	t->arg=arg;
	t->priority=prio;
	
	//	Setup thread's stack with return
	//	address and values to pop for registers
	void (*start) (void)=kthread_start;
	_Static_assert(sizeof(start)==2,"Function pointer size not equal to 2");
	unsigned int fp;
	_Static_assert(sizeof(fp)==2,"unsigned int size not equal to 2");
	memcpy(&fp,&start,sizeof(fp));
	t->sp=&t->stack[STACK_SIZE-1U];
	*(t->sp--)=fp&0xFFU;
	*(t->sp--)=(fp>>8U)&0xFFU;
	*(t->sp--)=0;	//	ret/reti pop 3 bytes on Mega 2560
	t->sp-=RESTORE_POP;
	
	//	Put thread in queue of threads waiting to run
	kthread_enqueue(t);
	
	//	Return values to caller
	if (thread) *thread=avail;
	last_error=ENONE;
	
}


//	Initializes threading
static int kthread_init (void) {
	
	memset(threads,0,sizeof(threads));
	current_thread=0;
	memset(&thread_queue,0,sizeof(thread_queue));
	quantum_expired=false;
	
	//	Start idle task
	kthread_create(0,kidle,IDLE_PRIO,0);
	
	return 0;
	
}


static int kinit (void) {
	
	//	Interrupts disabled while in kernel
	disable_interrupt();
	
	if (
		(kthread_init()!=0) ||
		(klast_error_init()!=0) ||
		(ktimer_init()!=0) ||
		(kmutex_init()!=0) ||
		(kevent_init()!=0)
	) return -1;
	
	#ifdef DEBUG
	//	Configure debugging pins
	
	//	Port 22: High when in the kernel, low
	//	otherwise
	DDRA|=1<<PA0;
	//	We start in the kernel, so this starts
	//	high
	PORTA|=1<<PA0;
	
	//	Port 23: High when in user space, low
	//	otherwise
	DDRA|=1<<PA1;
	//	We start in the kernel, so this starts
	//	low
	PORTA&=~(1<<PA1);
	
	//	Port 24: High as the timer ISR enters,
	//	low as it exits
	DDRA|=1<<PA2;
	//	We do not start in an ISR...
	PORTA&=~(1<<PA2);
	
	#endif
	
	return 0;
	
}


static void kthread_set_priority (thread_t thread, priority_t prio) {
	
	//	The idle thread is always thread zero
	if ((thread>MAX_THREADS) || (thread==0) || (threads[thread].state==DEAD)) {
		
		last_error=EINVAL;
		return;
		
	}
	
	threads[thread].priority=prio;
	//	The change of priority may have affected when the thread
	//	should run again
	kthread_enqueue(&threads[thread]);
	
}


static struct kthread * kthread_wait_pop (struct kthread_linked_list * ll) {
	
	struct kthread * retr=ll->head;
	if (!retr) return 0;
	ll->head=retr->wait.next;
	retr->wait.next=0;
	if (!ll->head) ll->tail=0;
	
	//	If we pop a thread off the waiting queue
	//	it's ready to run again, so set its state
	//	and put it in the dispatch queue
	retr->state=READY;
	kthread_enqueue(retr);
	
	return retr;
	
}


static void kthread_wait_insert (struct kthread_linked_list * ll, struct kthread * after, struct kthread * t) {
	
	//	Being inserted into the wait queue means
	//	you're blocked
	t->state=BLOCKED;
	if (t==current_thread) kdispatch();
	
	if (after) {
		
		t->wait.next=after->wait.next;
		after->wait.next=t;
		
	} else {
		
		t->wait.next=ll->head;
		ll->head=t;
		
	}
	
	if (!t->wait.next) ll->tail=t;
	
}


static void kthread_wait_prio_push (struct kthread_linked_list * ll, struct kthread * t) {
	
	if (!ll->head || (ll->head->priority<t->priority)) {
		
		kthread_wait_insert(ll,0,t);
		return;
		
	}
	
	struct kthread * loc;
	for (loc=ll->head;loc->wait.next!=0;loc=loc->wait.next) {
		
		if (loc->wait.next->priority<t->priority) break;
		
	}
	
	kthread_wait_insert(ll,loc,t);
	
}


static void kmutex_create (mutex_t * mutex) {
	
	mutex_t m;
	for (m=0;m<MAX_MUTEXES;++m) if (!mutexes[m].valid) break;
	if (m==MAX_MUTEXES) {
		
		last_error=EAGAIN;
		return;
		
	}
	
	struct kmutex * ptr=&mutexes[m];
	memset(ptr,0,sizeof(*ptr));
	ptr->valid=true;
	*mutex=m;
	
}


//	Returns a pointer to the kernel mutex object
//	if the handle is valid, NULL otherwise
static struct kmutex * kmutex_get (mutex_t mutex) {
	
	if (mutex>=MAX_MUTEXES) {
		
		last_error=EINVAL;
		return 0;
		
	}
	
	struct kmutex * retr=&mutexes[mutex];
	
	if (!retr->valid) {
		
		last_error=EINVAL;
		return 0;
		
	}
	
	return retr;
	
}


static void kmutex_destroy (mutex_t mutex) {
	
	struct kmutex * curr=kmutex_get(mutex);
	if (!curr) return;
	
	//	If the destroyed mutex has threads waiting on it
	//	that's just too bad, undefined behaviour
	curr->valid=false;
	
}


static void kmutex_lock (mutex_t mutex) {
	
	struct kmutex * curr=kmutex_get(mutex);
	if (!curr) return;
	
	//	Unowned, current thread becomes owner
	//	at once
	if (!curr->owner) {
		
		curr->owner=current_thread;
		curr->count=1;
		return;
		
	}
	
	//	Support recursive locking
	if (curr->owner==current_thread) {
		
		if (curr->count==UINT_MAX) {
			
			last_error=EAGAIN;
			return;
			
		}
		
		++curr->count;
		return;
		
	}
	
	//	TODO: Handle priority inversion
	
	kthread_wait_prio_push(&curr->queue,current_thread);
	
}


static void kmutex_unlock (mutex_t mutex) {
	
	struct kmutex * curr=kmutex_get(mutex);
	if (!curr) return;
	
	if (current_thread!=curr->owner) {
		
		last_error=EPERM;
		return;
		
	}
	
	//	Don't actually release the lock until the
	//	recursion count reaches zero
	if ((--curr->count)!=0) return;
	
	curr->owner=kthread_wait_pop(&curr->queue);
	curr->count=1;
	
}


static void kevent_create (event_t * event) {
	
	event_t e;
	for (e=0;e<MAX_EVENTS;++e) if (!events[e].valid) break;
	if (e==MAX_EVENTS) {
		
		last_error=EAGAIN;
		return;
		
	}
	
	struct kevent * ev=&events[e];
	memset(ev,0,sizeof(*ev));
	ev->valid=true;
	*event=e;
	
}


//	Returns a pointer to the kernel event object
//	if the handle is valid, NULL and sets EINVAL
//	otherwise
static struct kevent * kevent_get (event_t event) {
	
	if (event>=MAX_EVENTS) {
		
		last_error=EINVAL;
		return 0;
		
	}
	
	struct kevent * retr=&events[event];
	
	if (!retr) {
		
		last_error=EINVAL;
		return 0;
		
	}
	
	return retr;
	
}


static void kevent_destroy (event_t event) {
	
	struct kevent * e=kevent_get(event);
	if (!e) return;
	
	//	If there's a thread waiting that's undefined
	//	behaviour, too bad
	e->valid=false;
	
}


static void kevent_wait (event_t event) {
	
	struct kevent * e=kevent_get(event);
	if (!e) return;
	
	//	Event already signalled, return at
	//	once
	if (e->set) {
		
		e->set=false;
		return;
		
	}
	
	//	Another thread is already waiting
	if (e->wait) {
		
		last_error=EBUSY;
		return;
		
	}
	
	//	Wait
	e->wait=current_thread;
	current_thread->state=BLOCKED;
	kdispatch();
	
}


static void kevent_signal (event_t event) {
	
	struct kevent * e=kevent_get(event);
	if (!e) return;
	
	//	There's a thread waiting just
	//	release it
	if (e->wait) {
		
		e->wait->state=READY;
		kthread_enqueue(e->wait);
		e->wait=0;
		
		return;
		
	}
	
	e->set=true;
	
}


#define SYSCALL_POP(var,buffer,i) do { memcpy(&(var),((unsigned char *)(buffer))+i,sizeof((var)));i+=sizeof((var)); } while (0)


static int kstart (void) {
	
	//	Get an initial thread
	kdispatch();
	
	for (;;) {
		
		current_thread->state=RUNNING;
		kexit();
		if (current_thread->state==RUNNING) current_thread->state=READY;
		
		if (quantum_expired) {
			
			quantum_expired=false;
			kdispatch();
			continue;
			
		}
		
		last_error=ENONE;
		
		enum syscall num=syscall_state.num;
		void * args=syscall_state.args;
		size_t len=syscall_state.len;
		size_t i=0;
		switch (num) {
			
			case SYSCALL_YIELD:
				kdispatch();
				break;
				
			case SYSCALL_THREAD_CREATE:{
				
				if (len!=(sizeof(thread_t *)+sizeof(void (*) (void *))+sizeof(priority_t)+sizeof(void *))) goto invalid_length;
				
				thread_t * thread;
				SYSCALL_POP(thread,args,i);
				void (*f) (void *);
				SYSCALL_POP(f,args,i);
				priority_t prio;
				SYSCALL_POP(prio,args,i);
				void * arg;
				SYSCALL_POP(arg,args,i);
				
				kthread_create(thread,f,prio,arg);
				
			}break;
			
			case SYSCALL_THREAD_SELF:{
				
				if (len!=sizeof(thread_t *)) goto invalid_length;
				
				thread_t * retr;
				SYSCALL_POP(retr,args,i);
				struct kthread * begin=threads;
				*retr=(thread_t)(current_thread-begin);
				
			}break;
			
			case SYSCALL_THREAD_SET_PRIORITY:{
				
				if (len!=(sizeof(thread_t)+sizeof(priority_t))) goto invalid_length;
				
				thread_t thread;
				SYSCALL_POP(thread,args,i);
				priority_t prio;
				SYSCALL_POP(prio,args,i);
				
				kthread_set_priority(thread,prio);
				
			}break;
			
			case SYSCALL_MUTEX_CREATE:{
				
				if (len!=sizeof(mutex_t *)) goto invalid_length;
				
				mutex_t * mutex;
				SYSCALL_POP(mutex,args,i);
				
				kmutex_create(mutex);
				
			}break;
			
			case SYSCALL_MUTEX_DESTROY:
			case SYSCALL_MUTEX_LOCK:
			case SYSCALL_MUTEX_UNLOCK:{
				
				if (len!=sizeof(mutex_t)) goto invalid_length;
				
				mutex_t mutex;
				SYSCALL_POP(mutex,args,i);
				
				switch (num) {
					
					case SYSCALL_MUTEX_DESTROY:
						kmutex_destroy(mutex);
						break;
					case SYSCALL_MUTEX_LOCK:
						kmutex_lock(mutex);
						break;
					case SYSCALL_MUTEX_UNLOCK:
					default:
						kmutex_unlock(mutex);
						break;
					
				}
				
			}break;
			
			case SYSCALL_EVENT_CREATE:{
				
				if (len!=sizeof(event_t *)) goto invalid_length;
				
				event_t * event;
				SYSCALL_POP(event,args,i);
				
				kevent_create(event);
				
			}break;
			
			case SYSCALL_EVENT_DESTROY:
			case SYSCALL_EVENT_WAIT:{
				
				if (len!=sizeof(event_t)) goto invalid_length;
				
				event_t event;
				SYSCALL_POP(event,args,i);
				
				if (num==SYSCALL_EVENT_DESTROY) kevent_destroy(event);
				else kevent_wait(event);
				
			}break;
			
			case SYSCALL_EVENT_SIGNAL:{
				
				if (len!=(sizeof(event_t)+sizeof(error_t *))) goto invalid_length;
				
				event_t event;
				SYSCALL_POP(event,args,i);
				error_t * err;
				SYSCALL_POP(err,args,i);
				
				kevent_signal(event);
				if (err) *err=last_error;
				
			}break;
			
			default:
				last_error=EINVAL;
				break;
			
		}
		
		continue;
		
		invalid_length:
		last_error=EINVAL;
		
	}
	
	return 0;
	
}


void rtos_main (void);


static void main_thread (void * arg) {
	
	(void)arg;
	rtos_main();
	
}


int main (void) {
	
	kinit();
	
	thread_t m;
	kthread_create(&m,main_thread,MAIN_PRIO,0);
	
	kstart();
	
}


int syscall (enum syscall num, void * args, size_t len) {
	
	unsigned char i=push_interrupt();
	
	syscall_state.num=num;
	syscall_state.args=args;
	syscall_state.len=len;
	kenter();
	
	bool success=last_error==ENONE;
	
	if (num!=SYSCALL_EVENT_SIGNAL) {
		
		current_thread->last_error=last_error;
		current_thread->last_syscall=num;
		
	}
	
	pop_interrupt(i);
	
	return success ? 0 : -1;
	
}


ISR(TIMER1_COMPA_vect,ISR_BLOCK) {
	
	#ifdef DEBUG
	PORTA|=1<<PA2;
	#endif
	
	quantum_expired=true;
	kenter();
	
}
