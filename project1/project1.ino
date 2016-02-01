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


class serial_control : public serial {
	
	
	private:
	
	
		servo & se_;
		unsigned long pin_;
		
		
	protected:
	
	
		virtual void receive (unsigned char b) override {
			
			unsigned char servo_part=b&127U;
			bool laser=(b&128U)!=0;
			
			Serial.print(static_cast<unsigned>(servo_part));
			Serial.print(' ');
			
			if (laser) digitalWrite(pin_,HIGH);
			else digitalWrite(pin_,LOW);
			
			unsigned long width=2000;
			width*=servo_part;
			width/=128U;
			width+=500U;
			Serial.print(width);
			Serial.print(' ');
			se_.width(width);
			
		}
		
		
	public:
	
	
		serial_control () = delete;
		
		
		serial_control (servo & se, unsigned long pin, Stream & st, scheduler & s, size_t i, unsigned long delay=0, bool active=true)
			:	serial(st,s,i,delay,active),
				se_(se),
				pin_(pin)
		{	}
	
	
};


void setup () {
	
	pinMode(22,OUTPUT);
	pinMode(26,OUTPUT);
	Serial.begin(9600);
	Serial1.begin(9600);
	
}


void loop() {
	
	scheduler s;
	servo se(s,0,22,500);
	se.activate();
	serial_control sc(se,26,Serial1,s,1,10000);
	sc.activate();
	
	for (;;) s();
	
}
