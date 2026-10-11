#include "esphome/components/voice_pyramid_audio/audio.h"
#include "nvs_memory.h"
#include <cassert>
#include <limits>
#include <gain.h>

using namespace esphome;
using namespace voice_pyramid_audio;
using voice_pyramid::Clock;
using voice_pyramid::Amplifier;
using ::openathan::LoadResult;
using ::openathan::SavedSettings;

static SavedSettings legacy_settings(unsigned volume = 80) {
  SavedSettings settings;
  settings.revision = 12; settings.value.volume = volume;
  settings.value.timezone.name = "UTC";
  settings.value.prayer.offsets[1] = 4;
  return settings;
}
static void install_legacy(const SavedSettings &settings) {
  openathan_component::NvsSettingsStore store;
  SavedSettings unused;
  assert(store.load(unused) == LoadResult::EMPTY && store.save(settings));
}
static float legacy_db(unsigned percent) { return (static_cast<int>(legacy_register(percent)) - 191) * 0.5f; }
static void curves() {
  assert(migrate_volume(0) == 0 && migrate_volume(70) == 56 && migrate_volume(80) == 64 && migrate_volume(100) == 80);
  assert(volume_plan(0).software_level == 0 && volume_plan(100).codec_level == 0.75f &&
         volume_plan(100).software_level == 1.0f);
  for (unsigned old = 1; old <= 100; ++old) {
    const unsigned migrated = migrate_volume(old);
    assert(migrated >= 1 && migrated <= 100 && migrated >= migrate_volume(old - 1));
    assert(volume_db(migrated) <= legacy_db(old) + 0.0001f);
    assert(legacy_db(old) - volume_db(migrated) < 0.966f);
  }
  for (unsigned percent = 1; percent <= 100; ++percent) {
    const auto plan = volume_plan(percent);
    assert(plan.codec_level <= 0.75f && plan.codec_level >= 0);
    assert(plan.software_level >= 0.001f && plan.software_level <= 1);
    const int reg = static_cast<int>(plan.codec_level * 255);
    const float effective = (reg - 191) * 0.5f - 49 * (1 - plan.software_level);
    assert(std::fabs(effective - volume_db(percent)) < 0.0001f);
    const unsigned rollback = rollback_volume(percent);
    assert(rollback <= 100 && (!rollback || legacy_db(rollback) <= volume_db(percent) + 0.0001f));
  }
}
static void storage() {
  nvs_test::reset(); install_legacy(legacy_settings());
  nvs_test::committed[{openathan_storage::PRAYER, "state"}] = {1,2,3};
  nvs_test::committed[{"other", "settings"}] = {4,5,6};
  const auto before = nvs_test::committed;
  SavedSettings migrated;
  VolumeSettingsStore store;
  assert(store.load(migrated) == LoadResult::LOADED);
  assert(migrated.revision == 13 && migrated.value.volume == 64);
  assert(migrated.value.prayer == legacy_settings().value.prayer);
  assert(nvs_test::committed == before && nvs_test::writes == 1);
  ::openathan::SettingsService service(store);
  assert(service.begin(migrated.value));
  auto next = migrated.value; next.volume = 100;
  assert(service.update(next, 12, [](const auto &) { return true; }) == ::openathan::SettingsResult::CONFLICT);
  assert(service.update(next, 13, [](const auto &) { return true; }) == ::openathan::SettingsResult::SAVED);
  VolumeSettingsStore reboot;
  SavedSettings restored;
  assert(reboot.load(restored) == LoadResult::LOADED && restored == *service.saved());
  openathan_component::NvsSettingsStore old_firmware;
  SavedSettings mirror;
  assert(old_firmware.load(mirror) == LoadResult::LOADED && mirror.value.volume == 100);
  assert(mirror.value.prayer == next.prayer && mirror.revision == 14);
  mirror.value.volume = 75; mirror.value.prayer.offsets[2] = 7; ++mirror.revision;
  assert(old_firmware.save(mirror));
  VolumeSettingsStore after_downgrade;
  assert(after_downgrade.load(restored) == LoadResult::LOADED);
  assert(restored.value.volume == migrate_volume(75) && restored.revision == 16);
  assert(restored.value.prayer == mirror.value.prayer);
  assert(nvs_test::committed.at({openathan_storage::PRAYER, "state"}) == before.at({openathan_storage::PRAYER, "state"}));
  assert(nvs_test::committed.at({"other", "settings"}) == before.at({"other", "settings"}));
  nvs_test::committed[{openathan_storage::PRAYER, "pyr_volume"}][40] ^= 1;
  const auto corrupt = nvs_test::committed;
  VolumeSettingsStore damaged;
  assert(damaged.load(restored) == LoadResult::ERROR && nvs_test::committed == corrupt);
  nvs_test::reset(); VolumeSettingsStore fresh;
  assert(fresh.load(restored) == LoadResult::EMPTY);
  const auto defaults = legacy_settings(56);
  assert(fresh.save(defaults));
  VolumeSettingsStore fresh_reboot;
  assert(fresh_reboot.load(restored) == LoadResult::LOADED && restored == defaults);
  nvs_test::reset(); auto maximum = legacy_settings(); maximum.revision = UINT32_MAX;
  install_legacy(maximum); VolumeSettingsStore exhausted;
  assert(exhausted.load(restored) == LoadResult::ERROR);
}
static void interrupted_saves() {
  for (unsigned target : {40U, 100U}) for (bool companion : {false, true})
      for (bool early : {false, true}) for (bool commit : {false, true}) for (unsigned cut = 1; cut <= 2; ++cut) {
    nvs_test::reset(); install_legacy(legacy_settings());
    nvs_test::committed[{openathan_storage::PRAYER, "state"}] = {1,2,3};
    VolumeSettingsStore store; SavedSettings next;
    assert(store.load(next) == LoadResult::LOADED);
    if (companion) assert(store.save(next));
    const auto previous = next;
    ++next.revision; next.value.volume = target;
    next.value.prayer.offsets[3] = 5;
    next.value.prayer.latitude = 43.6532;
    next.value.prayer.longitude = -79.3832;
    const unsigned writes = nvs_test::writes, commits = nvs_test::commits;
    nvs_test::early_persist = early;
    if (commit) nvs_test::cut_commit_number = commits + cut;
    else nvs_test::cut_write_number = writes + cut;
    try { store.save(next); assert(false); } catch (const std::runtime_error &) {}
    nvs_test::power_cycle(); nvs_test::cut_commit_number = nvs_test::cut_write_number = 0;
    VolumeSettingsStore reboot; SavedSettings restored;
    assert(reboot.load(restored) == LoadResult::LOADED);
    // The old level, complete new level, or a quieter rollback mirror survives.
    if (restored.value.prayer == previous.value.prayer) assert(restored == previous);
    else {
      auto expected = next.value;
      expected.volume = restored.value.volume;
      assert(restored.value == expected && restored.value.volume <= target);
    }
    assert(nvs_test::committed.at({openathan_storage::PRAYER, "state"}) == std::vector<uint8_t>({1,2,3}));
  }
  for (bool early : {false, true}) for (bool commit : {false, true}) {
    nvs_test::reset(); install_legacy(legacy_settings());
    VolumeSettingsStore store; SavedSettings next;
    assert(store.load(next) == LoadResult::LOADED); ++next.revision; next.value.volume = 20;
    nvs_test::early_persist = early;
    nvs_test::fail_write = !commit; nvs_test::fail_commit = commit;
    assert(!store.save(next)); nvs_test::power_cycle();
    nvs_test::fail_write = nvs_test::fail_commit = false;
    VolumeSettingsStore reboot; SavedSettings restored;
    assert(reboot.load(restored) == LoadResult::LOADED);
    assert(restored.value.volume == 64 || restored.value.volume <= 20);
  }
}
static void isolated_storage() {
  nvs_test::reset(); install_legacy(legacy_settings());
  const auto production = nvs_test::committed;
  VolumeSettingsStore isolated("oa_validation"); SavedSettings settings;
  assert(isolated.load(settings) == LoadResult::EMPTY);
  settings = legacy_settings(56);
  assert(isolated.save(settings));
  VolumeSettingsStore reboot("oa_validation"); SavedSettings restored;
  assert(reboot.load(restored) == LoadResult::LOADED && restored == settings);
  for (const auto &[key, value] : production) assert(nvs_test::committed.at(key) == value);
  assert(nvs_test::committed.contains({"oa_validation", "settings"}));
  assert(nvs_test::committed.contains({"oa_validation", "pyr_volume"}));
}
int64_t esp_timer_get_time() { return fake_i2s::now; }
namespace esphome::i2s_audio {
struct VolumeFenceTestAccess {
 static void exhaust(VolumeFenceState &state) { state.generation_ = UINT32_MAX; }
 static void exhaust_session(VolumeFenceState &state) { state.session_ = UINT32_MAX; }
};
}
using i2s_audio::VolumeFenceStatus;
struct Fixture {
 Clock clock; Amplifier amplifier; es8311::ES8311 dac;
 i2s_audio::I2SAudioComponent bus; i2s_audio::I2SAudioSpeaker speaker;
 openathan_audio::PartitionAudio playback; Audio audio;
 Fixture(bool hardware=true) {
  fake_i2s::reset();
  speaker.set_parent(&bus); speaker.set_dout_pin(7);speaker.set_slot_bit_width(16);
  speaker.set_buffer_duration(500);speaker.set_timeout(100000);speaker.set_volume_fence(true);speaker.setup();
  amplifier.set_enable_on_startup(false);
  audio.set_speaker(&speaker);audio.set_clock(&clock);audio.set_amplifier(&amplifier);
  audio.set_dac(&dac);audio.set_playback(&playback);
  if(hardware){amplifier.setup();playback.available=true;}
 }
 bool enabled() { uint8_t value{};assert(amplifier.read_byte(1,&value));return value&8; }
 void apply(unsigned value) {assert(audio.request_volume_percent(value));audio.loop();assert(audio.volume_percent()==value);}
 void run() {
  speaker.start();speaker.loop();
  if(!fake_i2s::task){assert(!speaker.volume_fence_healthy());speaker.loop();return;}
  try {fake_i2s::task(fake_i2s::task_arg);}catch(const fake_i2s::Exit&){}
  fake_i2s::on_write={};fake_i2s::on_fill={};speaker.loop();
 }
};
static void amplifier_control() {
 for(bool enabled:{false,true}) {
  Amplifier amp;amp.set_enable_on_startup(enabled);amp.setup();assert(amp.ready());
  uint8_t control{};assert(amp.read_byte(1,&control)&&control==(enabled?0x78:0x70));
  assert(amp.write_byte(1,0xF7)&&amp.set_pa_enabled(true));
  assert(amp.read_byte(1,&control)&&control==0xFF);
  assert(amp.set_pa_enabled(false)&&amp.read_byte(1,&control)&&control==0xF7);
  amp.on_shutdown();assert(amp.read_byte(1,&control)&&control==0xF7);
 }
 for(unsigned failure=0;failure<5;++failure) {
  Amplifier amp;amp.set_enable_on_startup(false);
  amp.fail_at=failure==0?1:failure==1?2:0;amp.fail_read=failure==2;
  amp.wrong_readback=failure==3;if(failure==4)amp.chip_id=0;
  amp.setup();assert(amp.is_failed()&&!amp.ready());
 }
}
static void adapter_idle_and_faults() {
 Fixture startup(false);assert(startup.audio.request_volume_percent(64));
 fake_i2s::now=3000000;startup.audio.loop();assert(!startup.audio.is_failed()&&!startup.audio.ready());
 startup.amplifier.setup();startup.playback.available=true;startup.audio.loop();
 assert(startup.audio.ready()&&startup.enabled()&&startup.audio.volume_percent()==64);
 assert(!startup.audio.request_volume_percent(101));
 startup.apply(0);assert(!startup.enabled()&&startup.dac.mute==0x60&&startup.speaker.fenced_volume()==0);
 startup.apply(1);assert(startup.enabled());startup.audio.on_shutdown();assert(!startup.enabled());
 for(unsigned failure=0;failure<11;++failure) {
  Fixture f;
  f.dac.mute_write_ok=failure!=0;f.dac.volume_write_ok=failure!=1;f.dac.unmute_write_ok=failure!=2;
  f.dac.register_write_ok=failure!=3;f.dac.register_read_ok=failure!=4;
  f.dac.wrong_readback=failure==5;f.dac.wrong_mute_readback=failure==6;
  f.amplifier.fail_at=failure==7?f.amplifier.writes.size()+1:0;
  f.amplifier.wrong_readback=failure==8;
  if(failure==9)f.amplifier.fail_at=f.amplifier.writes.size()+3;
  f.dac.wrong_unmute_readback=failure==10;
  f.audio.request_volume_percent(80);f.audio.loop();
  assert(f.audio.is_failed()&&!f.audio.ready()&&!f.audio.start(::openathan::Track::FAJR));
  assert(f.speaker.fenced_volume()==0);
 }
 Fixture lost;lost.apply(56);lost.playback.available=false;lost.audio.loop();assert(lost.audio.is_failed()&&!lost.enabled());
}
static void streaming_handoff() {
 Fixture f;const std::string url="https://cdn.mp3quran.net/audio/test/001.mp3";
 assert(f.audio.stream_supported()&&!f.audio.start_stream(url));
 f.apply(56);assert(f.audio.start_stream(url));
 assert(f.audio.source()==::openathan::PlaybackSource::QURAN);
 assert(f.audio.stream_state()==::openathan::StreamState::PLAYING);
 assert(f.audio.playback_epoch()==f.playback.playback_epoch());
 auto epoch=f.audio.playback_epoch();f.audio.stop();
 assert(f.audio.playback_epoch()!=epoch&&f.audio.source()==::openathan::PlaybackSource::NONE);
 assert(f.audio.start(::openathan::Track::FAJR)&&f.audio.source()==::openathan::PlaybackSource::ATHAN);
}
static void dma_fence() {
 Fixture f;f.apply(100);
 unsigned buffers=0;bool requested=false,completed=false,old_tail=false;
 const int16_t pcm[4]={20000,20000,20000,20000};
 fake_i2s::on_fill=[&]{f.speaker.loop();f.speaker.play(reinterpret_cast<const uint8_t*>(pcm),sizeof(pcm));};
 fake_i2s::on_write=[&]{
  ++buffers;f.speaker.loop();
  if(buffers==8){
   requested=true;assert(f.audio.request_volume_percent(20));f.audio.loop();
   // The logical media player already reports the new level, while five old
   // descriptors remain in the fake driver. It cannot authorize unmute.
   f.playback.level=volume_plan(20).software_level;
   const auto generation=f.speaker.request_volume_with_fence(volume_plan(20).software_level);
   assert(f.speaker.volume_fence_status(generation)==VolumeFenceStatus::PENDING);
  }
  if(requested){
   f.audio.loop();
   if(!f.audio.ready())assert(!f.enabled()&&f.dac.mute==0x60);
   else completed=true;
   if(!fake_i2s::dma.empty()) {int16_t sample{};std::memcpy(&sample,fake_i2s::dma.front().data(),2);
    if(sample==20000){old_tail=true;assert(!f.enabled());}}
  }
  if(buffers==24)f.speaker.stop();
 };
 f.run();assert(requested&&old_tail&&completed&&f.audio.volume_percent()==20);
 // Restored/queued media-player preferences cannot change owned attenuation.
 auto before=f.speaker.fenced_volume();f.speaker.set_volume(1);f.speaker.set_mute_state(true);
 f.speaker.set_mute_state(false);assert(f.speaker.fenced_volume()==before);
}
static void rapid_pause_stop_restart() {
 Fixture f;f.apply(100);assert(f.audio.start(::openathan::Track::NORMAL));
 unsigned buffers=0;bool resumed=false;size_t resumed_after=0;
 const int32_t pcm[8]={INT32_MAX/2,INT32_MAX/2,INT32_MAX/2,INT32_MAX/2,INT32_MAX/2,INT32_MAX/2,INT32_MAX/2,INT32_MAX/2};
 f.speaker.set_audio_stream_info({32,1,44100});
 unsigned fills=0;
 fake_i2s::on_fill=[&]{f.speaker.loop();++fills;
  // A short first chunk followed by a full source fill leaves a staged raw
  // frame beyond the current DMA descriptor, then pause emits silence fences.
  const size_t bytes=fills==6?sizeof(int32_t):4*sizeof(int32_t);
  f.speaker.play(reinterpret_cast<const uint8_t*>(pcm),bytes);
 };
 fake_i2s::on_write=[&]{
  ++buffers;f.speaker.loop();
  if(buffers==7){assert(f.audio.request_volume_percent(50));f.audio.loop();
   auto level=f.speaker.fenced_volume();assert(f.audio.request_volume_percent(50));assert(f.speaker.fenced_volume()==level);
   assert(f.audio.request_volume_percent(10));f.audio.loop();f.speaker.set_pause_state(true);
  }
  f.audio.loop();
  if(buffers>6&&!f.audio.ready())assert(!f.enabled());
  if(buffers>12&&f.audio.ready()&&!resumed){assert(f.audio.volume_percent()==10);resumed=true;resumed_after=fake_i2s::transmitted.size();f.speaker.set_pause_state(false);}
  if(buffers==26){assert(f.audio.request_volume_percent(40));f.audio.loop();f.audio.stop();}
 };
 f.run();assert(resumed&&!f.audio.playing());
 const int32_t factor=esp_audio_libs::gain::db_to_q31(-49*(1-volume_plan(10).software_level));
 const int32_t ceiling=((static_cast<int64_t>(pcm[0])*factor/(int64_t(1)<<31))>>16)+1;
 for(size_t i=resumed_after;i<fake_i2s::transmitted.size();++i)
  for(size_t j=0;j<fake_i2s::transmitted[i].size();j+=2){int16_t sample{};
   std::memcpy(&sample,fake_i2s::transmitted[i].data()+j,2);assert(sample<=ceiling);}
f.audio.loop();f.speaker.loop();f.audio.loop();
 assert(f.audio.volume_percent()==40);assert(f.playback.starts==1); // No recording replay on Stop.
 auto stale=f.speaker.request_volume_with_fence(.5f);f.speaker.start();f.speaker.loop();
 assert(f.speaker.volume_fence_status(stale)==VolumeFenceStatus::CANCELLED);
 fake_i2s::on_write=[&]{f.speaker.stop();};
 try{fake_i2s::task(fake_i2s::task_arg);}catch(const fake_i2s::Exit&){}f.speaker.loop();
 auto idle=f.speaker.request_volume_with_fence(.5f);assert(f.speaker.volume_fence_status(idle)==VolumeFenceStatus::COMPLETE);
}
static void superseded_deadline() {
 Fixture f;f.apply(100);unsigned writes=0;
 fake_i2s::on_write=[&]{
  ++writes;f.speaker.loop();
  if(writes==2){assert(f.audio.request_volume_percent(20));f.audio.loop();}
  if(writes==3){fake_i2s::now+=1500000;assert(f.audio.request_volume_percent(30));f.audio.loop();}
  if(writes==4){fake_i2s::now+=600000;assert(f.audio.request_volume_percent(30));
   f.audio.loop();assert(!f.audio.is_failed()&&!f.audio.ready());}
  f.audio.loop();if(writes==20)f.speaker.stop();
 };
 f.run();assert(f.audio.volume_percent()==30&&!f.audio.is_failed());
}
static void dma_faults_and_deadline() {
 for(unsigned fault=0;fault<7;++fault){
  Fixture f;f.apply(100);unsigned writes=0;
  fake_i2s::on_write=[&]{
   ++writes;f.speaker.loop();
   if(writes==2){assert(f.audio.request_volume_percent(30));f.audio.loop();}
   if(writes==3){if(fault==0)fake_i2s::partial_write=true;if(fault==1)fake_i2s::drop_events=true;
    if(fault==2){fake_i2s::now+=2000000;assert(f.audio.request_volume_percent(30));}if(fault==3)f.audio.stop();
    if(fault==5){for(auto q:fake_i2s::queues)if(q->width==sizeof(i2s_audio::VolumeDmaRecord))q->entries.clear();
      for(unsigned i=0;i<6;++i)fake_i2s::callback(nullptr,nullptr,fake_i2s::context);}
    if(fault==6){fake_i2s::defer_isr_bits=true;for(unsigned i=0;i<12;++i)fake_i2s::callback(nullptr,nullptr,fake_i2s::context);
     assert(!f.speaker.volume_fence_healthy());}}
   f.audio.loop();if(writes>20)f.speaker.stop();
  };
  if(fault==0)fake_i2s::on_fill=[&]{f.speaker.loop();const int16_t pcm[4]={20000,20000,20000,20000};
   f.speaker.play(reinterpret_cast<const uint8_t*>(pcm),sizeof(pcm));};
  if(fault==4)fake_i2s::allocation_failure=true;
  f.run();f.audio.loop();
  if(fault==3){f.audio.loop();assert(f.audio.ready()&&f.playback.starts==0);}
  else {assert(f.audio.is_failed()&&!f.audio.ready()&&!f.enabled());assert(f.speaker.fenced_volume()==0);}
 }
 for(unsigned failure=0;failure<7;++failure){
  Fixture f;f.apply(100);
  fake_i2s::ring_allocation_failure=failure==0;fake_i2s::source_allocation_failure=failure==1;
  fake_i2s::task_creation_failure=failure==2;fake_i2s::preload_partial=failure==3;
  fake_i2s::enable_failure=failure==4;fake_i2s::channel_failure=failure==5;
  if(failure==6){f.speaker.start();f.speaker.loop();fake_i2s::allocation_failure=true;
   try{fake_i2s::task(fake_i2s::task_arg);}catch(const fake_i2s::Exit&){}f.speaker.loop();}
  else f.run();
  f.audio.loop();assert(f.audio.is_failed()&&!f.enabled());
 }
 Fixture callback;callback.apply(100);fake_i2s::callback_failure=true;callback.run();callback.audio.loop();assert(callback.audio.is_failed());
 Fixture layout;layout.apply(100);fake_i2s::invalid_dma_layout=true;layout.run();layout.audio.loop();assert(layout.audio.is_failed());
 Fixture pending;pending.apply(56);pending.speaker.start();pending.speaker.stop();pending.speaker.loop();
 assert(!fake_i2s::task);auto cleaned=pending.speaker.request_volume_with_fence(.5f);
 assert(pending.speaker.volume_fence_status(cleaned)==VolumeFenceStatus::COMPLETE);
 Fixture late;late.apply(100);fake_i2s::on_write=[&]{late.speaker.stop();};
 fake_i2s::on_disable=[&]{fake_i2s::defer_isr_bits=true;for(unsigned i=0;i<12;++i)fake_i2s::callback(nullptr,nullptr,fake_i2s::context);};
 late.run();late.audio.loop();assert(late.audio.is_failed()&&!late.enabled());
 Fixture busy;busy.apply(100);busy.bus.lock();busy.run();busy.audio.loop();assert(busy.audio.is_failed());busy.bus.unlock();
 i2s_audio::VolumeFenceState state;auto token=state.request(.5f,1);assert(state.status(token)==VolumeFenceStatus::COMPLETE);
 state.begin_session();assert(state.status(token)==VolumeFenceStatus::CANCELLED);
 auto current=state.request(.2f,2);state.complete(token);assert(state.status(current)==VolumeFenceStatus::PENDING);
 state.end_session();assert(state.status(current)==VolumeFenceStatus::CANCELLED);
 i2s_audio::VolumeFenceTestAccess::exhaust(state);assert(!state.request(1,INT32_MAX).generation&&state.failed());
 i2s_audio::VolumeFenceState session;i2s_audio::VolumeFenceTestAccess::exhaust_session(session);assert(!session.begin_session()&&session.failed());
}
int main() { curves(); storage(); interrupted_saves(); isolated_storage();amplifier_control();
 adapter_idle_and_faults();streaming_handoff();dma_fence();rapid_pause_stop_restart();superseded_deadline();dma_faults_and_deadline(); }
