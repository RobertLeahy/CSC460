#include <kernel.h>
#include <os.h>
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


//	These are provided in assembly
void kswitch (void);
void kexit (void);


static void kidle (void * arg) {
	
	//	Unused: Always NULL
	(void)arg;
	
	//	Continually yield
	for (;;) kyield();
	
}


//	Initializes threading
static int kthread_init (void) {
	
	memset(threads,0,sizeof(threads));
	memset(thread_queue,0,sizeof(thread_queue));
	current_thread=0;
	
	//	Start idle task
	thread_t ignored;
	if (kthread_create(&ignored,kidle,IDLE_PRIO,0)!=0) return -1;
	
	return 0;
	
}


//	Initializes global errno to ENONE
static int klast_error_init (void) {
	
	last_error=ENONE;
	
	return 0;
	
}


int kinit (void) {
	
	if (
		(kthread_init()!=0) ||
		(klast_error_init()!=0)
	) return -1;
	
	return 0;
	
}


int kstart (void) {
	
	asm ("jmp kexit"::);
	
	return 0;
	
}


static void kthread_start (void) {
	
	current_thread->start_routine(current_thread->arg);
	
	//	TODO: Terminate
	
}


int kthread_create (thread_t * thread, void (*f) (void *), priority_t prio, void * arg) {
	
	//	TODO: Use this
	(void)prio;
	
	//	Find an available thread control block
	thread_t avail;
	for (avail=0;avail<MAX_THREADS;++avail) if (threads[avail].state==DEAD) break;
	
	//	No available thread control block: There
	//	are MAX_THREADS threads running
	if (avail==MAX_THREADS) {
		
		errno=EAGAIN;
		return -1;
		
	}
	
	//	Initialize thread control block
	struct kthread * t=&threads[avail];
	memset(t,0,sizeof(*t));
	t->state=READY;
	t->start_routine=f;
	t->arg=arg;
	
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
	unsigned int i;
	for (i=0;i<MAX_THREADS;++i) if (thread_queue[i]==0) {
		
		thread_queue[i]=t;
		break;
		
	}
	
	//	Return values to caller
	*thread=avail;
	errno=ENONE;
	return 0;
	
}


void kyield (void) {
	
	kswitch();
	
}


void kdispatch (void) {
	
	//	This implements simple round robin scheduling:
	//	The next non-NULL entry in the queue becomes the
	//	current task and is put back on the end
	
	struct kthread * next;
	do {
		
		next=thread_queue[0];
		memmove(thread_queue,&thread_queue[1],sizeof(thread_queue)-sizeof(struct kthread *));
		thread_queue[MAX_THREADS-1U]=next;
		
	} while (!next);
	
	current_thread=next;
	
}
