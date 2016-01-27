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


class servo {
	
	
	private:
	
	
		scheduler & s_;
		size_t i_;
		unsigned long w_;
		unsigned long p_;
		bool state_;
		
		
		static constexpr unsigned long period=20000U;
		static constexpr unsigned long min=500U;
		static constexpr unsigned long max=2500U;
		static constexpr unsigned long default_width=min+((max-min)/2U);
		
		
		static void callback (scheduler &, size_t, void * ptr) {
			
			auto & self=*reinterpret_cast<servo *>(ptr);
			
			if (self.state_) {
				
				digitalWrite(self.p_,LOW);
				self.state_=false;
				
				return;
				
			}
			
			digitalWrite(self.p_,HIGH);
			self.s_.schedule(self.i_,self.w_);
			self.state_=true;
			
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
		
		
		servo (scheduler & s, size_t i, unsigned long pin, unsigned long w=default_width, unsigned long delay=0) : s_(s), i_(i), w_(w), p_(pin), state_(false) {
			
			check_width();
			s.create(i,period,delay,&callback,this);
			
		}
		
		
		~servo () {
			
			deactivate();
			
		}
		
		
		explicit operator bool () const {
			
			return s_.is_active(i_);
			
		}
		
		
		void activate () const {
			
			s_.activate(i_);
			
		}
		
		
		void deactivate () const {
			
			s_.deactivate(i_);
			
		}
		
		
		unsigned long width () const {
			
			return w_;
			
		}
		void width (unsigned long w) {
			
			w_=w;
			
			check_width();
			
		}
	
	
};


void setup () {
	
	pinMode(22,OUTPUT);
	
}


struct direction {
	
	
	unsigned long width;
	bool up;
	::servo & servo;
	
	
};


void sweep (scheduler & s, size_t, void * ptr) {
	
	direction & d=*reinterpret_cast<direction *>(ptr);
	
	if (d.width==2500) d.up=false;
	else if (d.width==500) d.up=true;
	
	if (d.up) d.width+=100;
	else d.width-=100;
	
	d.servo.width(d.width);
	
}


void loop() {
	
	scheduler s;
	servo se(s,0,22,500U);
	se.activate();
	
	direction d{500U,true,se};
	s.create(1,20000,100,&sweep,&d);
	
	for (;;) s();
	
}
