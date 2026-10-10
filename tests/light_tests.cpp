#include "openathan/lights.h"
#include "nvs_settings_store.h"
#include "nvs_memory.h"
#include <cstdio>
#include <cstdlib>
using namespace openathan;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); std::abort(); } } while (0)
struct Calculator : DayCalculator {
  bool fail{}, empty{};
  unsigned overlap{};
  bool calculate(const Settings &s, CivilDate date, PrayerDay &out) override {
    out = {};
    if (fail) return false;
    if (empty) return true;
    const int64_t day = int64_t(day_number(date)) * 86400;
    const int hours[]{5, 6, 12, 15, 18, 20};
    for (unsigned i = 0; i < 6; ++i) out[i] = day + hours[i] * 3600 + s.offsets[i] * 60;
    if (overlap) out[5] = day + 29*3600 + (overlap-1)*1800; // Shared or late Isha, before sunrise.
    return true;
  }
};
void policy() {
  LightSettings settings;
  LightInputs i{false,false,true,true,false,10000,8199};
  CHECK(light_mode(settings,i) == LightMode::GREEN);
  i.utc=8200; CHECK(light_mode(settings,i) == LightMode::ORANGE);
  i.utc=9399; CHECK(light_mode(settings,i) == LightMode::ORANGE);
  i.utc=9400; CHECK(light_mode(settings,i) == LightMode::RED);
  i.utc=10000; CHECK(light_mode(settings,i) == LightMode::OFF);
  i.playing=true; CHECK(light_mode(settings,i) == LightMode::PLAYING);
  i.clock_ready=false; CHECK(light_mode(settings,i) == LightMode::WAITING);
  i.fault=true; CHECK(light_mode(settings,i) == LightMode::FAULT);
  i.maintenance=true; CHECK(light_mode(settings,i) == LightMode::OFF);
  i.maintenance=false; settings.enabled=false; CHECK(light_mode(settings,i) == LightMode::OFF);
  settings.enabled=true; settings.brightness_percent=0; CHECK(light_mode(settings,i) == LightMode::OFF);
  for (auto mode : {LightMode::PLAYING,LightMode::WAITING,LightMode::FAULT}) {
    CHECK(light_frame(mode,20,0)==light_frame(mode,20,3000));
    CHECK(light_frame(mode,20,1500).brightness==20);
    CHECK(light_frame(mode,20,0).brightness==2);
    for (unsigned ms=0; ms<6000; ms+=50) {
      CHECK(light_frame(mode,20,ms).brightness<=20);
      CHECK(light_frame(mode,1,ms+17).brightness==1);
      CHECK(light_frame(mode,0,ms).brightness==0);
    }
  }
  CHECK(light_frame(LightMode::OFF,100,1500)==LightFrame{});
}
void timetable() {
  Calculator calculator; LightSchedule schedule; Settings s;
  s.enabled.fill(false);
  const CivilDate date{2026,9,29}; const int64_t day=int64_t(day_number(date))*86400;
  CHECK(schedule.rebuild(calculator,s,date));
  CHECK(schedule.next(day+5*3600-1)==day+5*3600);
  const auto muted = schedule.next_event(day+5*3600-1);
  CHECK(muted && muted->key == EventKey({day_number(date),Prayer::FAJR}) && !muted->enabled);
  CHECK(muted->utc == *schedule.next(day+5*3600-1));
  CHECK(schedule.next(day+5*3600)==day+12*3600); // No sunrise or audio eligibility filter.
  CHECK(schedule.next(day+23*3600)==day+29*3600);
  CHECK(schedule.next(day+4*3600)==day+5*3600); // Backward correction.
  s.offsets[0]=30; CHECK(schedule.rebuild(calculator,s,date));
  CHECK(schedule.next(day+5*3600)==day+5*3600+1800);
  calculator.overlap=1; s.offsets[0]=0; CHECK(schedule.rebuild(calculator,s,date));
  CHECK(schedule.next(day+28*3600)==day+29*3600);
  auto shared = schedule.next_event(day+28*3600);
  CHECK(shared && shared->key == EventKey({day_number(date)+1,Prayer::FAJR}));
  CHECK(shared->shared_with == EventKey({day_number(date),Prayer::ISHA}) && !shared->enabled);
  s.enabled[4]=true; CHECK(schedule.rebuild(calculator,s,date));
  shared = schedule.next_event(day+28*3600);
  CHECK(shared->key == EventKey({day_number(date),Prayer::ISHA}) && shared->enabled);
  CHECK(shared->shared_with == EventKey({day_number(date)+1,Prayer::FAJR}));
  s.enabled[0]=true; CHECK(schedule.rebuild(calculator,s,date));
  shared = schedule.next_event(day+28*3600);
  CHECK(shared->key.prayer == Prayer::FAJR && shared->enabled);
  // Guard days retain both identities at either edge without adding targets.
  shared = schedule.next_event(day-86400+5*3600-1);
  CHECK(shared->key == EventKey({day_number(date)-1,Prayer::FAJR}));
  CHECK(shared->shared_with == EventKey({day_number(date)-2,Prayer::ISHA}));
  shared = schedule.next_event(day+86400+19*3600);
  CHECK(shared->key == EventKey({day_number(date)+2,Prayer::FAJR}));
  CHECK(shared->shared_with == EventKey({day_number(date)+1,Prayer::ISHA}));
  CHECK(schedule.next(day+29*3600)==day+36*3600);
  calculator.overlap=2; CHECK(schedule.rebuild(calculator,s,date));
  const auto suppressed = schedule.next_event(day+29*3600);
  CHECK(suppressed && suppressed->key.prayer == Prayer::ISHA && !suppressed->enabled);
  CHECK(suppressed->utc == *schedule.next(day+29*3600));
  const auto edge = schedule.next_event(day+86400+19*3600);
  CHECK(edge && edge->key.prayer == Prayer::ISHA && !edge->enabled);
  calculator.empty=true; CHECK(schedule.rebuild(calculator,s,date)); CHECK(!schedule.next(day));
  calculator.fail=true; CHECK(!schedule.rebuild(calculator,s,date)); CHECK(!schedule.next(day));
  CHECK(!schedule.next_event(day));
}
void records() {
  const SavedLights original{42,{false,37}}; const auto encoded=encode_lights(original);
  SavedLights out; CHECK(decode_lights(encoded,out) && out==original);
  for (size_t byte=0; byte<encoded.size(); ++byte) for (unsigned bit=0; bit<8; ++bit) {
    auto bad=encoded; bad[byte]^=1U<<bit; CHECK(!decode_lights(bad,out));
  }
  CHECK(!decode_lights(encode_lights({0,{}}),out));
  CHECK(!decode_lights(encode_lights({1,{true,101}}),out));
}
void durability() {
  using esphome::openathan_component::NvsLightStore;
  using esphome::openathan_storage::PRAYER;
  nvs_test::reset();
  const auto key=std::make_pair(std::string(PRAYER),std::string("lights"));
  nvs_test::committed[{PRAYER,"settings"}]={9,8,7};
  nvs_test::committed[{PRAYER,"scheduler"}]={6,5,4};
  const auto existing=nvs_test::committed;
  NvsLightStore store; LightPreferences service(store); service.begin();
  CHECK(service.saved()->revision==1 && service.saved()->value.brightness_percent==20 && nvs_test::writes==0);
  CHECK(service.update({true,20},1)==LightSaveResult::UNCHANGED && nvs_test::writes==0);
  CHECK(service.update({false,42},0)==LightSaveResult::CONFLICT);
  CHECK(service.update({false,101},1)==LightSaveResult::INVALID);
  CHECK(service.update({false,42},1)==LightSaveResult::SAVED);
  const auto baseline=nvs_test::committed;
  for (const auto &[k,v]:existing) CHECK(nvs_test::committed.at(k)==v);
  nvs_test::fail_commit=true;
  CHECK(service.update({true,70},2)==LightSaveResult::STORAGE);
  CHECK(service.saved()->value==LightSettings({false,42}) && !service.writable());
  nvs_test::fail_commit=false; nvs_test::power_cycle();
  NvsLightStore reboot; LightPreferences restored(reboot); restored.begin();
  CHECK(restored.saved()==service.saved());
  // Every interrupted set/commit can restore only the old or complete new value.
  for (bool early : {false,true}) for (bool commit : {false,true}) {
    nvs_test::committed=baseline; nvs_test::power_cycle();
    nvs_test::early_persist=early;
    NvsLightStore interrupted; LightPreferences writer(interrupted); writer.begin();
    nvs_test::cut_after_write=!commit; nvs_test::cut_after_commit=commit;
    try { writer.update({true,77},2); CHECK(false); } catch (const std::runtime_error &) {}
    nvs_test::cut_after_write=nvs_test::cut_after_commit=false; nvs_test::power_cycle();
    NvsLightStore after; LightPreferences reader(after); reader.begin(); CHECK(reader.saved());
    CHECK(reader.saved()->value==LightSettings({false,42}) || reader.saved()->value==LightSettings({true,77}));
    for (const auto &[k,v]:existing) CHECK(nvs_test::committed.at(k)==v);
  }
  nvs_test::committed[key][0]^=1;
  NvsLightStore corrupt; LightPreferences disabled(corrupt); disabled.begin();
  CHECK(!disabled.saved() && !disabled.writable());
}
int main() { policy(); timetable(); records(); durability(); }
