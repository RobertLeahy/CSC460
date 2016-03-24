#include <avr/interrupt.h>
#include <avr/io.h>
#include <os.h>


#ifndef TEST

#error TEST must be defined to the desired test number

#elif TEST==1

//
// LAB - TEST 1
//	Noah Spriggs, Murray Dunne
//
//
// EXPECTED RUNNING ORDER: P1,P2,P3,P1,P2,P3,P1
//
// P1 sleep              lock(attept)            lock
// P2      sleep                     signal
// P3           lock wait                  unlock

MUTEX mut;
EVENT evt;

void Task_P1(void)
{
	Task_Sleep(10); // sleep 100ms
	Mutex_Lock(mut);
    for(;;);
}

void Task_P2(void)
{
	Task_Sleep(20); // sleep 200ms
	Event_Signal(evt);
    for(;;);
}

void Task_P3(void)
{
	Mutex_Lock(mut);
	Event_Wait(evt);
	Mutex_Unlock(mut);
    for(;;);
}

void a_main(void)
{
	/*
	//Place these as necessary to display output if not already doing so inside the RTOS
	//initialize pins
	DDRB |= (1<<PB1);	//pin 52
	DDRB |= (1<<PB2);	//pin 51	
	DDRB |= (1<<PB3);	//pin 50
	
	
	PORTB |= (1<<PB1);	//pin 52 on
	PORTB |= (1<<PB2);	//pin 51 on
	PORTB |= (1<<PB3);	//pin 50 on


	PORTB &= ~(1<<PB1);	//pin 52 off
	PORTB &= ~(1<<PB2);	//pin 51 off
	PORTB &= ~(1<<PB3);	//pin 50 off

	*/
	mut = Mutex_Init();
	evt = Event_Init();

	Task_Create(Task_P1, 1, 0);
	Task_Create(Task_P2, 2, 0);
	Task_Create(Task_P3, 3, 0);
}

#elif TEST==2

//
// LAB - TEST 2
//	Noah Spriggs, Murray Dunne, Daniel McIlvaney

// EXPECTED RUNNING ORDER: P1,P2,P3,P1,P2,P3,P1
//
// P1 sleep              lock(attept)            lock
// P2      sleep                     resume
// P3           lock suspend               unlock

MUTEX mut;
EVENT evt;
volatile PID pid;

void Task_P1(void)
{
	Task_Sleep(10); // sleep 100ms
	Mutex_Lock(mut);
    for(;;);
}

void Task_P2(void)
{
	Task_Sleep(20); // sleep 200ms
	Task_Resume(pid);
    for(;;);
}

void Task_P3(void)
{
	Mutex_Lock(mut);
	Task_Suspend(pid);
	Mutex_Unlock(mut);
    for(;;);
}

void a_main(void)
{
	/*
	//Place these as necessary to display output if not already doing so inside the RTOS
	//initialize pins
	DDRB |= (1<<PB1);	//pin 52
	DDRB |= (1<<PB2);	//pin 51	
	DDRB |= (1<<PB3);	//pin 50
	
	
	PORTB |= (1<<PB1);	//pin 52 on
	PORTB |= (1<<PB2);	//pin 51 on
	PORTB |= (1<<PB3);	//pin 50 on


	PORTB &= ~(1<<PB1);	//pin 52 off
	PORTB &= ~(1<<PB2);	//pin 51 off
	PORTB &= ~(1<<PB3);	//pin 50 off

	*/
	mut = Mutex_Init();
	evt = Event_Init();

	Task_Create(Task_P1, 1, 0);
	Task_Create(Task_P2, 2, 0);
	pid = Task_Create(Task_P3, 3, 0);
}

#elif TEST==3

//
// LAB - TEST 3
//	Noah Spriggs, Murray Dunne, Daniel McIlvaney
//
// EXPECTED RUNNING ORDER: P1,P2,P3,P2,P1,P3,P2,P1
//
// P1 sleep                     lock 1                               unlock 1
// P2 lock 1  sleep    lock 2                    unlock 2 unlock 1
// P3 sleep   lock 2   sleep           unlock 2

MUTEX mut1;
MUTEX mut2;

void Task_P1(void)
{
	Task_Sleep(30);
	Mutex_Lock(mut1);
    Mutex_Unlock(mut1);
    for(;;);
}

void Task_P2(void)
{
    Mutex_Lock(mut1);
    Task_Sleep(20);
    Mutex_Lock(mut2);
    Task_Sleep(10);
    Mutex_Unlock(mut2);
	Mutex_Unlock(mut1);
    for(;;);
}

void Task_P3(void)
{
	Task_Sleep(10);
    Mutex_Lock(mut2);
    Task_Sleep(20);
    Mutex_Unlock(mut2);
    for(;;);
}

void a_main(void)
{
	/*
	//Place these as necessary to display output if not already doing so inside the RTOS
	//initialize pins
	DDRB |= (1<<PB1);	//pin 52
	DDRB |= (1<<PB2);	//pin 51	
	DDRB |= (1<<PB3);	//pin 50
	
	
	PORTB |= (1<<PB1);	//pin 52 on
	PORTB |= (1<<PB2);	//pin 51 on
	PORTB |= (1<<PB3);	//pin 50 on


	PORTB &= ~(1<<PB1);	//pin 52 off
	PORTB &= ~(1<<PB2);	//pin 51 off
	PORTB &= ~(1<<PB3);	//pin 50 off

	*/
	mut1 = Mutex_Init();
    mut2 = Mutex_Init();

	Task_Create(Task_P1, 1, 0);
	Task_Create(Task_P2, 2, 0);
	Task_Create(Task_P3, 3, 0);
}

#elif TEST==4

//
// LAB - TEST 4
//	Noah Spriggs, Murray Dunne, Daniel McIlvaney
//
// EXPECTED RUNNING ORDER: P0,P1,P2,P3,TIMER,P0,P1,P2
//
// P0     wait 3            signal 2, signal 1
// P1     wait 1
// P2     wait 2
// P3            timer->signal 3

MUTEX mut;
EVENT evt1;
EVENT evt2;
EVENT evt3;

void Task_P0(void)
{
	for(;;)
	{
		Event_Wait(evt3);
		Event_Signal(evt2);
		Event_Signal(evt1);		
	}
}

void Task_P1(void)
{
	Event_Wait(evt1);
    Task_Terminate();
    for(;;);
}

void Task_P2(void)
{
	Event_Wait(evt2);
    Task_Terminate();
    for(;;);
}

void Task_P3(void)
{
    for(;;);
}

void configure_timer()
{
    //Clear timer config.
    TCCR3A = 0;
    TCCR3B = 0;  
    //Set to CTC (mode 4)
    TCCR3B |= (1<<WGM32);

    //Set prescaller to 256
    TCCR3B |= (1<<CS32);

    //Set TOP value (0.1 seconds)
    OCR3A = 6250;

    //Set timer to 0 (optional here).
    TCNT3 = 0;
    
    //Enable interupt A for timer 3.
    TIMSK3 |= (1<<OCIE3A);
}

void timer_handler(void)
{
    Event_Signal(evt3);
}

ISR(TIMER3_COMPA_vect)
{
    TIMSK3 &= ~(1<<OCIE3A);
    timer_handler();
}

void a_main(void)
{
	/*
	//Place these as necessary to display output if not already doing so inside the RTOS
	//initialize pins
	DDRB |= (1<<PB1);	//pin 52
	DDRB |= (1<<PB2);	//pin 51	
	DDRB |= (1<<PB3);	//pin 50
	
	
	PORTB |= (1<<PB1);	//pin 52 on
	PORTB |= (1<<PB2);	//pin 51 on
	PORTB |= (1<<PB3);	//pin 50 on


	PORTB &= ~(1<<PB1);	//pin 52 off
	PORTB &= ~(1<<PB2);	//pin 51 off
	PORTB &= ~(1<<PB3);	//pin 50 off

	*/
	evt1 = Event_Init();
    evt2 = Event_Init();
    evt3 = Event_Init();

	Task_Create(Task_P1, 1, 0);
	Task_Create(Task_P2, 2, 0);
	Task_Create(Task_P0, 0, 0);
	Task_Create(Task_P3, 3, 0);
	
    configure_timer();
}

#else

#error TEST specifies invalid test number

#endif
