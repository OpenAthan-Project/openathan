#include "openathan.h"
#include "nvs_memory.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); std::abort(); } } while (0)
using namespace openathan;
static uint64_t mono = 1000;
int64_t esp_timer_get_time() { return mono*1000; }
static int64_t epoch(CivilDate d, unsigned hour = 0) { return int64_t(day_number(d))*86400+hour*3600; }
struct Audio : Playback {
  unsigned volume{70}, requested{70}, requests{}, starts{}, stops{};
  bool active{}, drop{}, available{true};
  bool ready() const override { return available; }
  bool playing() const override { return active; }
  bool start(Track) override { ++starts; active = true; return true; }
  void stop() override { ++stops; active = false; }
  std::optional<unsigned> volume_percent() const override { return volume; }
  bool request_volume_percent(unsigned value) override { ++requests; requested = value; if (!drop) volume = value; return true; }
};
struct Calculator : DayCalculator {
  bool calculate(const Settings &s, CivilDate date, PrayerDay &out) override {
    constexpr unsigned hours[] = {5,6,12,15,18,20};
    for (unsigned i=0; i<6; ++i) out[i] = epoch(date,hours[i])+60*s.offsets[i];
    return true;
  }
};
struct Device : esphome::openathan_component::OpenAthan {
  int64_t utc{epoch({2026,9,25},4)};
  bool valid{true};
  ClockSample read() override {
    CivilDate date;
    const bool known = local_date(utc,date);
    return {valid && known, utc, mono, date, 0};
  }
  void step(unsigned seconds) { utc += seconds; mono += seconds*1000; loop(); update(); }
};
struct Fixture {
  esphome::time::RealTimeClock clock;
  Calculator calculator;
  Audio audio;
  Device device;
  Fixture(bool reset = true) {
    if (reset) nvs_test::reset();
    esphome::time::set_global_tz({});
    device.set_default_timezone("UTC"); device.set_clock(&clock); device.set_playback(&audio);
    device.set_calculator(&calculator);
  }
  void begin() { device.setup(); }
  DeviceSettings value() { return device.settings_service()->saved()->value; }
  SettingsResult save(const DeviceSettings &value) {
    return device.change_settings(value, device.settings_service()->saved()->revision);
  }
};
static void updates_and_replay() {
  Fixture f; f.begin();
  CHECK(f.device.status().automatic_ready);
  const auto first_revision = f.device.settings_service()->saved()->revision;
  auto bad = f.value(); bad.prayer.offsets[0] = 120;
  CHECK(f.save(bad) == SettingsResult::INVALID_SCHEDULE);
  CHECK(f.device.status().automatic_ready && f.device.settings_service()->saved()->revision == first_revision);
  f.device.utc = epoch({2026,9,25},5)-1; f.device.update();
  f.audio.active = true;
  auto volume = f.value(); volume.volume = 35;
  CHECK(f.save(volume) == SettingsResult::SAVED && f.audio.stops == 0);
  f.device.step(1);
  CHECK(f.audio.starts == 1 && f.audio.volume == 35 && f.audio.active);
  auto moved = f.value(); moved.prayer.offsets[0] = 1; moved.prayer.hanafi = true;
  CHECK(f.save(moved) == SettingsResult::SAVED && f.audio.stops == 0);
  for (int i=0;i<65;++i) f.device.step(1);
  CHECK(f.audio.starts == 1);
  // Persisted settings supersede boot defaults without resetting consumption.
  Fixture reboot(false); reboot.device.utc = epoch({2026,9,25},5)-1; reboot.begin();
  CHECK(reboot.value() == moved);
  for (int i=0;i<70;++i) reboot.device.step(1);
  CHECK(reboot.audio.starts == 0);
  // Disabled events are consumed and cannot replay after re-enabling + offset.
  auto disabled = reboot.value(); disabled.prayer.enabled[1] = false;
  CHECK(reboot.save(disabled) == SettingsResult::SAVED);
  reboot.device.utc = epoch({2026,9,25},12)-1; reboot.device.update(); reboot.device.step(1);
  disabled.prayer.enabled[1] = true; disabled.prayer.offsets[2] = 1;
  CHECK(reboot.save(disabled) == SettingsResult::SAVED);
  for (int i=0;i<65;++i) reboot.device.step(1);
  CHECK(reboot.audio.starts == 0);
  reboot.device.utc = epoch({2026,9,25},15)-1; reboot.device.update(); reboot.device.step(1);
  CHECK(reboot.audio.starts == 1);
}
static void volume_and_faults() {
  Fixture f; f.audio.volume = 61; f.begin(); CHECK(f.value().volume == 61);
  f.audio.drop = true; f.audio.active = true;
  auto changed = f.value(); changed.volume = 32;
  CHECK(f.save(changed) == SettingsResult::SAVED);
  CHECK(!f.device.status().automatic_ready && f.audio.active && f.audio.stops == 0);
  for (int i=0;i<20;++i) f.device.step(1);
  CHECK(std::string(f.device.settings_application_status()) == "volume_failed" && f.audio.requests > 1);
  f.audio.drop = false; f.device.step(1); f.device.step(1);
  CHECK(f.device.status().automatic_ready && f.audio.volume == 32);
  nvs_test::fail_commit = true; changed.volume = 88;
  CHECK(f.save(changed) == SettingsResult::STORAGE);
  CHECK(f.value().volume == 32 && f.audio.volume == 32 && f.audio.active);
  CHECK(!f.device.status().automatic_ready);
  nvs_test::fail_commit = false;
  CHECK(f.save(changed) == SettingsResult::STORAGE);
  nvs_test::power_cycle(); Fixture reboot(false); reboot.begin(); CHECK(reboot.value().volume == 32);
  CHECK(std::string(reboot.device.settings_application_status()) != "storage_fault");
}
static void timezones() {
  Fixture f; f.device.valid = false; f.begin();
  auto s = f.value();
  s.timezone = {"America/Toronto",18000,14400,
      {7200,0,DstRuleType::MONTH_WEEK_DAY,3,2,0}, {7200,0,DstRuleType::MONTH_WEEK_DAY,11,1,0}};
  CHECK(f.save(s) == SettingsResult::SAVED && !f.device.status().automatic_ready);
  // Exercise the real pinned ESPHome timezone implementation, not libc TZ.
  CivilDate date;
  CHECK(f.device.local_date(epoch({2026,3,8},4)+59*60,s.timezone,date) && day_number(date)==day_number({2026,3,7}));
  CHECK(f.device.local_date(epoch({2026,3,9},4),s.timezone,date) && day_number(date)==day_number({2026,3,9}));
  CHECK(f.device.local_date(epoch({2026,11,2},4),s.timezone,date) && day_number(date)==day_number({2026,11,1}));
  CHECK(esphome::time::get_global_tz().std_offset_seconds == 0);
  s.timezone = {"Asia/Kolkata",-19800,0,{},{}};
  CHECK(f.save(s) == SettingsResult::SAVED);
  CHECK(f.device.local_date(epoch({2026,9,25},19),date) && day_number(date)==day_number({2026,9,26}));
  s.timezone = {"Australia/Sydney",-36000,-39600,
      {7200,0,DstRuleType::MONTH_WEEK_DAY,10,1,0}, {10800,0,DstRuleType::MONTH_WEEK_DAY,4,1,0}};
  CHECK(f.device.local_date(epoch({2026,1,1},13),s.timezone,date) && day_number(date)==day_number({2026,1,2}));
  CHECK(f.device.local_date(epoch({2026,7,1},13),s.timezone,date) && day_number(date)==day_number({2026,7,1}));
}

static void occurrence_identity() {
  Fixture f;f.begin();
  const auto displayed=f.device.status().next;CHECK(displayed);
  f.device.utc += 86400;f.device.update();
  CHECK(!f.device.skip_occurrence(*displayed));
  CHECK(!f.device.status().skip);
}
static void setup_gate_and_preview() {
  Fixture f; f.device.require_setup(); f.begin();
  CHECK(std::string(f.device.setup_state()) == "incomplete");
  CHECK(!f.device.status().automatic_ready);
  const auto initial_writes = nvs_test::writes;
  f.device.utc = epoch({2026,9,25},5)-1;
  for (int i=0;i<65;++i) f.device.step(1);
  CHECK(f.audio.starts == 0 && nvs_test::writes == initial_writes);
  CHECK(!f.device.skip_next() && !f.device.cancel_skip());
  PrayerDay times; std::vector<Event> events; std::vector<ScheduleConflict> conflicts;
  CHECK(f.device.preview(f.value(), times, events, conflicts));
  CHECK(nvs_test::writes == initial_writes && f.audio.starts == 0);
  auto changed = f.value(); changed.prayer.latitude=44; changed.volume=42;
  CHECK(f.save(changed)==SettingsResult::SAVED);
  Fixture interrupted(false); interrupted.device.require_setup(); interrupted.begin();
  CHECK(!interrupted.device.activated() && interrupted.value()==changed);
  const auto revision=interrupted.device.settings_service()->saved()->revision;
  CHECK(!interrupted.device.finish_setup(revision-1));
  CHECK(interrupted.device.finish_setup(revision));
  CHECK(interrupted.device.activated());
  Fixture reboot(false); reboot.device.require_setup(); reboot.begin();
  CHECK(reboot.device.activated() && reboot.value()==changed);
  // Existing developer settings are adopted without resetting their history.
  Fixture legacy; legacy.begin(); legacy.device.utc=epoch({2026,9,25},5)-1;legacy.device.update();legacy.device.step(1);
  const auto history=nvs_test::committed.at({"openathan","scheduler"});
  Fixture migrated(false);migrated.device.require_setup();migrated.device.utc=legacy.device.utc;migrated.begin();
  CHECK(migrated.device.activated() && nvs_test::committed.at({"openathan","scheduler"})==history);
}
#ifdef OPENATHAN_JSON_TEST
#include "../firmware/esphome/components/openathan_device/local_api.h"
static void local_api() {
  using namespace esphome::openathan_device;
  Fixture f;f.device.require_setup();f.begin();
  const ZoneEntry zones[]={{"UTC",R"({"standard_offset":0,"daylight_offset":0,"start":{"type":0,"time_seconds":0,"day":0,"month":0,"week":0,"day_of_week":0},"end":{"type":0,"time_seconds":0,"day":0,"month":0,"week":0,"day_of_week":0}})"}};
  LocalApi api(&f.device,zones,1);api.set_context("openathan-test.local",true);
  auto call=[&](const char *method,const char *uri,const std::string &body="") {
    ApiExchange exchange;exchange.method=method;exchange.uri=uri;exchange.body=body;
    api.handle(exchange);return exchange;
  };
  auto initial=call("GET","/api/status");CHECK(initial.code==200);
  JsonDocument current;CHECK(!deserializeJson(current,initial.response));
  CHECK(current["setup"]=="incomplete" && current["clock_ready"].as<bool>());
  CHECK(current["schedule"]["state"]=="setup_required");
  JsonDocument request;request["schema"]=1;request["expected_revision"]=1;request["settings"]=current["settings"];
  request["settings"]["latitude"]=44.3894;
  auto body=[&](){std::string value;serializeJson(request,value);return value;};
  const auto writes=nvs_test::writes;
  CHECK(call("POST","/api/preview",body()).code==200 && nvs_test::writes==writes);
  f.device.valid=false;
  CHECK(call("POST","/api/preview",body()).response.find("waiting_for_time")!=std::string::npos);
  auto saved=call("POST","/api/activate",body());CHECK(saved.code==200);
  CHECK(f.device.activated() && !f.device.status().automatic_ready);
  CHECK(call("POST","/api/settings",body()).code==409); // stale write cannot replay
  CHECK(call("POST","/api/stop","{}").code==200);
  request["expected_revision"]=2;
  request["settings"]["timezone"]= "Invented/Zone";
  CHECK(call("POST","/api/settings",body()).code==400);
  request["settings"]["timezone"]="UTC";
  request["settings"]["timezone_rules"]["standard_offset"]=3600;
  CHECK(call("POST","/api/settings",body()).code==200 && f.value().timezone.standard_offset==0);
  request["refresh_timezone"]=true;
  CHECK(call("POST","/api/settings",body()).code==200 && f.value().timezone.standard_offset==0);
  request["surprise"]=true;
  CHECK(call("POST","/api/settings",body()).code==400);request.remove("surprise");
  CHECK(call("POST","/api/settings","{broken").code==400);
  f.device.valid=true;f.device.update();
  auto state=call("GET","/api/status");CHECK(!deserializeJson(current,state.response));
  JsonDocument skip;skip["expected_revision"]=2;skip["occurrence"]=current["next"];
  std::string payload;serializeJson(skip,payload);
  CHECK(call("POST","/api/skip",payload).code==200);
  auto prior=f.value();prior.prayer.offsets[0]=1;CHECK(f.save(prior)==SettingsResult::SAVED);
  CHECK(call("POST","/api/skip",payload).code==409);
  CHECK(call("GET","/api/timezones").response.find("UTC")!=std::string::npos);
  CHECK(call("GET","/not-found").code==404);
}
#endif

#ifdef OPENATHAN_JSON_TEST
static void coordinate_roundtrip() {
  // Exercise wire JSON, not an in-memory JsonDocument copy: ArduinoJson's
  // normal number writer and reader can both change coordinate precision.
  for (const auto &[latitude, longitude] : {
      std::pair{"43.6532123456789", "-79.3832123456789"},
      std::pair{"43.65", "179.99999999999997"},
      std::pair{"43.653212345678909", "-79.383212345678913"},
      std::pair{"-89.99999999999999", "1.234567890123456e-10"},
      std::pair{"0.1", "-0.1"}, std::pair{"-0.0", "180"}}) {
    Fixture f; f.begin();
    JsonDocument snapshot, request;
    f.device.write_settings_json(snapshot.to<JsonObject>());
    request["schema"] = 1; request["expected_revision"] = 1;
    request["settings"] = snapshot["settings"];
    request["settings"]["latitude"] = serialized(latitude);
    request["settings"]["longitude"] = serialized(longitude);
    std::string payload; serializeJson(request, payload);
    payload.replace(payload.find("\"latitude\""), 10, "\"lati\\u0074ude\"");
    f.device.read_settings_json(payload);
    CHECK(f.device.settings_request_ok());
    const auto saved = *f.device.settings_service()->saved();
    CHECK(saved.value.prayer.latitude == std::strtod(latitude, nullptr));
    CHECK(saved.value.prayer.longitude == std::strtod(longitude, nullptr));
    CHECK(std::signbit(saved.value.prayer.latitude) == std::signbit(std::strtod(latitude, nullptr)));
    f.device.utc = epoch({2026,9,25},5)-1; f.device.update();
    const auto writes = nvs_test::writes;
    for (unsigned repeat = 0; repeat < 3; ++repeat) {
      snapshot.clear(); f.device.write_settings_json(snapshot.to<JsonObject>());
      if (repeat == 0) {
        std::string exported; serializeJson(snapshot, exported);
        std::puts(exported.c_str());  // Checked with Python's JSON decoder too.
      }
      // Confirm exports preserve the stored number for a standard JSON client.
      for (const auto &[name, expected] : {
          std::pair{"latitude", saved.value.prayer.latitude},
          std::pair{"longitude", saved.value.prayer.longitude}}) {
        std::string number; serializeJson(snapshot["settings"][name], number);
        CHECK(std::strtod(number.c_str(), nullptr) == expected);
      }
      request["expected_revision"] = saved.revision;
      request["settings"] = snapshot["settings"];
      payload.clear(); serializeJson(request, payload);
      f.device.read_settings_json(payload);
      CHECK(f.device.settings_request_ok() && nvs_test::writes == writes);
      CHECK(*f.device.settings_service()->saved() == saved);
    }
    // Saving only volume when Fajr is due, before its poll, must not rearm.
    f.device.utc = epoch({2026,9,25},5); mono += 1000;
    request["settings"]["volume"] = 35;
    payload.clear(); serializeJson(request, payload);
    f.device.read_settings_json(payload);
    CHECK(f.device.settings_request_ok() && f.value().prayer == saved.value.prayer);
    CHECK(nvs_test::writes == writes + 1);
    f.device.loop(); f.device.update(); CHECK(f.audio.starts == 1);
  }
}
static void json_transport() {
  Fixture f; f.begin();
  JsonDocument snapshot;
  f.device.write_settings_json(snapshot.to<JsonObject>());
  CHECK(snapshot["settings"].size() == 10 && snapshot["revision"].as<unsigned>() == 1);
  CHECK(snapshot["consumed_through"]["fajr"].is<int32_t>());
  JsonDocument request;
  request["schema"] = 1; request["expected_revision"] = 1;
  request["settings"] = snapshot["settings"];
  request["settings"]["volume"] = 31;
  std::string payload; serializeJson(request,payload);
  f.device.read_settings_json(payload);
  CHECK(f.device.settings_request_ok() && f.value().volume == 31);
  f.device.read_settings_json(payload);
  CHECK(!f.device.settings_request_ok() && f.device.settings_request_error() == "revision conflict");
  request["expected_revision"] = 2;
  payload.clear(); serializeJson(request,payload);
  const auto writes = nvs_test::writes;
  f.device.read_settings_json(payload);
  CHECK(f.device.settings_request_ok() && nvs_test::writes == writes);
  for (unsigned fault=0; fault<12; ++fault) {
    JsonDocument invalid; invalid.set(request);
    if (fault==0) invalid["settings"]["volume"] = true;
    if (fault==1) invalid["settings"]["volume"] = 256;
    if (fault==2) invalid["settings"]["latitude"] = "43.0";
    if (fault==3) invalid["settings"]["enabled"]["fajr"] = 1;
    if (fault==4) invalid["settings"]["offsets"]["fajr"] = 0.5;
    if (fault==5) invalid["settings"]["timezone_rules"]["start"]["type"] = 5;
    if (fault==6) invalid["settings"]["surprise"] = 1;
    if (fault==7) invalid["schema"] = true;
    if (fault==8) invalid["expected_revision"] = -1;
    if (fault==9) invalid["settings"]["method"] = std::string("muslim_world_league\0extra",24);
    if (fault==10) invalid["settings"]["latitude"] = serialized("90.00000000000001");
    if (fault==11) invalid["settings"]["longitude"] = serialized("180.00000000000003");
    payload.clear(); serializeJson(invalid,payload);
    f.device.read_settings_json(payload);
    CHECK(!f.device.settings_request_ok() && nvs_test::writes == writes);
  }
  for (const auto &invalid : {std::string("{broken"),std::string(4097,' ')}) {
    f.device.read_settings_json(invalid); CHECK(!f.device.settings_request_ok());
  }
}
#endif
int main() {
  updates_and_replay(); volume_and_faults(); timezones(); occurrence_identity(); setup_gate_and_preview();
#ifdef OPENATHAN_JSON_TEST
  json_transport(); coordinate_roundtrip(); local_api();
#endif
}
