class scheduler {


	public:
	
	
		using callback_type=void (*) (scheduler &, size_t, void *);


	private:
	
	
		class task {
			
			
			public:
			
			
				callback_type callback;
				void * ptr;
				unsigned long next;
				unsigned long next_regular;
				unsigned long period;
				bool active;
			
			
		};
		
		
		static constexpr size_t num=8;
		task ts_ [num];
		
		
	public:
	
	
		scheduler (const scheduler &) = delete;
		scheduler (scheduler &&) = delete;
		scheduler & operator = (const scheduler &) = delete;
		scheduler & operator = (scheduler &&) = delete;
		
		
		scheduler () {
			
			memset(ts_,0,sizeof(ts_));
			
		}
		
		
		void operator () () {
			
			auto now=micros();
			size_t i=0;
			for (;i<num;++i) {
				
				auto & curr=ts_[i];
				
				if (!curr.active) continue;
				
				if (curr.next<=now) break;
				
			}
			
			if (i==num) return;
			
			auto & t=ts_[i];
			if (t.next==t.next_regular) t.next_regular=t.next_regular+t.period;
			t.next=t.next_regular;
			
			t.callback(*this,i,t.ptr);
			
		}
		
		
		void activate (size_t i) {
			
			auto & t=ts_[i];
			if (t.active) return;
			t.active=true;
			auto now=micros();
			t.next=t.next_regular=now;
			
		}
		
		
		void deactivate (size_t i) {
			
			ts_[i].active=false;
			
		}
		
		
		bool is_active (size_t i) {
			
			return ts_[i].active;
			
		}
		
		
		void create (size_t i, unsigned long period, unsigned long delay, callback_type callback, void * ptr, bool active=true) {
		
			auto & t=ts_[i];
			t.callback=callback;
			t.ptr=ptr;
			auto now=micros();
			t.next_regular=t.next=now+delay;
			t.active=active;
			t.period=period;
		
		}
		
		
		void schedule (size_t i, unsigned long delay) {
		
			auto now=micros();
			ts_[i].next=now+delay;
		
		}


};


class scheduled_task {


	private:
	
	
		scheduler & s_;
		size_t i_;
		
		
		static void callback (scheduler &, size_t, void * ptr) {
			
			auto & self=*reinterpret_cast<scheduled_task *>(ptr);
			self.execute();
			
		}
		
		
	protected:
	
	
		virtual void execute () = 0;
		
		
	public:
	
	
		scheduled_task () = delete;
		scheduled_task (const scheduled_task &) = delete;
		scheduled_task (scheduled_task &&) = delete;
		scheduled_task & operator = (const scheduled_task &) = delete;
		scheduled_task & operator = (scheduled_task &&) = delete;
		
		
		scheduled_task (scheduler & s, size_t i, unsigned long period, unsigned long delay=0, bool active=true) : s_(s), i_(i) {
			
			s_.create(i_,period,delay,&callback,this,active);
			
		}
		
		
		virtual ~scheduled_task () noexcept {
			
			deactivate();
			
		}
		
		
		void activate () noexcept {
			
			s_.activate(i_);
			
		}
		
		
		void deactivate () noexcept {
			
			s_.deactivate(i_);
			
		}
		
		
		explicit operator bool () const noexcept {
			
			return s_.is_active(i_);
			
		}
		
		
		void schedule (unsigned long delay) {
			
			s_.schedule(i_,delay);
			
		}


};


class servo : public scheduled_task {
	
	
	private:
	
	
		unsigned long w_;
		unsigned long p_;
		bool state_;
		
		
		static constexpr unsigned long period=20000U;
		static constexpr unsigned long min=500U;
		static constexpr unsigned long max=2500U;
		static constexpr unsigned long default_width=min+((max-min)/2U);
		
		
	protected:
	
	
		virtual void execute () override {
			
			if (state_) {
				
				digitalWrite(p_,LOW);
				state_=false;
				
				return;
				
			}
			
			digitalWrite(p_,HIGH);
			schedule(w_);
			state_=true;
			
		}
		
		
		void check_width () {
			
			if (w_>max) w_=max;
			else if (w_<min) w_=min;
			
		}
		
		
	public:
	
	
		servo () = delete;
		servo (const servo &) = delete;
		servo (servo &&) = delete;
		servo & operator = (const servo &) = delete;
		servo & operator = (servo &&) = delete;
		
		
		servo (scheduler & s, size_t i, unsigned long pin, unsigned long w=default_width, unsigned long delay=0) : scheduled_task(s,i,period,delay), w_(w), p_(pin), state_(false) {
			
			check_width();
			
		}
		
		
		unsigned long width () const {
			
			return w_;
			
		}
		void width (unsigned long w) {
			
			w_=w;
			
			check_width();
			
		}
	
	
};


class serial : public scheduled_task {


	private:
		
	
		Stream & s_;
		
		
		static constexpr unsigned long period=1000U*1000U/20U;
		
		
	protected:
	
	
		virtual void receive (unsigned char b) = 0;
		
		
		virtual void execute () override {
			
			for (auto b=s_.read();b!=-1;b=s_.read()) receive(static_cast<unsigned char>(b));
			
		}
		
		
	public:
	
	
		serial (Stream & st, scheduler & s, size_t i, unsigned long delay=0, bool active=true) : scheduled_task(s,i,period,delay,active), s_(st) {	}


};

#include <LiquidCrystal.h>

#define JOYSTICK_BUTTON_PIN 24
#define JOYSTICK_XAXIS_PIN A8
#define LIGHT_SENSOR_PIN A9

#define LIGHT_SENSOR_THRESHOLD 800

int joystick_x_value = 512;
bool joystick_pressed = false;
bool light_sensor_active = false;

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

void setup () {

	pinMode(40, OUTPUT);
	pinMode(JOYSTICK_BUTTON_PIN, INPUT_PULLUP);
	
	// Initialize LCD screen to 16x2 characters
	lcd.begin(16, 2);

	// Display the label for the joystick
	lcd.setCursor(0,0);
	lcd.print("Joystick:");
	
	// Display the label for the joystick
	lcd.setCursor(14,0);
	lcd.print(",");
	
	// Display the label for the light sensor
	lcd.setCursor(0,1);
	lcd.print("Light:");
	
	Serial1.begin(9600);
}

void show_lcd (scheduler & s, size_t, void * ptr) {
	
	// Clear LCD
	lcd.setCursor(10,0);
	lcd.print("    ");
	
	// Print joystick x value
	lcd.setCursor(10,0);
	lcd.print(joystick_x_value);
	
	// Print joystick pressed value
	lcd.setCursor(15,0);
	lcd.print(joystick_pressed);
	
	// Print light sensor value
	lcd.setCursor(7,1);
	lcd.print(light_sensor_active ? "On " : "Off");
}

void check_joystick (scheduler & s, size_t, void * ptr) {
	
	// Joystick value has range 0 - 1023
	joystick_x_value = analogRead(JOYSTICK_XAXIS_PIN);
	
	joystick_pressed = digitalRead(JOYSTICK_BUTTON_PIN) == LOW;
	
	// Combine 'x value' and 'joystick pressed value' into a single byte
	//   X value should be 0 - 127
	//   Set the high bit to 0 or 1 to indicate if the joystick was pressed
	int output_value = (joystick_x_value >> 3) | (128 * joystick_pressed);
	
	// Send the byte over bluetooth
	Serial1.write(output_value);
}

void check_light_sensor (scheduler & s, size_t, void * ptr) {
	
	light_sensor_active = analogRead(LIGHT_SENSOR_PIN) >= LIGHT_SENSOR_THRESHOLD;
}

void loop() {
	
	scheduler s;
	
	s.create(1, 20000, 0, &check_joystick, NULL);
	s.create(2, 100000, 100, &check_light_sensor, NULL);
	s.create(3, 100000, 200, &show_lcd, NULL);
	
	for (;;) s();
	
}
