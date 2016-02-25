#include <avr/interrupt.h>
#include <avr/io.h>
#include <kernel.h>
#include <os.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>


#define MAX_THREADS (4U)
#define MAX_MUTEXES (8U)
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
error_t last_error;
void * ksp;
static bool quantum_expired;


struct kthread_linked_list {
	
	struct kthread * head;
	struct kthread * tail;
	
};


static struct kthread_linked_list thread_queue;


struct kmutex {
	
	bool valid;
	struct kthread_linked_list queue;
	
};
static struct kmutex mutexes [MAX_MUTEXES];


//	These are provided in assembly
void kenter (void);
void kexit (void);


static void kidle (void * arg) {
	
	//	Unused: Always NULL
	(void)arg;
	
	//	Put the CPU to sleep
	for (;;) asm volatile ("sleep"::);
	
}


//	Initializes global errno to ENONE
static int klast_error_init (void) {
	
	last_error=ENONE;
	
	return 0;
	
}


//	Initializes global collection of mutexes
static int kmutex_init (void) {
	
	memset(mutexes,0,sizeof(mutexes));
	
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
	//	run (we can ignore running threads because
	//	they will be re-enqueued at the end of their
	//	time slice
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
	
	//	If the first thread is running,
	//	then we can't move it, so just remove/restore
	struct kthread * restore=0;
	if (thread_queue.head && (thread_queue.head->state==RUNNING)) {
		
		restore=thread_queue.head;
		thread_queue.head=restore->queue.next;
		if (!thread_queue.head) thread_queue.tail=0;
		restore->queue.next=0;
		
	}
	
	kthread_enqueue_impl(t);
	
	if (restore) {
		
		restore->queue.next=thread_queue.head;
		if (!thread_queue.head) thread_queue.tail=restore;
		thread_queue.head=restore;
		
	}
	
}


static void kdispatch (void) {
	
	if (current_thread && (current_thread->state==RUNNING)) current_thread->state=READY;
	
	//	Loop until we find a viable thread to run
	do {
		
		//	Pop
		struct kthread * curr=thread_queue.head;
		thread_queue.head=curr->queue.next;
		if (!thread_queue.head) thread_queue.tail=0;
		curr->queue.next=0;
		
		//	Re-enqueue previously running thread
		kthread_enqueue(curr);
		
		//	Select front of queue as the current
		//	thread (this decision will be evaluated
		//	below)
		current_thread=thread_queue.head;
		
	} while (current_thread->state!=READY);
	
	current_thread->state=RUNNING;
	
}


static void kthread_start (void) {
	
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
		
		errno=EAGAIN;
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
	errno=ENONE;
	
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
		(kmutex_init()!=0)
	) return -1;
	
	return 0;
	
}


static void kthread_set_priority (thread_t thread, priority_t prio) {
	
	//	The idle thread is always thread zero
	if ((thread>MAX_THREADS) || (thread==0) || (threads[thread].state==DEAD)) {
		
		errno=EINVAL;
		return;
		
	}
	
	threads[thread].priority=prio;
	//	The change of priority may have affected when the thread
	//	should run again
	kthread_enqueue(&threads[thread]);
	
}


static struct kthread * kthread_wait_pop (struct kthread_linked_list * ll) {
	
	struct kthread * retr=ll->head->wait.next;
	ll->head->wait.next=0;
	ll->head=retr;
	if (!retr) {
		
		ll->tail=0;
		
		return 0;
		
	}
	
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
	
	t->wait.next=after->wait.next;
	after->wait.next=t;
	if (!t->wait.next) ll->tail=t;
	
}


static void kthread_wait_prio_push (struct kthread_linked_list * ll, struct kthread * t) {
	
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
		
		errno=EAGAIN;
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
		
		errno=EINVAL;
		return 0;
		
	}
	
	struct kmutex * retr=&mutexes[mutex];
	
	if (!retr->valid) {
		
		errno=EINVAL;
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
	if (!curr->queue.head) {
		
		curr->queue.tail=curr->queue.head=current_thread;
		return;
		
	}
	
	//	TODO: Make mutex recursive
	if (curr->queue.head==current_thread) {
		
		errno=EDEADLK;
		return;
		
	}
	
	//	TODO: Handle priority inversion
	
	kthread_wait_prio_push(&curr->queue,current_thread);
	
}


static void kmutex_unlock (mutex_t mutex) {
	
	struct kmutex * curr=kmutex_get(mutex);
	if (!curr) return;
	
	if (current_thread!=curr->queue.head) {
		
		errno=EPERM;
		return;
		
	}
	
	kthread_wait_pop(&curr->queue);
	
	//	TODO: Reschedule if higher priority
	//	thread was waiting?
	
}


#define SYSCALL_POP(var,buffer,i) do { memcpy(&(var),((unsigned char *)(buffer))+i,sizeof((var)));i+=sizeof((var)); } while (0)


static int kstart (void) {
	
	for (bool dispatch=true;;) {
		
		if (dispatch || (current_thread->state!=READY)) kdispatch();
		dispatch=false;
		kexit();
		
		if (quantum_expired) {
			
			quantum_expired=false;
			dispatch=true;
			continue;
			
		}
		
		current_thread->last_error=ENONE;
		
		struct ksyscall_state * state=&current_thread->syscall;
		enum syscall num=state->num;
		void * args=state->args;
		size_t len=state->len;
		switch (num) {
			
			case SYSCALL_YIELD:
				dispatch=true;
				break;
				
			case SYSCALL_THREAD_CREATE:{
				
				if (len!=(sizeof(thread_t *)+sizeof(void (*) (void *))+sizeof(priority_t)+sizeof(void *))) goto invalid_length;
				
				size_t i=0;
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
				
				size_t i=0;
				thread_t * retr;
				SYSCALL_POP(retr,args,i);
				struct kthread * begin=threads;
				*retr=(thread_t)(current_thread-begin);
				
			}break;
			
			case SYSCALL_THREAD_SET_PRIORITY:{
				
				if (len!=(sizeof(thread_t)+sizeof(priority_t))) goto invalid_length;
				
				size_t i=0;
				thread_t thread;
				SYSCALL_POP(thread,args,i);
				priority_t prio;
				SYSCALL_POP(prio,args,i);
				
				kthread_set_priority(thread,prio);
				
			}break;
			
			case SYSCALL_MUTEX_CREATE:{
				
				if (len!=sizeof(mutex_t *)) goto invalid_length;
				
				size_t i=0;
				mutex_t * mutex;
				SYSCALL_POP(mutex,args,i);
				
				kmutex_create(mutex);
				
			}break;
			
			case SYSCALL_MUTEX_DESTROY:
			case SYSCALL_MUTEX_LOCK:
			case SYSCALL_MUTEX_UNLOCK:{
				
				if (len!=sizeof(mutex_t)) goto invalid_length;
				
				size_t i=0;
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
			
			default:
				current_thread->last_error=EINVAL;
				break;
			
		}
		
		continue;
		
		invalid_length:
		current_thread->last_error=EINVAL;
		
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
	
	struct ksyscall_state * s=&current_thread->syscall;
	s->num=num;
	s->args=args;
	s->len=len;
	kenter();
	
	return (errno==ENONE) ? 0 : -1;
	
}


ISR(TIMER1_COMPA_vect,ISR_BLOCK) {
	
	quantum_expired=true;
	kenter();
	
}
