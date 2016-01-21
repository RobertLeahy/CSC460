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
	
	
		explicit scheduler () {
			
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
			t.next=t.next_regular=now+t.period;
			
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



void setup () {
	
	pinMode(22,OUTPUT);
	
}

static void pwm (scheduler & s, size_t i, void * ptr) {
	
	bool & pin_state=*reinterpret_cast<bool *>(ptr);
	if (pin_state) {
		
		digitalWrite(22,LOW);
		pin_state=false;
		return;
		
	}
	
	digitalWrite(22,HIGH);
	s.schedule(i,1500);
	pin_state=true;
	
}

void loop() {
	
	scheduler s;
	bool pin_state=false;
	s.create(0,20000,0,&pwm,&pin_state);
	
	for (;;) s();
	
}
