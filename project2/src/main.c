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

#else

#error TEST specifies invalid test number

#endif
