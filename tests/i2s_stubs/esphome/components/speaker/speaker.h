#pragma once
#include "fake_i2s.h"
#include "esphome/components/audio/audio.h"
namespace esphome::speaker {
enum State{STATE_STOPPED,STATE_STARTING,STATE_RUNNING,STATE_STOPPING};
class Speaker {
 public:
 virtual ~Speaker()=default;
 virtual void start()=0;virtual void stop()=0;virtual void finish()=0;
 virtual void set_pause_state(bool)=0;virtual bool get_pause_state()const=0;
 virtual size_t play(const uint8_t*,size_t,TickType_t)=0;virtual size_t play(const uint8_t*,size_t)=0;
 virtual bool has_buffered_data()const=0;virtual void set_volume(float)=0;virtual void set_mute_state(bool)=0;
 void set_audio_stream_info(audio::AudioStreamInfo i){audio_stream_info_=i;}
 protected:State state_=STATE_STOPPED;float volume_=1;bool mute_state_{};audio::AudioStreamInfo audio_stream_info_;
 std::function<void(uint32_t,int64_t)> audio_output_callback_=[](auto,auto){};
};
}
