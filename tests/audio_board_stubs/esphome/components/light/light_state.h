#pragma once
namespace esphome::light {
struct LightState {
 bool on{}; float red{},green{},blue{},brightness{}; unsigned transition{},calls{};
 struct Call {
  LightState &s;
  void set_state(bool v){s.on=v;}
  void set_transition_length(unsigned v){s.transition=v;}
  void set_rgb(float r,float g,float b){s.red=r;s.green=g;s.blue=b;}
  void set_brightness(float v){s.brightness=v;}
  void perform(){++s.calls;}
 };
 Call make_call(){return {*this};}
};
}
