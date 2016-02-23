#include <kernel.h>
#include <os.h>


error_t * get_last_error (void) {
	
	if (current_thread) return &current_thread->last_error;
	
	return &last_error;
	
}
