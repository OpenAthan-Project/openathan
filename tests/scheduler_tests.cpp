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
static void failed_rebuild_recovery() {
  // A failure can happen before calculation or after a partial event list.
  // Returning to the last good date must rebuild without reconfiguration.
  for (bool calculation_failure : {true, false}) {
    Fixture f;
    f.at(epoch({2026,9,25},12));
    CHECK(f.scheduler.status().automatic_ready && f.scheduler.status().next);
    f.calculator.fail = calculation_failure;
    if (!calculation_failure) f.calculator.hours[3] = 12;
    f.at(epoch({2026,6,21},12));
    CHECK(f.scheduler.status().fault == Fault::INVALID_SCHEDULE);
    CHECK(!f.scheduler.status().automatic_ready && !f.scheduler.status().next);
    f.calculator.fail = false;
    f.calculator.hours[3] = 15;
    f.at(epoch({2026,9,25},12));
    CHECK(f.scheduler.status().fault == Fault::NONE && f.scheduler.status().automatic_ready);
    CHECK(f.scheduler.status().next && f.scheduler.status().next->key.prayer == Prayer::ASR);
    CHECK(f.audio.starts.empty());
    f.at(epoch({2026,9,25},15)-1);
    f.clock.step(1); f.scheduler.tick();
    CHECK(f.audio.starts == std::vector<Track>{Track::NORMAL});
    f.clock.step(1); f.scheduler.tick();
    CHECK(f.audio.starts.size() == 1);
  }
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
static void high_latitude_daytime_playback() {
  struct City { double latitude, longitude; int utc_offset; };
  for (auto city : {City{51.5074,-0.1278,3600}, City{59.3293,18.0686,7200},
                    City{59.9139,10.7522,7200}, City{60.1699,24.9384,10800}}) {
    for (auto rule : {HighLatitudeRule::AUTO, HighLatitudeRule::MIDDLE_OF_NIGHT}) {
      for (CivilDate date : {CivilDate{2026,6,1}, CivilDate{2026,6,21}, CivilDate{2026,7,1}}) {
        FakeClock clock;
        clock.offset = [=](int64_t) { return city.utc_offset; };
        DayCalculator calculator;
        FakeStore store;
        FakePlayback audio;
        Settings settings;
        settings.latitude = city.latitude; settings.longitude = city.longitude;
        settings.high_latitude = rule;
        PrayerDay times;
        CHECK(calculator.calculate(settings,date,times) && times[2]);
        Scheduler scheduler(clock,calculator,store,audio);
        CHECK(scheduler.begin(settings));
        for (size_t i : {2, 3, 4}) {
          clock.utc = *times[i]-1; scheduler.tick();
          CHECK(scheduler.status().automatic_ready && scheduler.status().next);
          CHECK(scheduler.status().fault == Fault::NONE && scheduler.status().next->utc == *times[i]);
          clock.step(1); scheduler.tick();
          CHECK(audio.starts.size() == i-1 && audio.starts.back() == Track::NORMAL);
          clock.step(1); scheduler.tick();
          CHECK(audio.starts.size() == i-1);
        }
      }
    }
  }
}
static void shared_playback() {
  const auto today = day_number({2026,4,1});
  for (unsigned enabled = 0; enabled < 4; ++enabled) {
    Fixture f;
    f.calculator.hours[5] = 29; // yesterday's Isha is today's 05:00 Fajr
    f.settings.enabled[0] = enabled & 1; f.settings.enabled[4] = enabled & 2;
    CHECK(f.scheduler.configure(f.settings));
    f.at(epoch({2026,4,1},5)-1);
    auto status = f.scheduler.status();
    CHECK(status.automatic_ready && status.fault == Fault::NONE && status.conflicts.size() == 4);
    const auto &conflict = status.conflicts[1];
    CHECK((conflict.isha == EventKey{today-1, Prayer::ISHA}));
    CHECK((conflict.fajr == EventKey{today, Prayer::FAJR}));
    CHECK(conflict.isha_utc == conflict.fajr_utc && conflict.resolution == ConflictResolution::SHARED_PLAYBACK);
    CHECK(describe_conflict(conflict).find("Isha 2026-03-31 UTC=") != std::string::npos);
    CHECK(describe_conflict(conflict).find("Fajr 2026-04-01 UTC=") != std::string::npos);
    if (enabled) {
      CHECK(status.next && status.next->shared_with);
      CHECK(status.next->key.prayer == (enabled & 1 ? Prayer::FAJR : Prayer::ISHA));
    } else CHECK(status.next->key.prayer == Prayer::DHUHR);
    std::vector<std::string> operations;
    f.store.operations = &operations; f.audio.operations = &operations;
    f.clock.step(1); f.scheduler.tick();
    CHECK(f.audio.starts.size() == (enabled ? 1 : 0));
    CHECK(f.store.saved->consumed_through[0] == today && f.store.saved->consumed_through[4] == today-1);
    if (enabled) {
      CHECK(f.audio.starts[0] == (enabled & 1 ? Track::FAJR : Track::NORMAL));
      CHECK((operations == std::vector<std::string>{"save", "play"}));
    }
    f.scheduler.stop(); f.clock.step(1); f.scheduler.tick();
    Scheduler restarted(f.clock, f.calculator, f.store, f.audio);
    CHECK(restarted.begin(f.settings)); restarted.tick(); f.clock.step(1); restarted.tick();
    CHECK(!f.audio.active && f.audio.starts.size() == (enabled ? 1 : 0));
  }
  Fixture failure;
  failure.calculator.hours[5] = 29;
  failure.at(epoch({2026,4,1},5)-1); failure.store.save_fails = true;
  failure.clock.step(1); failure.scheduler.tick();
  CHECK(failure.audio.starts.empty() && failure.scheduler.status().fault == Fault::STORAGE);
  Fixture unavailable;
  unavailable.calculator.hours[5] = 29; unavailable.audio.available = false;
  unavailable.due(5); unavailable.audio.available = true;
  unavailable.clock.step(1); unavailable.scheduler.tick();
  CHECK(unavailable.audio.starts.empty() && unavailable.store.saved->consumed_through[4] == today-1);
  Fixture already_handled;
  already_handled.calculator.hours[5] = 29;
  DurableState handled;
  handled.consumed_through[4] = today-1;
  already_handled.store.saved = handled;
  CHECK(already_handled.scheduler.begin(already_handled.settings)); already_handled.due(5);
  CHECK(already_handled.audio.starts.empty() && already_handled.store.saved->consumed_through[0] == today);
}
static void recommended_rule_year() {
  struct City { double latitude, longitude; Method method; };
  for (auto city : {City{51.5074,-0.1278,Method::MUSLIM_WORLD_LEAGUE},
                   City{59.3293,18.0686,Method::MUSLIM_WORLD_LEAGUE},
                   City{59.9139,10.7522,Method::MUSLIM_WORLD_LEAGUE},
                   City{60.1699,24.9384,Method::MUSLIM_WORLD_LEAGUE},
                   City{44.3894,-79.6903,Method::NORTH_AMERICA}}) {
    FakeClock clock;
    DayCalculator calculator;
    FakeStore store;
    FakePlayback audio;
    Settings settings;
    settings.latitude = city.latitude; settings.longitude = city.longitude; settings.method = city.method;
    CHECK(settings.high_latitude == HighLatitudeRule::AUTO);
    Scheduler scheduler(clock,calculator,store,audio);
    CHECK(scheduler.begin(settings));
    PrayerDay previous{}, times{};
    CHECK(calculator.calculate(settings,{2025,12,31},previous));
    size_t count = 0;
    for (auto serial=day_number({2026,1,1}); serial<=day_number({2026,12,31}); ++serial) {
      CHECK(calculator.calculate(settings,civil_date(serial),times));
      CHECK(previous[5] && times[0] && *previous[5] < *times[0]);
      clock.utc = *times[2]-1; scheduler.tick();
      CHECK(scheduler.status().automatic_ready && scheduler.status().conflicts.empty());
      CHECK(scheduler.status().next->utc == *times[2]);
      clock.step(1); scheduler.tick();
      CHECK(audio.starts.size() == ++count && audio.starts.back() == Track::NORMAL);
      previous = times;
    }
  }
}
static void shared_skips() {
  const auto today = day_number({2026,4,1});
  for (Prayer key : {Prayer::FAJR, Prayer::ISHA}) {
    Fixture f;
    f.calculator.hours[5] = 29;
    f.at(epoch({2026,4,1},5)-1);
    CHECK(f.scheduler.skip_next());
    const int writes = f.store.writes;
    CHECK(f.scheduler.skip_next() && writes == f.store.writes);
    CHECK(f.scheduler.cancel_skip() && !f.scheduler.status().skip);
    // Load an old-format skip for either identity, even if that member is now
    // disabled. It still silences the enabled member of the shared occurrence.
    f.store.saved->skip = EventKey{key == Prayer::FAJR ? today : today-1, key};
    DurableState restored;
    CHECK(decode_state(encode_state(*f.store.saved), restored) && restored == *f.store.saved);
    f.settings.enabled[static_cast<unsigned>(key)] = false;
    Scheduler restarted(f.clock, f.calculator, f.store, f.audio);
    CHECK(restarted.begin(f.settings)); restarted.tick();
    CHECK(restarted.status().skip == restored.skip);
    f.clock.step(1); restarted.tick();
    CHECK(f.audio.starts.empty() && !restarted.status().skip);
    CHECK(f.store.saved->consumed_through[0] == today && f.store.saved->consumed_through[4] == today-1);

    Fixture split;
    split.calculator.hours[5] = 29;
    split.settings.enabled[0] = key == Prayer::FAJR;
    CHECK(split.scheduler.configure(split.settings));
    split.at(epoch({2026,4,1},5)-1); CHECK(split.scheduler.skip_next());
    CHECK(split.scheduler.status().skip->prayer == key);
    split.settings.enabled[0] = true; split.settings.offsets[0] = 5;
    CHECK(split.scheduler.configure(split.settings)); split.scheduler.tick();
    split.clock.step(1); split.scheduler.tick(); // Isha, now separate
    CHECK(split.audio.starts.size() == (key == Prayer::FAJR ? 1 : 0));
    split.at(epoch({2026,4,1},5,5)-1); split.clock.step(1); split.scheduler.tick();
    CHECK(split.audio.starts.size() == 1 && !split.scheduler.status().skip);
    CHECK(split.audio.starts[0] == (key == Prayer::FAJR ? Track::NORMAL : Track::FAJR));
  }
  Fixture cancel;
  cancel.calculator.hours[5] = 29; cancel.at(epoch({2026,4,1},5)-1);
  CHECK(cancel.scheduler.skip_next() && cancel.scheduler.cancel_skip());
  cancel.clock.step(1); cancel.scheduler.tick(); CHECK(cancel.audio.starts.size() == 1);
}
static void conflicting_boundaries() {
  Fixture invalid;
  invalid.calculator.hours[5] = 30; // Isha also collides with next sunrise
  invalid.scheduler.tick();
  CHECK(invalid.scheduler.status().fault == Fault::INVALID_SCHEDULE && !invalid.scheduler.status().automatic_ready);
  for (bool fajr_enabled : {false, true}) {
    Fixture reversed;
    reversed.calculator.hours[5] = 29; reversed.settings.offsets[5] = 1;
    reversed.settings.enabled[0] = fajr_enabled;
    CHECK(reversed.scheduler.configure(reversed.settings));
    reversed.at(epoch({2026,4,1},5)-1);
    CHECK(reversed.scheduler.status().conflicts[1].resolution == ConflictResolution::ISHA_SUPPRESSED);
    reversed.clock.step(1); reversed.scheduler.tick();
    CHECK(reversed.audio.starts.size() == (fajr_enabled ? 1 : 0));
    CHECK(reversed.scheduler.status().next->key.prayer == Prayer::DHUHR);
    CHECK(reversed.scheduler.skip_next() && reversed.scheduler.status().skip->prayer == Prayer::DHUHR);
    CHECK(reversed.scheduler.cancel_skip());
    for (unsigned i=0; i<60; ++i) { reversed.clock.step(1); reversed.scheduler.tick(); }
    CHECK(reversed.audio.starts.size() == (fajr_enabled ? 1 : 0));
    CHECK(reversed.store.saved->consumed_through[4] == day_number({2026,3,31}));
    for (int hour : {12,15,18}) reversed.due(hour);
    CHECK(reversed.audio.starts.size() == (fajr_enabled ? 4 : 3));
    CHECK(reversed.scheduler.status().automatic_ready && reversed.scheduler.status().fault == Fault::NONE);
  }
  Fixture close;
  close.calculator.hours[5] = 29; close.settings.offsets[5] = -1;
  CHECK(close.scheduler.configure(close.settings)); close.at(epoch({2026,4,1},4,59)-1);
  close.clock.step(1); close.scheduler.tick();
  CHECK(close.audio.starts == std::vector<Track>{Track::NORMAL});
  for (unsigned i=0; i<60; ++i) { close.clock.step(1); close.scheduler.tick(); }
  CHECK((close.audio.starts == std::vector<Track>{Track::NORMAL, Track::FAJR}) && close.audio.stops == 2);
  for (CivilDate date : {CivilDate{2026,5,1}, CivilDate{2027,1,1}}) {
    for (bool skip : {false, true}) {
      Fixture midnight;
      midnight.calculator.hours[0] = 0; midnight.calculator.hours[5] = 24;
      midnight.at(epoch(date)-1);
      if (skip) CHECK(midnight.scheduler.skip_next());
      midnight.clock.step(1); midnight.scheduler.tick();
      CHECK(midnight.audio.starts.size() == (skip ? 0 : 1) && !midnight.scheduler.status().skip);
      if (!skip) CHECK(midnight.audio.starts[0] == Track::FAJR);
      CHECK(midnight.store.saved->consumed_through[0] == day_number(date));
      CHECK(midnight.store.saved->consumed_through[4] == day_number(date)-1);
    }
  }
  // Both guard boundaries must be classified before choosing the next event.
  Fixture guards;
  guards.calculator.hours[5] = 29;
  guards.at(epoch({2026,4,1},12));
  CHECK(guards.scheduler.status().conflicts.front().isha.day == day_number({2026,3,30}));
  CHECK(guards.scheduler.status().conflicts.back().fajr.day == day_number({2026,4,3}));
  guards.calculator.fail = true; guards.at(epoch({2026,4,2},12));
  CHECK(guards.scheduler.status().conflicts.empty() && !guards.scheduler.status().automatic_ready);
  Fixture changed;
  changed.calculator.hours[5] = 29; changed.at(epoch({2026,4,1},5)-1);
  CHECK(!changed.scheduler.status().conflicts.empty());
  changed.calculator.hours[5] = 20;
  CHECK(changed.scheduler.configure(changed.settings)); changed.scheduler.tick();
  CHECK(changed.scheduler.status().conflicts.empty() && changed.scheduler.status().automatic_ready);
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
  failed_rebuild_recovery();
  high_latitude_daytime_playback();
  shared_playback(); shared_skips(); conflicting_boundaries();
  recommended_rule_year();
  std::puts("Scheduler clock, state, skip, priority, failure and calendar scenarios passed");
}
