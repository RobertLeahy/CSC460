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
static struct kthread * thread_queue [MAX_THREADS];
struct kthread * current_thread;
error_t last_error;
void * ksp;


struct kmutex {
	
	bool valid;
	struct kthread * queue [MAX_THREADS];
	
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


static struct kthread * kthread_dequeue (void) {
	
	struct kthread * retr;
	do {
		
		retr=thread_queue[0];
		memmove(thread_queue,&thread_queue[1],sizeof(thread_queue)-sizeof(struct kthread *));
		thread_queue[MAX_THREADS-1U]=0;
		
	} while (!retr || (retr->state!=READY));
	
	return retr;
	
}


static void kthread_enqueue (struct kthread * t) {
	
	if (t->state!=READY) return;
	
	//	We need to determine two or three things:
	//
	//	1.	Where should this task be (i.e. loc)
	//	2.	Has this task already been inserted (i.e. dup)
	//
	//	If 2, we also need to determine if the task is
	//	in the correct location for its current priority
	//	(i.e. is_sorted)
	bool is_sorted=true;
	size_t dup=MAX_THREADS;
	size_t loc=MAX_THREADS;
	for (size_t i=0;i<MAX_THREADS;++i) {
		
		struct kthread * curr=thread_queue[i];
		
		if (curr==t) {
			
			dup=i;
			
		} else {
			
			//	Determine whether the priority of
			//	this task is correct wrt the priority
			//	of the incoming thread
			if (loc==MAX_THREADS) {
				
				//	Priorities should all be larger
				//	than or equal to the incoming thread's
				//	since the incoming thread's position has
				//	not been found yet
				if (curr->priority<t->priority) is_sorted=false;
				
			} else {
				
				//	Priorities should all be smaller than
				//	or equal to the incoming thread's since
				//	the incoming thread's position has been
				//	found (and is therefore ahead of the
				//	current position in the queue)
				if (curr->priority>t->priority) is_sorted=false;
				
			}
			
		}
		
		//	We already found the insertion point,
		//	so we don't need to do anything else
		if (loc!=MAX_THREADS) continue;
		
		//	This position should be ahead of the incoming
		//	thread (assuming we insert the incoming thread
		//	and don't keep its current position)
		if (curr && (curr->start_routine!=&kidle) && (curr->priority>=t->priority)) continue;
		
		loc=i;
		
	}
	
	//	If this thread is already in the queue
	if (dup!=MAX_THREADS) {
		
		//	If its current position is fine for
		//	its current priority, that's great,
		//	we're done
		if (is_sorted) return;
		
		//	Otherwise we have to remove & re-insert
		
		//	The duplicate is infront of the insertion
		//	location, removing it will move the insertion
		//	location ahead one
		if (dup<loc) --loc;
		
		struct kthread ** ptr=thread_queue;
		ptr+=dup;
		memmove(ptr,ptr+1,sizeof(struct kthread *)*(MAX_THREADS-1U-dup));
		thread_queue[MAX_THREADS-1U]=0;
		
	}
	
	//	Insert
	struct kthread ** ptr=thread_queue;
	ptr+=loc;
	memmove(ptr+1U,ptr,sizeof(struct kthread *)*(MAX_THREADS-1U-loc));
	thread_queue[loc]=t;
	
}


static void kdispatch (void) {
	
	current_thread=kthread_dequeue();
	kthread_enqueue(current_thread);
	
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
	memset(thread_queue,0,sizeof(thread_queue));
	current_thread=0;
	
	//	Start idle task
	thread_t ignored;
	kthread_create(&ignored,kidle,IDLE_PRIO,0);
	
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
	
	if (mutex>=MAX_MUTEXES) return 0;
	
	struct kmutex * retr=&mutexes[mutex];
	if (!retr->valid) return 0;
	
	return retr;
	
}


static void kmutex_destroy (mutex_t mutex) {
	
	struct kmutex * curr=kmutex_get(mutex);
	if (!curr) {
		
		errno=EINVAL;
		return;
		
	}
	
	//	If the destroyed mutex has threads waiting on it
	//	that's just too bad, undefined behaviour
	curr->valid=false;
	
}


static void kmutex_lock (mutex_t mutex) {
	
	struct kmutex * curr=kmutex_get(mutex);
	if (!curr) {
		
		errno=EINVAL;
		return;
		
	}
	
	//	Unowned, current thread becomes owner
	//	at once
	if (!curr->queue[0]) {
		
		curr->queue[0]=current_thread;
		return;
		
	}
	
	//	TODO: Make mutex recursive
	if (curr->queue[0]==current_thread) {
		
		errno=EDEADLK;
		return;
		
	}
	
	//	Current thread will have to wait
	current_thread->state=BLOCKED;
	
	//	TODO: Handle priority inversion
	
	//	Higher priority threads get the
	//	lock first
	size_t loc;
	for (loc=1;loc<MAX_THREADS;++loc) {
		
		struct kthread * t=curr->queue[loc];
		if (!t) break;
		if (t->priority<current_thread->priority) break;
		
	}
	
	struct kthread ** ptr=curr->queue;
	ptr+=loc;
	memmove(ptr+1,ptr,sizeof(struct kthread *)*(MAX_THREADS-1U-loc));
	*ptr=current_thread;
	
}


static void kmutex_unlock (mutex_t mutex) {
	
	struct kmutex * curr=kmutex_get(mutex);
	if (!curr) {
		
		errno=EINVAL;
		return;
		
	}
	
	if (current_thread!=curr->queue[0]) {
		
		errno=EPERM;
		return;
		
	}
	
	memmove(curr->queue,&curr->queue[1],sizeof(curr->queue)-sizeof(struct kthread *));
	curr->queue[MAX_THREADS-1U]=0;
	struct kthread * wake=curr->queue[0];
	if (!wake) return;
	wake->state=READY;
	kthread_enqueue(wake);
	
	//	TODO: Reschedule if higher priority
	//	thread was waiting?
	
}


#define SYSCALL_POP(var,buffer,i) do { memcpy(&(var),((unsigned char *)(buffer))+i,sizeof((var)));i+=sizeof((var)); } while (0)


static int kstart (void) {
	
	for (bool dispatch=true;;) {
		
		if (dispatch || (current_thread->state!=READY)) kdispatch();
		dispatch=false;
		kexit();
		
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
				*retr=(size_t)(current_thread-begin);
				
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
	
	syscall(SYSCALL_YIELD,0,0);
	
}
