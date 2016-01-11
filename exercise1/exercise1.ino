#include <StandardCplusplus.h>
#include <system_configuration.h>
#include <unwind-cxx.h>
#include <utility.h>


void setup () {
	
	Serial.begin(9600);
	while (!Serial);
	
	Serial1.begin(9600);
	while (!Serial1);
	
}


static std::size_t from_computer (std::size_t bytes, bool & done) {
	
	if (done) return bytes;
	
	auto byte=Serial.read();
	if (byte==-1) return bytes;
	if (byte=='\x04') {
		
		done=true;
		
		return bytes;
		
	}
	
	while (Serial1.write(byte)==0);
	
	return bytes+1;
	
}


static std::size_t from_loopback (std::size_t bytes) {
	
	auto byte=Serial1.read();
	if (byte==-1) return bytes;
	
	if ((bytes!=0) && ((bytes%30U)==0)) {
		
		while (Serial.write('\r')==0);
		while (Serial.write('\n')==0);
		
	}
	
	while (Serial.write(byte)==0);
	
	return bytes+1;
	
}


void loop () {
	
	std::size_t in(0);
	std::size_t out(0);
	bool done(false);
	for (;;) {
		
		in=from_computer(in,done);
		
		out=from_loopback(out);
		
		if (done && (in==out)) break;
		
	}
	
	Serial.print("\r\n\r\nTotal bytes: ");
	Serial.print(out);
	
	//	Just wait forever
	for (;;);
	
}
