#pragma once
#include <functional>
namespace esphome {
namespace setup_priority { constexpr float IO=900,HARDWARE=800,DATA=600,PROCESSOR=400; }
class Component {
 public:
 virtual ~Component()=default;
 virtual void setup(){} virtual void loop(){} virtual void on_shutdown(){}
 virtual void dump_config(){} virtual float get_setup_priority()const{return 0;}
 void mark_failed(){failed_=true;} bool is_failed()const{return failed_;} bool is_ready()const{return !failed_;}
 bool status_has_error()const{return error_;} void status_clear_error(){error_=false;}
 void status_momentary_error(const char*,unsigned){error_=true;}
 void set_timeout(unsigned,std::function<void()> f){f();}
 private: bool failed_{},error_{};
};
}
