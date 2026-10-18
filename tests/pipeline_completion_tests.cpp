// Execute the production process_state/start/stop bodies with deterministic
// event groups, task deletion and a speaker whose output drains independently.
#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <iostream>
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
using EventBits_t=uint32_t;
using esp_err_t=int;
constexpr int ESP_OK=0;
EventBits_t xEventGroupGetBits(EventBits_t *bits){return *bits;}
void xEventGroupClearBits(EventBits_t *bits,EventBits_t value){*bits&=~value;}
void xEventGroupSetBits(EventBits_t *bits,EventBits_t value){*bits|=value;}
bool xQueueReceive(void *,void *,int){return false;}
namespace audio { enum class AudioFileType{MP3};struct AudioFile{};struct AudioStreamInfo{}; }
#include "pipeline_types.inc"
struct Task {
 bool created{};unsigned delayed_deletions{};
 bool is_created() const{return created;}
 bool deallocate(){if(delayed_deletions){--delayed_deletions;return false;}created=false;return true;}
};
struct Speaker {
 bool running{true};unsigned stops{},finishes{};
 bool is_running()const{return running;}
 void stop(){++stops;running=false;}
 void finish(){++finishes;} // Output may remain running until a later poll.
};
struct AudioPipeline {
 PipelineOutcome outcome_{PipelineOutcome::NONE};uint32_t session_{};bool completion_pending_{};
 bool hard_stop_{},is_playing_{},is_finishing_{},pause_state_{},pending_url_{},pending_file_{};
 Task read_task_,decode_task_;Speaker *speaker_;EventBits_t bits{};EventBits_t *event_group_{&bits};
 void *info_error_queue_{};std::string current_uri_;audio::AudioFile *current_audio_file_{};uint32_t playback_ms_{};
 void start_tasks_(){read_task_.created=decode_task_.created=true;}
 void start_file(audio::AudioFile *);void start_url(const std::string &);esp_err_t stop();AudioPipelineState process_state();
};
#include "pipeline_methods.inc"
int main(){
 constexpr auto finished=READER_MESSAGE_FINISHED|DECODER_MESSAGE_FINISHED;
 constexpr auto success=finished|READER_MESSAGE_EOF|DECODER_MESSAGE_SUCCESS;
 audio::AudioFile file;
 for(bool failure:{false,true}){
  Speaker speaker;AudioPipeline p;p.speaker_=&speaker;p.start_file(&file);p.process_state();
  p.bits=success;p.is_playing_=false;
  if(failure){p.bits|=DECODER_MESSAGE_ERROR|PIPELINE_COMMAND_STOP;assert(p.process_state()==AudioPipelineState::ERROR_DECODING);}
  p.process_state();
  if(failure){assert(p.outcome_==PipelineOutcome::FAILED && speaker.stops==1);}
  else {assert(speaker.finishes==1 && p.outcome_==PipelineOutcome::NONE);speaker.running=false;p.process_state();assert(p.outcome_==PipelineOutcome::COMPLETED);}
 }
 for(auto bits:{finished,finished|READER_MESSAGE_EOF,finished|DECODER_MESSAGE_SUCCESS}){
  Speaker speaker;speaker.running=false;AudioPipeline p;p.speaker_=&speaker;p.start_file(&file);p.process_state();
  p.bits=bits;p.is_playing_=false;p.process_state();assert(p.outcome_!=PipelineOutcome::COMPLETED);
 }
 Speaker speaker;AudioPipeline p;p.speaker_=&speaker;p.start_file(&file);p.process_state();p.bits=success;p.stop();
 p.is_playing_=false;p.process_state();assert(p.outcome_==PipelineOutcome::CANCELLED && speaker.stops==1);
 p.start_file(&file);assert(p.outcome_==PipelineOutcome::NONE && p.session_==2);p.process_state();
 assert(!(p.bits&(READER_MESSAGE_EOF|DECODER_MESSAGE_SUCCESS)));
 p.bits=success;p.is_playing_=false;p.read_task_.delayed_deletions=1;
 assert(p.process_state()==AudioPipelineState::PLAYING && p.outcome_==PipelineOutcome::NONE);
 p.process_state();speaker.running=false;p.process_state();assert(p.outcome_==PipelineOutcome::COMPLETED);
 std::cout<<"Production pipeline: drained completion, failure latch, Stop, stale flags and delayed task deletion passed\n";
}
