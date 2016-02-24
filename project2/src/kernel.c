#include <avr/interrupt.h>
#include <avr/io.h>
#include <kernel.h>
#include <os.h>
#include <stddef.h>
#include <string.h>


#define MAX_THREADS (4U)
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


//	Disables/enables interrupts
#define	disable_interrupt() asm volatile ("cli"::)
#define enable_interrupt() asm volatile ("sei"::)


static struct kthread threads [MAX_THREADS];
static struct kthread * thread_queue [MAX_THREADS];
struct kthread * current_thread;
error_t last_error;
void * ksp;


static struct {
	enum syscall num;
	void * args;
	size_t len;
} syscall_state;


//	These are provided in assembly
void kenter (void);
void kexit (void);


static void kidle (void * arg) {
	
	//	Unused: Always NULL
	(void)arg;
	
	//	Continually yield
	for (;;) yield();
	
}


//	Initializes global errno to ENONE
static int klast_error_init (void) {
	
	last_error=ENONE;
	
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


static void kthread_start (void) {
	
	//	Interrupts are enabled in non-kernel
	//	threads
	enable_interrupt();
	
	//	Run the thread
	current_thread->start_routine(current_thread->arg);
	
	//	Terminate the thread
	disable_interrupt();
	current_thread->state=DEAD;
	syscall_state.num=SYSCALL_YIELD;
	kenter();
	
}


static void kthread_create (thread_t * thread, void (*f) (void *), priority_t prio, void * arg) {
	
	//	Find an available thread control block
	thread_t avail;
	for (avail=0;avail<MAX_THREADS;++avail) if (threads[avail].state==DEAD) break;
	
	//	No available thread control block: There
	//	are MAX_THREADS threads running
	if (avail==MAX_THREADS) {
		
		if (current_thread) current_thread->last_error=EAGAIN;
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
	size_t i;
	for (i=0;i<MAX_THREADS;++i) if (thread_queue[i]==0) {
		
		thread_queue[i]=t;
		break;
		
	}
	
	//	Return values to caller
	*thread=avail;
	if (current_thread) current_thread->last_error=ENONE;
	
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


static void kdispatch (void) {
	
	//	This implements simple round robin scheduling:
	//	The next non-NULL entry in the queue becomes the
	//	current task and is put back on the end
	
	struct kthread * next;
	do {
		
		next=thread_queue[0];
		memmove(thread_queue,&thread_queue[1],sizeof(thread_queue)-sizeof(struct kthread *));
		thread_queue[MAX_THREADS-1U]=next;
		
	} while (!next || (next->state==DEAD));
	
	current_thread=next;
	
}


static int kinit (void) {
	
	//	Interrupts disabled while in kernel
	disable_interrupt();
	
	if (
		(kthread_init()!=0) ||
		(klast_error_init()!=0) ||
		(ktimer_init()!=0)
	) return -1;
	
	return 0;
	
}


#define SYSCALL_POP(var,buffer,i) do { memcpy(&(var),((unsigned char *)(buffer))+i,sizeof((var)));i+=sizeof((var)); } while (0)


static int kstart (void) {
	
	for (;;) {
		
		kdispatch();
		kexit();
		
		switch (syscall_state.num) {
			
			case SYSCALL_YIELD:
				break;
				
			case SYSCALL_THREAD_CREATE:{
				
				if (syscall_state.len!=(sizeof(thread_t *)+sizeof(void (*) (void *))+sizeof(priority_t)+sizeof(void *))) {
					
					current_thread->last_error=EINVAL;
					break;
					
				}
				
				size_t i=0;
				thread_t * thread;
				SYSCALL_POP(thread,syscall_state.args,i);
				void (*f) (void *);
				SYSCALL_POP(f,syscall_state.args,i);
				priority_t prio;
				SYSCALL_POP(prio,syscall_state.args,i);
				void * arg;
				SYSCALL_POP(arg,syscall_state.args,i);
				
				kthread_create(thread,f,prio,arg);
				
			}break;
			
			default:
				current_thread->last_error=EINVAL;
				break;
			
		}
		
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
	kthread_create(&m,main_thread,IDLE_PRIO+1U,0);
	
	kstart();
	
}


int syscall (enum syscall num, void * args, size_t len) {
	
	syscall_state.args=args;
	syscall_state.num=num;
	syscall_state.len=len;
	kenter();
	
	return (errno==ENONE) ? 0 : -1;
	
}


ISR(TIMER1_COMPA_vect,ISR_BLOCK) {
	
	syscall_state.num=SYSCALL_YIELD;
	kenter();
	
}
