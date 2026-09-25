#include "openathan/scheduler.h"
#include "openathan/scheduler_state.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <string>

#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::abort(); } } while (0)
using namespace openathan;
static int64_t epoch(CivilDate date, int hour = 0, int minute = 0, int second = 0) {
  return int64_t(day_number(date)) * 86400 + hour*3600 + minute*60 + second;
}
struct FakeClock : Clock {
  int64_t utc{epoch({2026, 4, 1})};
  uint64_t mono{1000};
  bool valid{true};
  uint16_t subsecond{};
  std::function<int(int64_t)> offset = [](int64_t) { return 0; };
  ClockSample read() override { return {valid, utc, mono, civil_date(static_cast<int32_t>((utc + offset(utc))/86400)), subsecond}; }
  void step(unsigned seconds) { utc += seconds; mono += seconds * 1000ULL; }
};
struct FakeCalculator : DayCalculator {
  std::array<int, 6> hours{5, 6, 12, 15, 18, 20};
  int missing{-1};
  bool fail{false};
  bool calculate(const Settings &s, CivilDate date, PrayerDay &out) override {
    if (fail) return false;
    for (size_t i = 0; i < 6; ++i)
      out[i] = int(i) == missing ? std::nullopt : std::optional<int64_t>(epoch(date, hours[i]) + s.offsets[i]*60);
    return true;
  }
};
struct FakeStore : StateStore {
  std::optional<DurableState> saved;
  bool load_fails{false}, save_fails{false};
  int writes{};
  std::vector<std::string> *operations{};
  LoadResult load(DurableState &out) override {
    if (load_fails) return LoadResult::ERROR;
    if (!saved) return LoadResult::EMPTY;
    out = *saved;
    return LoadResult::LOADED;
  }
  bool save(const DurableState &state) override {
    ++writes;
    if (operations) operations->push_back("save");
    if (save_fails) return false;
    saved = state;
    return true;
  }
};
struct FakePlayback : Playback {
  bool available{true}, active{false}, accepts{true};
  int stops{};
  std::vector<Track> starts;
  std::vector<std::string> *operations{};
  bool ready() const override { return available; }
  bool playing() const override { return active; }
  bool start(Track track) override {
    if (operations) operations->push_back("play");
    if (!accepts) return false;
    stop(); active = true; starts.push_back(track); return true;
  }
  void stop() override { ++stops; active = false; }
};
struct Fixture {
  FakeClock clock;
  FakeCalculator calculator;
  FakeStore store;
  FakePlayback audio;
  Settings settings;
  Scheduler scheduler{clock, calculator, store, audio};
  Fixture() { CHECK(scheduler.begin(settings)); }
  void at(int64_t time) { clock.utc = time; scheduler.tick(); }
  void due(int hour) { at(epoch({2026,4,1},hour)-1); clock.step(1); scheduler.tick(); }
};

static void normal_and_priority() {
  Fixture f;
  std::vector<std::string> operations;
  f.store.operations = &operations; f.audio.operations = &operations;
  f.at(epoch({2026,4,1},5)-1);
  operations.clear(); f.audio.active = true;
  f.clock.step(1); f.scheduler.tick();
  CHECK((operations == std::vector<std::string>{"save", "play"}));
  CHECK(f.audio.starts.size() == 1 && f.audio.starts[0] == Track::FAJR && f.audio.stops == 1);
  f.scheduler.tick(); f.scheduler.stop(); f.clock.step(1); f.scheduler.tick();
  CHECK(f.audio.starts.size() == 1 && !f.audio.active);
  for (int hour : {12,15,18,20}) f.due(hour);
  CHECK(f.audio.starts.size() == 5);
  for (size_t i = 1; i < 5; ++i) CHECK(f.audio.starts[i] == Track::NORMAL);
  const int writes = f.store.writes;
  f.clock.step(1); f.scheduler.tick();
  CHECK(f.store.writes == writes);
}
static void restarts_and_watermarks() {
  Fixture f;
  f.at(epoch({2026,4,1},5)-1);
  Scheduler before(f.clock,f.calculator,f.store,f.audio);
  CHECK(before.begin(f.settings)); before.tick(); f.clock.step(1); before.tick();
  CHECK(f.audio.starts.size() == 1);
  Scheduler after(f.clock,f.calculator,f.store,f.audio);
  CHECK(after.begin(f.settings)); after.tick(); f.clock.step(1); after.tick();
  CHECK(f.audio.starts.size() == 1);
  f.clock.utc = epoch({2026,3,1},5)-1; after.tick(); f.clock.step(1); after.tick();
  CHECK(f.audio.starts.size() == 1);
  f.clock.utc = epoch({2026,4,1},5); after.tick();
  auto changed = f.settings; changed.offsets[0] = 1;
  CHECK(after.configure(changed)); after.tick();
  for (int i = 0; i < 60; ++i) { f.clock.step(1); after.tick(); }
  CHECK(f.audio.starts.size() == 1);
  Fixture boot_at_due;
  boot_at_due.at(epoch({2026,4,1},5));
  CHECK(boot_at_due.audio.starts.empty());
}
static void clock_edges() {
  Fixture f;
  f.clock.valid = false; f.scheduler.tick();
  CHECK(!f.scheduler.status().clock_ready && f.store.writes == 0 && !f.scheduler.skip_next());
  f.clock.valid = true; f.at(epoch({2026,4,1},5)+1);
  CHECK(f.audio.starts.empty());
  Fixture jitter;
  jitter.at(epoch({2026,4,1},5)-1); jitter.clock.step(3); jitter.scheduler.tick();
  CHECK(jitter.audio.starts.size() == 1);
  Fixture too_late;
  too_late.clock.subsecond=1; too_late.at(epoch({2026,4,1},5)-1);
  too_late.clock.step(3); too_late.scheduler.tick();
  CHECK(too_late.audio.starts.empty()); // 2001ms late, beyond the grace interval
  Fixture stalled;
  stalled.at(epoch({2026,4,1},5)-1); stalled.clock.step(4); stalled.scheduler.tick();
  CHECK(stalled.audio.starts.empty());
  Fixture corrected;
  corrected.at(epoch({2026,4,1},5)-10);
  corrected.clock.utc += 20; corrected.clock.mono += 1000; corrected.scheduler.tick();
  CHECK(corrected.audio.starts.empty());
  corrected.clock.utc -= 11; corrected.scheduler.tick(); corrected.clock.step(1); corrected.scheduler.tick();
  CHECK(corrected.audio.starts.empty());
  Fixture invalidated;
  Fixture small_step;
  small_step.at(epoch({2026,4,1},5)-1);
  small_step.clock.utc+=2; small_step.clock.mono+=1000; small_step.scheduler.tick();
  CHECK(small_step.audio.starts.empty()); // one-second wall-clock correction
  Fixture fractional;
  fractional.clock.subsecond=900; fractional.at(epoch({2026,4,1},5)-1);
  fractional.clock.utc+=1; fractional.clock.subsecond=100; fractional.clock.mono+=200;
  fractional.scheduler.tick();
  CHECK(fractional.audio.starts.size()==1); // normal subsecond boundary, no correction
  invalidated.at(epoch({2026,4,1},5)-1); invalidated.clock.valid=false; invalidated.scheduler.tick();
  invalidated.clock.step(2); invalidated.clock.valid=true; invalidated.scheduler.tick();
  CHECK(invalidated.audio.starts.empty());
}
static void skips() {
  Fixture f;
  f.at(epoch({2026,4,1},5)-2);
  CHECK(f.scheduler.skip_next());
  auto target = f.scheduler.status().skip;
  CHECK(target && target->prayer == Prayer::FAJR);
  const int writes = f.store.writes;
  CHECK(f.scheduler.skip_next() && f.store.writes == writes);
  Scheduler restored(f.clock,f.calculator,f.store,f.audio);
  CHECK(restored.begin(f.settings)); restored.tick();
  CHECK(restored.status().skip == target);
  f.clock.step(2); restored.tick();
  CHECK(f.audio.starts.empty() && !restored.status().skip);
  CHECK(restored.skip_next());
  CHECK(restored.status().skip->prayer == Prayer::DHUHR);
  CHECK(restored.cancel_skip());
  f.clock.utc = epoch({2026,4,1},12)-1; restored.tick(); f.clock.step(1); restored.tick();
  CHECK(f.audio.starts.size() == 1);
  Fixture disabled;
  disabled.at(epoch({2026,4,1},5)-1); CHECK(disabled.scheduler.skip_next());
  disabled.settings.enabled[0] = false; CHECK(disabled.scheduler.configure(disabled.settings)); disabled.scheduler.tick();
  CHECK(!disabled.scheduler.status().skip && disabled.scheduler.status().next->key.prayer == Prayer::DHUHR);
  Fixture backwards;
  backwards.at(epoch({2026,4,1},5)-1); CHECK(backwards.scheduler.skip_next());
  const auto original = backwards.scheduler.status().skip;
  backwards.at(epoch({2026,3,1}));
  CHECK(backwards.scheduler.status().skip == original);
  backwards.at(epoch({2026,4,1},5)-1); backwards.clock.step(1); backwards.scheduler.tick();
  CHECK(backwards.audio.starts.empty() && !backwards.scheduler.status().skip);
}
static void failures() {
  Fixture f;
  f.at(epoch({2026,4,1},5)-1); f.store.save_fails = true; f.clock.step(1); f.scheduler.tick();
  CHECK(f.audio.starts.empty() && f.scheduler.status().fault == Fault::STORAGE);
  f.store.save_fails = false; f.clock.step(1); f.scheduler.tick();
  CHECK(f.audio.starts.empty() && !f.scheduler.status().automatic_ready);
  Fixture skip_fail;
  skip_fail.at(epoch({2026,4,1},5)-1); skip_fail.store.save_fails = true;
  CHECK(!skip_fail.scheduler.skip_next() && !skip_fail.scheduler.status().skip);
  FakeClock clock; FakeCalculator calc; FakeStore corrupt; FakePlayback audio;
  corrupt.load_fails = true;
  Scheduler unreadable(clock,calc,corrupt,audio);
  CHECK(!unreadable.begin({})); unreadable.tick();
  CHECK(unreadable.status().fault == Fault::STORAGE && corrupt.writes == 0);
  Fixture unavailable;
  unavailable.audio.available = false; unavailable.due(5);
  CHECK(unavailable.audio.starts.empty() && unavailable.scheduler.status().fault == Fault::AUDIO_UNAVAILABLE);
  unavailable.audio.available = true; unavailable.clock.step(1); unavailable.scheduler.tick();
  CHECK(unavailable.audio.starts.empty());
  Fixture rejected;
  rejected.audio.accepts = false; rejected.due(5);
  CHECK(rejected.scheduler.status().fault == Fault::PLAYBACK_REJECTED);
  rejected.audio.accepts = true; rejected.clock.step(1); rejected.scheduler.tick();
  CHECK(rejected.audio.starts.empty());
}
static void settings_and_missing_events() {
  Fixture f;
  f.settings.offsets[0]=121; CHECK(!f.scheduler.configure(f.settings));
  f.settings.offsets[0]=120; CHECK(f.scheduler.configure(f.settings));
  f.scheduler.tick(); CHECK(f.scheduler.status().fault == Fault::INVALID_SCHEDULE);
  f.settings.offsets[0]=60; CHECK(f.scheduler.configure(f.settings));
  f.scheduler.tick(); CHECK(f.scheduler.status().fault == Fault::INVALID_SCHEDULE);
  f.settings.offsets[0]=0; f.settings.latitude=std::numeric_limits<double>::quiet_NaN();
  CHECK(!f.scheduler.configure(f.settings));
  Fixture disabled; disabled.settings.enabled[0]=false; CHECK(disabled.scheduler.configure(disabled.settings));
  disabled.due(5); CHECK(disabled.audio.starts.empty());
  Fixture missing; missing.calculator.missing=0; missing.due(5);
  CHECK(missing.audio.starts.empty() && missing.scheduler.status().next->key.prayer == Prayer::DHUHR);
  Fixture broken; broken.calculator.fail=true; broken.scheduler.tick();
  CHECK(broken.scheduler.status().fault == Fault::INVALID_SCHEDULE && !broken.scheduler.status().next);
}
static void calendar_and_dst() {
  for (CivilDate day : {CivilDate{2026,4,30}, CivilDate{2026,12,31}}) {
    Fixture f;
    f.calculator.hours[5]=23; f.settings.offsets[5]=90; CHECK(f.scheduler.configure(f.settings));
    f.at(epoch(day,23,59,59));
    for (int i=0;i<=1800;++i) { f.clock.step(1); f.scheduler.tick(); }
    CHECK(f.audio.starts.size()==1 && f.audio.starts[0]==Track::NORMAL);
    CHECK(f.store.saved->consumed_through[4] == day_number(day));
  }
  Fixture early_fajr;
  early_fajr.calculator.hours[0]=1; early_fajr.settings.offsets[0]=-120;
  CHECK(early_fajr.scheduler.configure(early_fajr.settings));
  early_fajr.at(epoch({2026,4,30},23)-1); early_fajr.clock.step(1); early_fajr.scheduler.tick();
  CHECK(early_fajr.audio.starts.size()==1 && early_fajr.audio.starts[0]==Track::FAJR);
  CHECK(early_fajr.store.saved->consumed_through[0] == day_number({2026,5,1}));
  for (bool spring : {true,false}) {
    Fixture f;
    CivilDate day = spring ? CivilDate{2026,3,8} : CivilDate{2026,11,1};
    const int64_t transition = epoch(day,spring ? 7 : 6);
    f.calculator.hours[0]=spring ? 7 : 6; f.calculator.hours[1]=8;
    f.clock.offset = [=](int64_t now) { return (spring ? (now < transition ? -5 : -4) : (now < transition ? -4 : -5))*3600; };
    f.at(transition-1); f.clock.step(1); f.scheduler.tick();
    CHECK(f.audio.starts.size()==1);
    for(int i=0;i<3600;++i) { f.clock.step(1); f.scheduler.tick(); }
    CHECK(f.audio.starts.size()==1);
  }
}
static void real_calculation_integration() {
  FakeClock clock;
  clock.utc=epoch({2015,7,12},8,41,59);
  clock.offset=[](int64_t) { return -4*3600; };
  DayCalculator calculator;
  FakeStore store;
  FakePlayback audio;
  Settings settings;
  settings.latitude=35.775; settings.longitude=-78.6336;
  settings.method=Method::NORTH_AMERICA; settings.hanafi=true;
  Scheduler scheduler(clock,calculator,store,audio);
  CHECK(scheduler.begin(settings)); scheduler.tick();
  CHECK(scheduler.status().next && scheduler.status().next->utc==epoch({2015,7,12},8,42));
  clock.step(1); scheduler.tick();
  CHECK(audio.starts.size()==1 && audio.starts[0]==Track::FAJR);
  CHECK(store.saved->consumed_through[0]==day_number({2015,7,12}));
}
static void state_integrity() {
  DurableState state;
  state.consumed_through[0] = day_number({2026,4,1});
  state.skip = EventKey{day_number({2026,4,2}),Prayer::FAJR};
  auto record=encode_state(state);
  DurableState restored;
  CHECK(decode_state(record,restored) && restored==state);
  for(size_t i=0;i<record.size();++i) { auto corrupt=record; corrupt[i]^=1; CHECK(!decode_state(corrupt,restored)); }
  state.skip->day=day_number({2026,4,1});
  CHECK(!decode_state(encode_state(state),restored));
  state.skip.reset(); state.consumed_through[0]=999999;
  CHECK(!decode_state(encode_state(state),restored));
}
int main() {
  normal_and_priority(); restarts_and_watermarks(); clock_edges(); skips(); failures();
  settings_and_missing_events(); calendar_and_dst(); state_integrity(); real_calculation_integration();
  std::puts("Scheduler clock, state, skip, priority, failure and calendar scenarios passed");
}
