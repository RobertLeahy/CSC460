/**
 *	\file
 */


#pragma once


#include <interrupt.h>
#include <stdbool.h>


#define EMPTY ((void)0)


#ifdef DEBUG

//	Delays long enough that the logic analyzer
//	can pick up a transition before this
#define LOGIC_ANALYZER_DELAY asm volatile (	\
	"nop\r\n"	\
	"nop\r\n"	\
	"nop\r\n"	\
	"nop\r\n"	\
	"nop\r\n"	\
	"nop\r\n"	\
	"nop\r\n"	\
	"nop\r\n"	\
	"nop\r\n"	\
	"nop\r\n"	\
	"nop\r\n"	\
	"nop\r\n"	\
	"nop\r\n"	\
	"nop\r\n"	\
	"nop\r\n"	\
	"nop"	\
)

#define debug_high_impl(port,pin) do {	\
	bool i=disable_and_push_interrupt();	\
	(PORT ## port|=1<<(pin));	\
	pop_interrupt(i);	\
} while (0)
#define debug_high(port,pin) debug_high_impl(port,pin)
#define debug_low_impl(port,pin) do {	\
	bool i=disable_and_push_interrupt();	\
	(PORT ## port&=~(1<<(pin)));	\
	pop_interrupt(i);	\
} while (0)
#define debug_low(port,pin) debug_low_impl(port,pin)

#define debug_setup_impl(port,pin,hi) do {	\
	DDR ## port|=1<<(pin);	\
	if (hi) debug_high(port,(pin));	\
	else debug_low(port,(pin));	\
} while (0)
#define debug_setup(port,pin,hi) debug_setup_impl(port,pin,hi)

#define debug_pulse_impl(port,pin,num) do {	\
	size_t bound=(num);	\
	for (size_t i=0;i<bound;++i) {	\
		if (i!=0) LOGIC_ANALYZER_DELAY;	\
		debug_high(port,(pin));	\
		LOGIC_ANALYZER_DELAY;	\
		debug_low(port,(pin));	\
	}	\
} while (0)
#define debug_pulse(port,pin,num) debug_pulse_impl(port,pin,num)

//	Current pin allocations:
//
//	Pin 22:	High when in the kernel
//	Pin 23: High when in user space
//	Pin 24: High as scheduler ISR enters until end of reschedule
//	Pin 25: Pulsed when sleep timer overflows
//	Pin 26: High during UART UDRE interrupt
//	Pin 27: High during UART RX interrupt
//	Pin 28: High during context switching
//	Pin 29: Pulses whenever sleep timer compare interrupt occurs

//	DISABLED - Previously port 27
#define DEBUG_THREAD_PORT A
#define DEBUG_THREAD_PIN (PA5)
#define DEBUG_THREAD_SETUP debug_setup(DEBUG_THREAD_PORT,DEBUG_THREAD_PIN,false)
#undef DEBUG_THREAD_SETUP
#define DEBUG_THREAD_SETUP EMPTY
#define debug_thread_signal() debug_pulse(DEBUG_THREAD_PORT,DEBUG_THREAD_PIN,(current_thread-threads))
#undef debug_thread_signal
#define debug_thread_signal() EMPTY

#define DEBUG_KERNEL_PORT A
#define DEBUG_KERNEL_PIN (PA0)
#define DEBUG_KERNEL_SETUP debug_setup(DEBUG_KERNEL_PORT,DEBUG_KERNEL_PIN,true)
#define debug_kernel_enter() debug_high(DEBUG_KERNEL_PORT,DEBUG_KERNEL_PIN)
#define debug_kernel_exit() do {	\
	debug_thread_signal();	\
	debug_low(DEBUG_QUANTUM_PORT,DEBUG_QUANTUM_PIN);	\
	debug_low(DEBUG_KERNEL_PORT,DEBUG_KERNEL_PIN);	\
} while (0)

#define DEBUG_USER_SPACE_PORT A
#define DEBUG_USER_SPACE_PIN (PA1)
#define DEBUG_USER_SPACE_SETUP debug_setup(DEBUG_USER_SPACE_PORT,DEBUG_USER_SPACE_PIN,false)
#define debug_user_space_enter() debug_high(DEBUG_USER_SPACE_PORT,DEBUG_USER_SPACE_PIN)
#define debug_user_space_exit() debug_low(DEBUG_USER_SPACE_PORT,DEBUG_USER_SPACE_PIN)

#define DEBUG_QUANTUM_PORT A
#define DEBUG_QUANTUM_PIN (PA2)
#define DEBUG_QUANTUM_SETUP debug_setup(DEBUG_QUANTUM_PORT,DEBUG_QUANTUM_PIN,false)
#define debug_quantum() debug_high(DEBUG_QUANTUM_PORT,DEBUG_QUANTUM_PIN)

#define DEBUG_SLEEP_TIMER_PORT A
#define DEBUG_SLEEP_TIMER_PIN (PA7)
#define DEBUG_SLEEP_TIMER_SETUP debug_setup(DEBUG_SLEEP_TIMER_PORT,DEBUG_SLEEP_TIMER_PIN,false)
#define debug_sleep_timer() debug_pulse(DEBUG_SLEEP_TIMER_PORT,DEBUG_SLEEP_TIMER_PIN,1)

#define DEBUG_SLEEP_OVERFLOW_PORT A
#define DEBUG_SLEEP_OVERFLOW_PIN (PA3)
#define DEBUG_SLEEP_OVERFLOW_SETUP debug_setup(DEBUG_SLEEP_OVERFLOW_PORT,DEBUG_SLEEP_OVERFLOW_PIN,false)
#define debug_sleep_overflow() debug_pulse(DEBUG_SLEEP_OVERFLOW_PORT,DEBUG_SLEEP_OVERFLOW_PIN,1)

//	DISABLED - Previously port 26
#define DEBUG_SYSCALL_PORT A
#define DEBUG_SYSCALL_PIN (PA4)
#define DEBUG_SYSCALL_SETUP debug_setup(DEBUG_SYSCALL_PORT,DEBUG_SYSCALL_PIN,false)
#undef DEBUG_SYSCALL_SETUP
#define DEBUG_SYSCALL_SETUP EMPTY
#define debug_syscall() debug_pulse(DEBUG_SYSCALL_PORT,DEBUG_SYSCALL_PIN,syscall_state.num)
#undef debug_syscall
#define debug_syscall() EMPTY

//	DISABLED - Previously port 28
#define DEBUG_MAINTAIN_SLEEP_PORT A
#define DEBUG_MAINTAIN_SLEEP_PIN (PA6)
#define DEBUG_MAINTAIN_SLEEP_SETUP debug_setup(DEBUG_MAINTAIN_SLEEP_PORT,DEBUG_MAINTAIN_SLEEP_PIN,false)
#undef DEBUG_MAINTAIN_SLEEP_SETUP
#define DEBUG_MAINTAIN_SLEEP_SETUP EMPTY
#define debug_maintain_sleep_enter() debug_high(DEBUG_MAINTAIN_SLEEP_PORT,DEBUG_MAINTAIN_SLEEP_PIN)
#undef debug_maintain_sleep_enter
#define debug_maintain_sleep_enter() EMPTY
#define debug_maintain_sleep_exit() debug_low(DEBUG_MAINTAIN_SLEEP_PORT,DEBUG_MAINTAIN_SLEEP_PIN)
#undef debug_maintain_sleep_exit
#define debug_maintain_sleep_exit() EMPTY

#define DEBUG_CONTEXT_SWITCH_PORT A
#define DEBUG_CONTEXT_SWITCH_PIN (PA6)
#define DEBUG_CONTEXT_SWITCH_SETUP debug_setup(DEBUG_CONTEXT_SWITCH_PORT,DEBUG_CONTEXT_SWITCH_PIN,false)
#define debug_context_switch_begin() debug_high(DEBUG_CONTEXT_SWITCH_PORT,DEBUG_CONTEXT_SWITCH_PIN)
#define debug_context_switch_end() debug_low(DEBUG_CONTEXT_SWITCH_PORT,DEBUG_CONTEXT_SWITCH_PIN)

#define DEBUG_UART_RX_PORT A
#define DEBUG_UART_RX_PIN (PA5)
#define DEBUG_UART_RX_SETUP debug_setup(DEBUG_UART_RX_PORT,DEBUG_UART_RX_PIN,false)
#define debug_uart_rx_begin() debug_high(DEBUG_UART_RX_PORT,DEBUG_UART_RX_PIN)
#define debug_uart_rx_end() debug_low(DEBUG_UART_RX_PORT,DEBUG_UART_RX_PIN)

#define DEBUG_UART_UDRE_PORT A
#define DEBUG_UART_UDRE_PIN (PA4)
#define DEBUG_UART_UDRE_SETUP debug_setup(DEBUG_UART_UDRE_PORT,DEBUG_UART_UDRE_PIN,false)
#define debug_uart_udre_begin() debug_high(DEBUG_UART_UDRE_PORT,DEBUG_UART_UDRE_PIN)
#define debug_uart_udre_end() debug_low(DEBUG_UART_UDRE_PORT,DEBUG_UART_UDRE_PIN)

#define DEBUG_SETUP do {	\
	DEBUG_KERNEL_SETUP;	\
	DEBUG_USER_SPACE_SETUP;	\
	DEBUG_THREAD_SETUP;	\
	DEBUG_QUANTUM_SETUP;	\
	DEBUG_SLEEP_TIMER_SETUP;	\
	DEBUG_SLEEP_OVERFLOW_SETUP;	\
	DEBUG_SYSCALL_SETUP;	\
	DEBUG_MAINTAIN_SLEEP_SETUP;	\
	DEBUG_CONTEXT_SWITCH_SETUP;	\
	DEBUG_UART_RX_SETUP;	\
	DEBUG_UART_UDRE_SETUP;	\
} while (0)

#else

#define debug_kernel_enter() EMPTY
#define debug_kernel_exit() EMPTY

#define debug_user_space_enter() EMPTY
#define debug_user_space_exit() EMPTY

#define debug_quantum() EMPTY

#define debug_sleep_timer() EMPTY

#define debug_sleep_overflow() EMPTY

#define debug_syscall() EMPTY

#define debug_maintain_sleep_enter() EMPTY
#define debug_maintain_sleep_exit() EMPTY

#define debug_context_switch_begin() EMPTY
#define debug_context_switch_end() EMPTY

#define debug_uart_rx_begin() EMPTY
#define debug_uart_rx_end() EMPTY

#define debug_uart_udre_begin() EMPTY
#define debug_uart_udre_end() EMPTY

#define DEBUG_SETUP EMPTY

#endif
