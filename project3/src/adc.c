#include <adc.h>
#include <avr/io.h>


void adc_init (void) {
	
	ADCSRA|=(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
	ADMUX|=(1<<REFS0);
	ADCSRA|=(1<<ADEN);
	ADCSRA|=(1<<ADSC);
	
}


uint16_t adc_read (unsigned char c) {
	
	ADMUX&=0xE0;
	ADMUX|=c&0x07;
	ADCSRB=c&(1<<3);
	ADCSRA|=(1<<ADSC);
	while (ADCSRA&(1<<ADSC));
	return ADCW;
	
}
