#include <avr/common.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <debug.h>
#include <interrupt.h>
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
//	-	1 8 bit status register
#define RESTORE_POP (1U)
//	The priority of the idle task
#define IDLE_PRIO (0U)
//	The priority of the main thread
#define MAIN_PRIO (255U)


#ifdef TEST
#if TEST==4
#define RELEASE_TIMER3
#endif
#endif


static struct kthread threads [MAX_THREADS];
struct kthread * current_thread;
static error_t last_error;
static void * __attribute__((used)) ksp;
static bool maintain_sleep;
static bool quantum_expired;
static unsigned long long overflows;
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
static struct kthread_linked_list sleep_queue;


struct kmutex;
struct kmutex_node {
	
	struct kmutex * next;
	struct kmutex * prev;
	
};


struct kmutex {
	
	bool valid;
	struct kthread * owner;
	unsigned int count;
	struct kthread_linked_list queue;
	struct kmutex_node list;
	
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
//	addresses of the registers in question)
#define SREG_A "0x3F"
#define SPH_A "0x3E"
#define SPL_A "0x3D"
#define SAVE_CTX asm volatile (	\
	"in r16," SREG_A "\r\n"	\
	"push r16\r\n"	\
)
#define RESTORE_CTX asm volatile (	\
	"pop r16\r\n"	\
	"out " SREG_A ",r16\r\n"	\
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


//	These functions allows instrumenting calls
//	to/returns from kenter/kexit (since they're
//	not naked)
static void kenter (void) {
	
	debug_user_space_exit();
	
	debug_context_switch_begin();
	
	kenter_impl();
	
	debug_context_switch_end();
	
	debug_user_space_enter();
	
}


//	If I don't do it this way the microcontroller
//	starts freaking out and rebooting and not doing
//	anything, no idea why which is irritating but
//	I don't have enough time to actually care
__attribute__((used))
static void debug_thread_signal (void) {
	
	#if defined(DEBUG) && defined(DEBUG_THREAD_SIGNAL)
	debug_pulse(A,PA2,(current_thread-threads));
	#endif
	
}


static void kexit (void) {
	
	debug_kernel_exit();
	
	debug_context_switch_begin();
	
	kexit_impl();
	
	debug_context_switch_end();
	
	debug_kernel_enter();
	
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


//	Sets up timer for scheduling and
//	sleep
static int ktimer_init (void) {
	
	overflows=0;
	memset(&sleep_queue,0,sizeof(sleep_queue));
	maintain_sleep=false;
	
	#ifdef PREEMPTIVE_SCHEDULING
	//	Timer 1 (16 bit)
	//	Scheduling timer
	
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
	#endif
	
	#ifndef RELEASE_TIMER3
	//	Timer 3
	//	Sleep timer
	
	TCCR3A=0;
	TCCR3B=0;
	
	TCCR3B&=~(1<<CS32);
	TCCR3B|=1<<CS31;
	TCCR3B|=1<<CS30;
	
	TCNT3=0;
	
	//	We want to count timer overflows
	TIMSK3=1<<TOIE3;
	#endif
	
	//	When adjusting the prescalar
	//	for this timer this value must
	//	be adjusted as well
	#define NANOSECONDS_PER_TICK (4000U)
	#define TICKS_PER_SECOND (250000UL)
	
	return 0;
	
}


static bool kthread_lower_priority (const struct kthread * a, const struct kthread * b) {
	
	if (a->priority<b->priority) return true;
	
	if (a->start_routine==&kidle) return true;
	
	return false;
	
}


static bool kthread_higher_priority (const struct kthread * a, const struct kthread * b) {
	
	if (a->priority>b->priority) return true;
	
	if (b->start_routine==&kidle) return true;
	
	return false;
	
}


static bool kthread_equal_priority (const struct kthread * a, const struct kthread * b) {
	
	if (a->priority!=b->priority) return false;
	
	if ((a->start_routine==&kidle) || (b->start_routine==&kidle)) return false;
	
	return true;
	
}


static bool kthread_runnable (const struct kthread * t) {
	
	if (t->state!=READY) return false;
	if (t->suspended) return false;
	
	return true;
	
}


static void kthread_enqueue_impl (struct kthread * t) {
	
	//	Ignore threads that are not ready to
	//	run
	if (!kthread_runnable(t)) return;
	
	//	No threads in the queue add this one as the
	//	only thread
	if (!thread_queue.head) {
		
		thread_queue.head=thread_queue.tail=t;
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
			
			if (after) {
				
				if (kthread_lower_priority(loc,t)) continue;
				
				low_after=false;
				//	No need to continue
				break;
				
			} else if (!kthread_higher_priority(loc,t)) {
				
				high_before=false;
				
			}
			
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
	
	//	We track the current thread because
	//	if it changes the quantum restarts
	struct kthread * prev=current_thread;
	
	kthread_enqueue_impl(t);
	current_thread=thread_queue.head;
	
	//	Nothing changed, no need to restart
	//	quantum
	if (current_thread==prev) return;
	
	#ifdef PREEMPTIVE_SCHEDULING
	//	Restart quantum and clear pending
	//	interrupt (if any)
	TCNT1=0;
	TIFR1=1<<OCF1A;
	#endif
	
}


static void kdispatch (void) {
	
	//	Loop until we find a viable thread to run
	do {
		
		//	Pop
		thread_queue.head=current_thread->queue.next;
		if (!thread_queue.head) thread_queue.tail=0;
		current_thread->queue.next=0;
		
		//	Re-enqueue previously running thread
		kthread_enqueue(current_thread);
		
	} while (!kthread_runnable(current_thread));
	
}


static void kthread_start (void) {
	
	debug_user_space_enter();
	
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
	
	DEBUG_SETUP;
	
	return 0;
	
}


static struct kthread * kthread_get (thread_t t) {
	
	if ((t>MAX_THREADS) || (t==0)) {
		
		last_error=EINVAL;
		return 0;
		
	}
	
	struct kthread * retr=&threads[t];
	
	if (retr->state==DEAD) {
		
		last_error=EINVAL;
		return 0;
		
	}
	
	return retr;
	
}


static void kthread_block (struct kthread * t) {
	
	t->state=BLOCKED;
	if (current_thread==t) kdispatch();
	
}


static void kthread_set_priority (thread_t thread, priority_t prio) {
	
	struct kthread * t=kthread_get(thread);
	if (!t) return;
	
	//	If priority is being inherited we need
	//	to upgrade the effective priority if and
	//	only if the priority being set is higher
	//	than the inherited priority
	if (t->inheriting_priority && (t->priority>=prio)) {
		
		t->original_priority=prio;
		
		return;
		
	}
	
	t->priority=prio;
	//	Setting the priority means that either:
	//
	//	A.	Prority wasn't being inherited, which means
	//		this next line is effectively a noop
	//	B.	Priority was being inherited, but the inherited
	//		priority was less than the priority being set,
	//		so we stop inheriting
	t->inheriting_priority=false;
	//	The change of priority may have affected when the thread
	//	should run again
	if (t!=current_thread) kthread_enqueue(t);
	
}


static priority_t kthread_get_priority_impl (struct kthread * t, bool eff) {
	
	if (eff) return t->priority;
	
	if (t->inheriting_priority) return t->original_priority;
	
	return t->priority;
	
}


static void kthread_get_priority (thread_t thread, priority_t * prio, bool eff) {
	
	if (!prio) {
		
		last_error=EINVAL;
		return;
		
	}
	
	struct kthread * t=kthread_get(thread);
	if (!t) return;
	
	*prio=kthread_get_priority_impl(t,eff);
	
}


static void kthread_suspend (thread_t thread) {
	
	struct kthread * t=kthread_get(thread);
	if (!t) return;
	
	if (t->suspended) {
		
		last_error=EALREADY;
		return;
		
	}
	
	t->suspended=true;
	if (t==current_thread) kdispatch();
	
}


static void kthread_resume (thread_t thread) {
	
	struct kthread * t=kthread_get(thread);
	if (!t) return;
	
	if (!t->suspended) {
		
		last_error=EALREADY;
		return;
		
	}
	
	t->suspended=false;
	kthread_enqueue(t);
	
}


static struct kthread * kthread_wait_pop (struct kthread_linked_list * ll) {
	
	struct kthread * retr=ll->head;
	if (!retr) return 0;
	ll->head=retr->wait.next;
	retr->wait.next=0;
	if (ll->head) ll->head->wait.prev=0;
	else ll->tail=0;
	
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
	kthread_block(t);
	
	if (after) {
		
		t->wait.next=after->wait.next;
		t->wait.prev=after;
		after->wait.next=t;
		
	} else {
		
		t->wait.next=ll->head;
		ll->head=t;
		
	}
	
	if (t->wait.next) t->wait.next->wait.prev=t;
	else ll->tail=t;
	
}


static void kthread_wait_prio_push (struct kthread_linked_list * ll, struct kthread * t) {
	
	if (t->wait.prev) t->wait.prev->wait.next=t->wait.next;
	else ll->head=t->wait.next;
	
	if (t->wait.next) t->wait.next->wait.prev=t->wait.prev;
	else ll->tail=t->wait.prev;
	
	memset(&t->wait,0,sizeof(t->wait));
	
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


static void kthread_own (struct kthread * t, struct kmutex * m) {
	
	if (!t->owner_of.head) {
		
		t->owner_of.head=t->owner_of.tail=m;
		return;
		
	}
	
	t->owner_of.tail->list.next=m;
	m->list.prev=t->owner_of.tail;
	t->owner_of.tail=m;
	
}


static void kthread_unown (struct kthread * t, struct kmutex * m) {
	
	if (m->list.prev) m->list.prev->list.next=m->list.next;
	else t->owner_of.head=m->list.next;
	
	if (m->list.next) m->list.next->list.prev=m->list.prev;
	else t->owner_of.tail=m->list.prev;
	
	memset(&m->list,0,sizeof(m->list));
	
}


static void kmutex_lock (mutex_t mutex) {
	
	struct kmutex * curr=kmutex_get(mutex);
	if (!curr) return;
	
	//	Unowned, current thread becomes owner
	//	at once
	if (!curr->owner) {
		
		curr->owner=current_thread;
		curr->count=1;
		kthread_own(current_thread,curr);
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
	
	//	current_thread will change as we enter the
	//	wait queue as this thread will block, so
	//	we save it
	struct kthread * waiting=current_thread;
	//	We're waiting now
	current_thread->waiting_on=curr;
	kthread_wait_prio_push(&curr->queue,current_thread);
	
	//	Handle priority inversion by boosting priorities
	//	as necessary
	priority_t prio=kthread_get_priority_impl(waiting,true);
	struct kthread * boost=curr->owner;
	//	If we loop MAX_MUTEXES time there's a deadlock
	//	somewhere just give up
	for (size_t iters=0;iters<MAX_MUTEXES;++iters) {
		
		priority_t bprio=kthread_get_priority_impl(boost,true);
		//	No need to boost priority
		if (bprio>=prio) break;
		
		priority_t prev=boost->priority;
		boost->priority=prio;
		if (!boost->inheriting_priority) {
			
			boost->inheriting_priority=true;
			boost->original_priority=prev;
			
		}
		
		//	If the thread we just visited is in
		//	turn waiting on a mutex we continue to
		//	that mutex's owner such that our priority
		//	inheritance scheme is transitive
		if (boost->waiting_on) {
			
			//	Since the priority has been boosted, and
			//	since this thread is waiting on a mutex,
			//	it might be necessary to move it ahead in
			//	that mutex's waiting queue
			kthread_wait_prio_push(&boost->waiting_on->queue,boost);
			
			boost=boost->waiting_on->owner;
			continue;
			
		}
		
		//	The thread we visited isn't waiting on
		//	anything, make sure it's scheduled at
		//	its new priority and end
		kthread_enqueue(boost);
		break;
		
	}
	
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
	
	kthread_unown(current_thread,curr);
	
	//	Undo priority inheritance if necessary
	if (current_thread->inheriting_priority) {
		
		current_thread->priority=current_thread->original_priority;
		current_thread->inheriting_priority=false;
		
		//	See if there's another thread waiting on
		//	any of the mutexes that we own we necessitates
		//	priority inheritance
		priority_t max=0;
		for (struct kmutex * m=current_thread->owner_of.head;m;m=m->list.next) {
			
			//	No other threads waiting
			if (!m->queue.head) continue;
			
			priority_t consider=m->queue.head->priority;
			if (consider>max) max=consider;
			
		}
		
		if (max>current_thread->priority) {
			
			current_thread->inheriting_priority=true;
			current_thread->original_priority=current_thread->priority;
			current_thread->priority=max;
			
		}
		
		//	Re-enqueue thread so that it's position
		//	reflects its newly reduced priority
		kthread_enqueue(current_thread);
		
	}
	
	//	Transfer ownership to the new owner if there
	//	is one (i.e. prevent barging)
	curr->owner=kthread_wait_pop(&curr->queue);
	if (curr->owner) {
		
		kthread_own(curr->owner,curr);
		curr->owner->waiting_on=0;
		curr->count=1;
		
	}
	
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
	kthread_block(current_thread);
	
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


static struct ktime know (void) {
	
	//	Grab the state so it's as fresh as
	//	possible for detecting/compensating
	//	for an overflow
	struct ktime retr;
	retr.remainder=TCNT3;
	unsigned char i=TIFR3;
	retr.overflows=overflows;
	
	//	Now we check for a pending interrupt
	//	(this means that the global overflows
	//	variable is one lower than it should be)
	if ((i&(1<<TOV3))!=0) {
		
		++retr.overflows;
		
		//	There's a possibility that the following
		//	has happened:
		//
		//	1.	We sample TCNT3 and it's about to
		//		overflow (e.g. is 65535)
		//	2.	The timer ticks, overflowing and
		//		setting TOV3 in TIFR3
		//	3.	We read TIFR3
		//	4.	Based on TIFR3 we increment the number
		//		of overflows
		//
		//	In this case we'll have an extremely
		//	high remainder and an overflow one
		//	higher than it should be: I.e. a time
		//	that's not "now" but is ~65000 ticks
		//	in the future
		//
		//	We detect this situation by resampling
		//	TCNT3 and seeing if it's decreased, if
		//	it has we accept the new sample
		unsigned int resample=TCNT3;
		if (resample<retr.remainder) retr.remainder=resample;
		
	}
	
	return retr;
	
}


static struct ktime ktime_add (struct ktime a, struct ktime b) {
	
	if ((65535U-b.remainder)<a.remainder) ++a.overflows;
	a.remainder+=b.remainder;
	a.overflows+=b.overflows;
	
	return a;
	
}


//	Returns:
//
//	-	Negative integer if a is less than b
//	-	Zero if a and b are equal
//	-	Positive integer if a is greater than b
static int ktime_cmp (struct ktime a, struct ktime b) {
	
	if (a.overflows<b.overflows) return -1;
	if (a.overflows>b.overflows) return 1;
	
	if (a.remainder<b.remainder) return -1;
	if (a.remainder>b.remainder) return 1;
	
	return 0;
	
}


static struct ktime kto_time (struct timespec ts) {
	
	//	We sample "now" as early as possible
	//	to make sure our calculation is as accurate
	//	relative to when this function was called
	//	as possible
	struct ktime retr=know();
	
	//	Calculate nanoseconds contribution
	struct ktime ns;
	unsigned long ns_ticks=ts.tv_nsec/NANOSECONDS_PER_TICK;
	ns.remainder=ns_ticks%65536UL;
	ns.overflows=ns_ticks/65536UL;
	
	//	Calculate seconds contribution
	struct ktime s;
	unsigned long long os_per_s=TICKS_PER_SECOND/65536ULL;
	//	Note choice of higher precision to prevent overflow
	unsigned long long ticks_per_s=TICKS_PER_SECOND%65536ULL;
	s.overflows=ts.tv_sec*os_per_s;
	ticks_per_s*=ts.tv_sec;
	s.overflows+=ticks_per_s/65536UL;
	s.remainder=ticks_per_s%65536UL;
	
	return ktime_add(retr,ktime_add(s,ns));
	
}


#ifndef RELEASE_TIMER3
#define ksleep_overflow_pending() ((TIFR3&(1<<TOV3))!=0)
#define ksleep_tick_pending() ((TIFR3&(1<<OCF3A))!=0)
#define ksleep_enable_tick(cnt) do { OCR3A=(cnt);TIMSK3|=1<<OCIE3A; } while (0)
#define ksleep_disable_tick() do { TIMSK3&=~(1<<OCIE3A);TIFR3=1<<OCF3A; } while (0)
#define ksleep_no_work() (!sleep_queue.head || (sleep_queue.head->sleep.overflows>overflows))


//	Maintains sleep-related data structures et cetera
static void kmaintain_sleep_impl (void) {
	
	for (;;) {
		
		//	If we need an interrupt we'll set it up
		//	later
		ksleep_disable_tick();
		
		//	If there are no sleeping threads,
		//	or the first sleeping thread doesn't wake
		//	up this overflow cycle, there's nothing to do
		if (ksleep_no_work()) return;
		
		//	Sample current number of ticks
		unsigned int ticks=TCNT3;
		//	If an overflow interrupt is pending just
		//	wait for that
		if (ksleep_overflow_pending()) return;
		
		//	Now we have a number of ticks, and we know
		//	that number of ticks wasn't affected by
		//	overflow, let's use it to pop threads
		struct ktime now;
		now.overflows=overflows;
		now.remainder=ticks;
		while (sleep_queue.head && (ktime_cmp(now,sleep_queue.head->sleep)>=0)) {
			
			kthread_enqueue(kthread_wait_pop(&sleep_queue));
			
		}
		
		//	If we emptied the queue there's nothing left
		//	to do exit at once
		if (ksleep_no_work()) return;
		
		//	Now we know there's at least one sleeping thread,
		//	AND that it wakes up this overflow cycle
		
		unsigned int next=sleep_queue.head->sleep.remainder;
		ticks=TCNT3;
		//	Overflow interrupt pending, deal with it then
		if (ksleep_overflow_pending()) return;
		//	The time has already come, loop and dequeue
		if (ticks>=next) continue;
		
		//	Setup interrupt
		ksleep_enable_tick(next);
		ticks=TCNT3;
		//	Did an overflow interrupt become pending while
		//	we were setting up the interrupt?  If so just
		//	handle this there
		if (ksleep_overflow_pending()) {
			
			ksleep_disable_tick();
			return;
			
		}
		//	Did the time come while we were setting up the
		//	interrupt?  If so handle it here
		if (ticks>=next) continue;
		
		//	We're done!
		return;
		
	}
	
}
#endif


static void kmaintain_sleep (void) {
	
	debug_maintain_sleep_enter();
	
	#ifndef RELEASE_TIMER3
	kmaintain_sleep_impl();
	#endif
	
	debug_maintain_sleep_exit();
	
}


static void ksleep (struct timespec ts) {
	
	current_thread->sleep=kto_time(ts);
	
	//	Head of the sleep queue
	if (!sleep_queue.head || (ktime_cmp(current_thread->sleep,sleep_queue.head->sleep)<0)) {
		
		kthread_wait_insert(&sleep_queue,0,current_thread);
		
		//	We're the new head of the sleep queue so
		//	everything has to be setup again to be right
		//	for us
		kmaintain_sleep();
		
		return;
		
	}
	
	//	Find position in sleep queue
	struct kthread * loc;
	for (loc=sleep_queue.head;loc->wait.next!=0;loc=loc->wait.next) {
		
		if (ktime_cmp(current_thread->sleep,loc->wait.next->sleep)<0) break;
		
	}
	
	kthread_wait_insert(&sleep_queue,loc,current_thread);
	
	//	We don't have to do anything extra here because
	//	the kernel is waiting on the first element of
	//	the sleep queue.  Since the first element of the
	//	sleep queue isn't us we're good: Nothing changes
	
}


#define SYSCALL_POP(var,buffer,i) do { memcpy(&(var),((unsigned char *)(buffer))+i,sizeof((var)));i+=sizeof((var)); } while (0)


static int kstart (void) {
	
	//	Get an initial thread
	kdispatch();
	
	for (;;) {
		
		current_thread->state=RUNNING;
		kexit();
		if (current_thread->state==RUNNING) current_thread->state=READY;
		
		if (maintain_sleep) {
			
			maintain_sleep=false;
			kmaintain_sleep();
			continue;
			
		}
		
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
		debug_syscall();
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
			
			case SYSCALL_THREAD_SUSPEND:
			case SYSCALL_THREAD_RESUME:{
				
				if (len!=sizeof(thread_t)) goto invalid_length;
				
				thread_t thread;
				SYSCALL_POP(thread,args,i);
				
				if (num==SYSCALL_THREAD_SUSPEND) kthread_suspend(thread);
				else kthread_resume(thread);
				
			}break;
			
			case SYSCALL_THREAD_GET_PRIORITY:{
				
				if (len!=(sizeof(thread_t)+sizeof(priority_t *)+sizeof(bool))) goto invalid_length;
				
				thread_t thread;
				SYSCALL_POP(thread,args,i);
				priority_t * prio;
				SYSCALL_POP(prio,args,i);
				bool eff;
				SYSCALL_POP(eff,args,i);
				
				kthread_get_priority(thread,prio,eff);
				
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
			
			case SYSCALL_SLEEP:{
				
				if (len!=sizeof(struct timespec)) goto invalid_length;
				
				struct timespec ts;
				SYSCALL_POP(ts,args,i);
				
				ksleep(ts);
				
			}break;
			
			case SYSCALL_THREAD_TERMINATE:{
				
				if (len!=0) goto invalid_length;
				
				current_thread->state=DEAD;
				kdispatch();
				
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


void a_main (void);


static void main_thread (void * arg) {
	
	(void)arg;
	a_main();
	
}


int main (void) {
	
	kinit();
	
	debug_start();
	
	thread_t m;
	kthread_create(&m,main_thread,MAIN_PRIO,0);
	
	kstart();
	
}


int syscall (enum syscall num, void * args, size_t len) {
	
	bool i=disable_and_push_interrupt();
	
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


#ifndef PREEMPTIVE_SCHEDULING
ISR(TIMER1_COMPA_vect,ISR_BLOCK) {
	
	debug_quantum();
	
	quantum_expired=true;
	kenter();
	
}
#endif


#ifndef RELEASE_TIMER3
ISR(TIMER3_OVF_vect,ISR_BLOCK) {
	
	debug_sleep_overflow();
	
	++overflows;
	
	//	Check this outside the kernel to
	//	save context switch
	if (ksleep_no_work()) return;
	
	maintain_sleep=true;
	kenter();
	
}


ISR(TIMER3_COMPA_vect,ISR_BLOCK) {
	
	debug_sleep_timer();
	
	maintain_sleep=true;
	kenter();
	
}
#endif
