#include <roomba.h>
#include <stdbool.h>
#include <string.h>


void a_main (void) {
	
	struct roomba_opt opt;
	memset(&opt,0,sizeof(opt));
	opt.uart=1;
	opt.high_baud=false;
	
	struct roomba r;
	if (roomba_create(&r,opt)!=0) return;
	
}
