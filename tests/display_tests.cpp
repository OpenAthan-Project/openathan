#include "presenter.h"
#include "render.h"
#include "backlight.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); std::abort(); } } while (0)
using namespace openathan;
using namespace openathan::screen;
bool equal(const auto &value, const char *expected) { return std::string_view(value.data()) == expected; }

static void round_capture(const char *name, const Frame &frame, const char *directory) {
  std::array<uint32_t, 360 * 360> pixels{};
  const uint32_t ring = frame.proximity == LightMode::GREEN ? 0x00FF00 : frame.proximity == LightMode::ORANGE ? 0xFF6000 : 0xFF0000;
  render_round(frame, [&](int x, int y, uint32_t rgb) {
    CHECK(x >= 0 && x < 360 && y >= 0 && y < 360);
    const int dx = 2*x+1-360, dy = 2*y+1-360, distance = dx*dx+dy*dy;
    CHECK(distance <= 360*360);
    if (rgb == ring) CHECK(distance >= 328*328 && distance <= 344*344);
    else CHECK(distance < 328*328); // All text fits inside the ring, without clipping.
    pixels[y*360+x] = rgb;
  });
  CHECK(pixels[8*360+179] == ring && pixels[16*360+179] == 0);
  for (auto gap : {std::pair{236,252}, std::pair{268,310}})
    for (int y=gap.first; y<gap.second; ++y) for (int x=60; x<300; ++x)
      CHECK(pixels[y*360+x] == 0 || pixels[y*360+x] == ring);
  // The suffix shares the time's visible baseline, and never occupies its own row.
  const int time_width = static_cast<int>(std::strlen(frame.main.data())) * 48;
  const int suffix_width = static_cast<int>(std::strlen(frame.meridiem.data())) * 16;
  const int time_x = (360-time_width-(suffix_width ? 12 : 0)-suffix_width)/2;
  const int suffix_x = time_x+time_width+12;
  int digits_bottom=-1, suffix_bottom=-1;
  bool countdown=false, footer=false;
  for (int y=188; y<236; ++y) for (int x=0; x<360; ++x) {
    if (pixels[y*360+x] == 0xFFFFFF) {
      CHECK(x>=time_x && x<time_x+time_width);
      digits_bottom=y;
    }
    if (pixels[y*360+x] == 0xD0D8D8) {
      CHECK(suffix_width && x>=suffix_x && x<suffix_x+suffix_width && y>=216);
      suffix_bottom=y;
    }
  }
  CHECK(digits_bottom==229);
  CHECK(suffix_bottom==(suffix_width ? digits_bottom : -1));
  for (int y=252; y<268; ++y) for (int x=60; x<300; ++x)
    countdown |= pixels[y*360+x] == 0xD0D8D8;
  for (int y=310; y<326; ++y) for (int x=60; x<300; ++x)
    footer |= pixels[y*360+x] == 0xD0D8D8;
  CHECK(countdown && footer == !equal(frame.footer,""));
  if (!directory) return;
  std::filesystem::create_directories(directory);
  std::ofstream file(std::filesystem::path(directory)/(std::string(name)+".ppm"),std::ios::binary);
  file << "P6\n360 360\n255\n";
  for (auto rgb : pixels) {
    const char bytes[]{static_cast<char>(rgb>>16),static_cast<char>(rgb>>8),static_cast<char>(rgb)};
    file.write(bytes,3);
  }
  CHECK(file.good());
}
static void round_countdown(const char *directory) {
  const std::pair<int64_t,const char *> cases[]{
      {-1,""},{0,""},{1,"In <1min"},{59,"In <1min"},{60,"In 1min"},{61,"In 2min"},
      {3599,"In 1hr"},{3600,"In 1hr"},{3601,"In 1hr 1min"},{8100,"In 2hr 15min"},
      {86399,"In 24hr"},{86400,"In 24hr"},{179940,"In 49hr 59min"}};
  for (auto [seconds,expected] : cases) CHECK(equal(format_countdown(seconds),expected));
  CHECK(equal(format_countdown(std::numeric_limits<int64_t>::max()),""));
  Inputs in;
  in.round = in.setup_complete = in.clock_valid = in.wifi_connected = in.prayers_enabled = true;
  in.status.automatic_ready = true; in.utc=10000; in.local_time="18:27"; in.next_time="18:45";
  in.visual_next=Event{{20735,Prayer::MAGHRIB},11080,true,{}};
  // The audible next event deliberately differs: every rendered cue follows the visual event.
  in.status.next=Event{{20735,Prayer::ISHA},18000,true,{}};
  auto f=present(in);
  CHECK(equal(f.heading,"Maghrib") && equal(f.main,"18:45") && equal(f.countdown,"In 18min"));
  CHECK(f.proximity==LightMode::ORANGE);
  FrameCache cache; CHECK(cache.accept(f)); ++in.utc; CHECK(!cache.accept(present(in)));
  in.utc+=60; CHECK(cache.accept(present(in)));
  for (auto [seconds,mode] : {std::pair{1801,LightMode::GREEN},std::pair{1800,LightMode::ORANGE},
                            std::pair{601,LightMode::ORANGE},std::pair{600,LightMode::RED}}) {
    in.visual_next->utc=in.utc+seconds; CHECK(present(in).proximity==mode);
  }
  for (unsigned hours : {12U,24U}) for (unsigned p=0; p<5; ++p) for (unsigned state=0; state<4; ++state) {
    in.local_time="16:46"; // 49hr 59min until 18:45 two days later.
    in.hours=hours; in.wifi_connected=false; in.visual_next->key.prayer=static_cast<Prayer>(p);
    in.visual_next->utc=in.utc+179940; in.visual_next->enabled=state!=1;
    in.prayers_enabled=state!=1; in.status.automatic_ready=state!=3;
    in.status.skip=state==2 ? std::optional{in.visual_next->key} : std::nullopt;
    f=present(in);
    CHECK(f.upcoming && equal(f.footer,"Offline") && equal(f.countdown,"In 49hr 59min"));
    CHECK(equal(f.detail,state==1 ? "Muted" : state==2 ? "Will be skipped" : state==3 ? "Not ready yet" : "Next Athan"));
    const auto name=std::string("round-")+prayer_name(in.visual_next->key.prayer)+"-"+std::to_string(hours)+"-"+std::to_string(state);
    round_capture(name.c_str(),f,directory);
  }
  in.hours=12; in.visual_next->key.prayer=Prayer::MAGHRIB; in.status.skip.reset();
  in.visual_next->enabled=in.prayers_enabled=in.status.automatic_ready=in.wifi_connected=true;
  for (const auto *time : {"06:08", "00:59", "12:59", "23:59"}) for (unsigned hours : {12U,24U}) {
    in.hours=hours; in.next_time=time;
    const auto name=std::string("round-time-")+time+"-"+std::to_string(hours);
    round_capture(name.c_str(),present(in),directory);
  }
  in.hours=12; in.next_time="06:08"; in.local_time="22:30";
  in.visual_next->key={20736,Prayer::FAJR}; in.visual_next->utc=in.utc+27480;
  round_capture("round-inline-am",present(in),directory);
  in.visual_next->key={20735,Prayer::MAGHRIB};
  in.hours=12; in.next_time="18:45";
  for (auto [seconds,name] : {std::pair{8100,"round-hours"},std::pair{2700,"round-green"},
                            std::pair{1080,"round-orange"},std::pair{300,"round-red"},std::pair{59,"round-subminute"}}) {
    in.local_time=seconds==8100 ? "16:30" : seconds==2700 ? "18:00" : seconds==1080 ? "18:27" : seconds==300 ? "18:40" : "18:44";
    in.visual_next->utc=in.utc+seconds; round_capture(name,present(in),directory);
  }
  in.visual_next->shared_with=EventKey{20735,Prayer::ISHA}; in.status.skip=in.visual_next->shared_with;
  CHECK(equal(present(in).detail,"Will be skipped"));
  in.status.skip=EventKey{20734,Prayer::ISHA}; CHECK(equal(present(in).detail,"Next Athan"));
  for (unsigned state=0; state<6; ++state) {
    Inputs blocked=in;
    if (state==0) blocked.setup_complete=false;
    if (state==1) blocked.clock_valid=false;
    if (state==2) blocked.status.playing=true;
    if (state==3) blocked.status.fault=Fault::STORAGE;
    if (state==4) blocked.visual_next.reset();
    if (state==5) blocked.visual_next->utc=blocked.utc;
    const auto frame=present(blocked);
    CHECK(!frame.upcoming && equal(frame.countdown,"") && frame.proximity==LightMode::OFF);
  }
  in.round=false; CHECK(equal(present(in).heading,"Isha") && equal(present(in).countdown,""));
}

void capture(const char *name, const Frame &frame, const char *directory, bool upcoming = false) {
  CHECK(frame.upcoming == upcoming);
  std::array<uint32_t, 128 * 128> pixels{};
  render(frame, [&pixels](int x, int y, uint32_t rgb) {
    CHECK(x >= 0 && x < 128 && y >= 0 && y < 128);
    pixels[y * 128 + x] = rgb;
  });
  render_round(frame, [](int x, int y, uint32_t) {
    CHECK(x >= 0 && x < 360 && y >= 0 && y < 360);
    // Pixel centers must fit inside the visible circle, without silent clipping.
    const int dx = 2*x + 1 - 360, dy = 2*y + 1 - 360;
    CHECK(dx*dx + dy*dy <= 360*360);
  });
  if (upcoming) {
    const auto occupied = [&pixels](unsigned first, unsigned end) {
      for (unsigned i = first * 128; i < end * 128; ++i)
        if (pixels[i]) return true;
      return false;
    };
    // The reading order must stay distinct, including the longest status label
    // and prayer name. Empty bands prevent a duplicate label below the time.
    CHECK(occupied(6, 14) && occupied(30, 38) && occupied(44, 60) && occupied(68, 92));
    CHECK(occupied(100, 108) == !equal(frame.meridiem, ""));
    CHECK(occupied(116, 124) == !equal(frame.footer, ""));
    for (const auto &gap : {std::pair{14U, 30U}, {38U, 44U}, {60U, 68U}, {92U, 100U}, {108U, 116U}})
      CHECK(!occupied(gap.first, gap.second));
  }
  if (!directory) return;
  std::filesystem::create_directories(directory);
  std::ofstream file(std::filesystem::path(directory) / (std::string(name) + ".ppm"), std::ios::binary);
  file << "P6\n128 128\n255\n";
  for (auto rgb : pixels) {
    const char bytes[]{static_cast<char>(rgb >> 16), static_cast<char>(rgb >> 8), static_cast<char>(rgb)};
    file.write(bytes, 3);
  }
  CHECK(file.good());
}
int main(int argc, char **argv) {
  const char *directory = argc == 2 ? argv[1] : nullptr;
  round_countdown(directory);
  Inputs in;
  in.setup_complete = in.clock_valid = in.wifi_connected = in.prayers_enabled = true;
  in.local_time = "12:04"; in.next_time = "12:30";
  in.status.automatic_ready = true;
  const EventKey key{20729, Prayer::DHUHR};
  in.status.next = Event{key, 10000, true, {}};
  auto idle = present(in);
  CHECK(equal(idle.heading, "Dhuhr") && equal(idle.main, "12:30") && idle.main_scale == 3);
  CHECK(equal(idle.clock, "12:04") && equal(idle.detail, "Next Athan"));
  CHECK(equal(idle.footer, "")); // Normal operation needs no successful-connection label.
  capture("idle", idle, directory, true);
  FrameCache cache;
  CHECK(cache.accept(idle)); CHECK(!cache.accept(idle));
  in.local_time = "12:05"; CHECK(cache.accept(present(in)));
  in.wifi_connected = false;
  auto offline = present(in);
  CHECK(equal(offline.footer, "Offline") && equal(offline.detail, "Next Athan"));
  capture("offline", offline, directory, true);
  in.wifi_connected = true; in.status.playing = true;
  in.stop_button = false;
  CHECK(equal(present(in).detail, "Use phone")); capture("phone-playback", present(in), directory);
  in.stop_button = true;
  CHECK(equal(present(in).detail, "Button to stop"));
  CHECK(equal(present(in).main, "Playing") && equal(present(in).footer, ""));
  capture("playback", present(in), directory);
  in.status.playing = false; in.status.skip = key;
  CHECK(equal(present(in).detail, "Will be skipped")); capture("skip", present(in), directory, true);
  in.status.skip = EventKey{key.day, Prayer::FAJR};
  CHECK(equal(present(in).detail, "Next Athan")); // A stale/different skip must not label this event.
  in.status.next->shared_with = in.status.skip;
  CHECK(equal(present(in).detail, "Will be skipped"));
  in.status.next->shared_with.reset(); in.status.skip.reset();
  in.setup_complete = false;
  CHECK(equal(present(in).main, "Needed")); capture("setup", present(in), directory);
  in.setup_complete = true; in.clock_valid = false;
  CHECK(equal(present(in).clock, "--:--") && equal(present(in).main, "Waiting"));
  capture("waiting-time", present(in), directory);
  in.wifi_connected = false;
  CHECK(equal(present(in).detail, "Connect Wi-Fi"));
  in.status.playing = true;
  CHECK(equal(present(in).main, "Playing") && equal(present(in).clock, "--:--"));
  in.status.playing = false; in.wifi_connected = true;
  in.clock_valid = true; in.prayers_enabled = false; in.status.next.reset();
  CHECK(equal(present(in).main, "Off")); capture("disabled", present(in), directory);
  in.prayers_enabled = true;
  CHECK(equal(present(in).detail, "No time set")); capture("no-next-time", present(in), directory);
  in.status.next = Event{key, 10000, true, {}};
  in.status.automatic_ready = false;
  CHECK(equal(present(in).detail, "Not ready yet")); capture("not-ready", present(in), directory, true);
  in.status.fault = Fault::STORAGE;
  CHECK(present(in).error && equal(present(in).main, "Storage")); capture("storage-fault", present(in), directory);
  in.status.fault = Fault::AUDIO_UNAVAILABLE;
  CHECK(equal(present(in).main, "Audio")); capture("audio-fault", present(in), directory);
  in.status.fault = Fault::INVALID_SCHEDULE;
  CHECK(equal(present(in).main, "Schedule")); capture("schedule-fault", present(in), directory);
  in.status.fault = Fault::NONE; in.storage_fault = true;
  CHECK(equal(present(in).main, "Storage"));
  in.storage_fault = false; in.status.automatic_ready = true;
  in.local_time = "00:00"; in.next_time = "05:59";
  CHECK(equal(present(in).clock, "00:00") && equal(present(in).main, "05:59"));
  CHECK(!valid_time("24:00") && !valid_time("12:60") && !valid_time("1:00") && !valid_time("ab:cd"));
  in.local_time = "xx:xx"; in.next_time = "24:00";
  CHECK(equal(present(in).clock, "--:--") && equal(present(in).detail, "No time set"));
  // Render every prayer and every fault to catch text overflow with real copy.
  in.next_time = "23:59";
  for (unsigned p = 0; p < 5; ++p) {
    in.status.next->key.prayer = static_cast<Prayer>(p);
    capture(prayer_name(static_cast<Prayer>(p)), present(in), directory, true);
  }
  in.local_time = "00:00"; in.next_time = "12:00"; in.hours = 12;
  auto twelve = present(in);
  CHECK(equal(twelve.clock,"12:00 AM") && equal(twelve.main,"12:00") && equal(twelve.meridiem,"PM"));
  capture("twelve-noon",twelve,directory,true);
  CHECK(cache.accept(twelve)); CHECK(!cache.accept(twelve));
  in.local_time = "13:05"; in.next_time = "23:59";
  twelve = present(in);
  CHECK(equal(twelve.clock,"1:05 PM") && equal(twelve.main,"11:59") && equal(twelve.meridiem,"PM"));
  capture("twelve-evening",twelve,directory,true);
  in.hours = 24; CHECK(cache.accept(present(in)) && equal(present(in).meridiem,""));
  in.hours = 12; in.local_time = "20:30"; in.next_time = "06:02";
  in.status.next->key.prayer = Prayer::FAJR;
  capture("approved-layout", present(in), directory, true);
  in.local_time = "12:00"; in.next_time = "00:00";
  CHECK(equal(present(in).clock, "12:00 PM") && equal(present(in).main, "12:00") && equal(present(in).meridiem, "AM"));
  capture("twelve-midnight", present(in), directory, true);
  for (unsigned hours : {12U, 24U}) {
    in.hours = hours; in.local_time = "23:59"; in.next_time = "23:59";
    for (unsigned p = 0; p < 5; ++p) {
      in.status.next->key.prayer = static_cast<Prayer>(p);
      for (unsigned state = 0; state < 3; ++state) {
        in.status.automatic_ready = state != 2;
        in.status.skip = state == 1 ? std::optional{in.status.next->key} : std::nullopt;
        for (bool online : {false, true}) {
          in.wifi_connected = online;
          const std::string name = std::string(prayer_name(static_cast<Prayer>(p))) + "-" +
              std::to_string(hours) + "-" + (state == 0 ? "ready" : state == 1 ? "skipped" : "not-ready") +
              (online ? "-online" : "-offline");
          capture(name.c_str(), present(in), directory, true);
        }
      }
    }
  }
  using esphome::atom_s3r_display::Backlight;
  Backlight light; light.setup(); CHECK(!light.is_failed() && light.writes.size() == 9);
  CHECK(light.writes.back().reg == 0x0E && light.writes.back().data[0] == 128);
  for (const auto &write : light.writes) CHECK(write.reg <= 0x0F || write.reg == 0x70);
  for (unsigned fail = 1; fail <= 9; ++fail) {
    Backlight broken; broken.fail_at = fail; broken.setup();
    CHECK(broken.is_failed() && broken.writes.size() == fail); // Stop immediately after I2C failure.
  }
  const auto count = light.writes.size();
  CHECK(light.apply(1) && light.writes.size()==count+1);
  CHECK(light.writes.back().reg==0x0E && light.writes.back().data[0]==3);
  CHECK(light.apply(1) && light.writes.size()==count+1);
  CHECK(!light.apply(0) && !light.apply(101) && light.writes.size()==count+1);
  light.fail_at=count+2;
  CHECK(!light.apply(100) && !light.is_failed());
  CHECK(light.apply(100) && light.writes.back().reg==0x0E && light.writes.back().data[0]==255);
  const auto restored_count = light.writes.size();
  light.fail_at=restored_count+1;
  CHECK(!light.apply(50));
  // The failed transaction may have reached hardware. Reverting to the last
  // acknowledged value must send PWM again rather than trust the stale cache.
  CHECK(light.apply(100) && light.writes.size()==restored_count+2);
  CHECK(light.writes.back().reg==0x0E && light.writes.back().data[0]==255);
  CHECK(light.apply(100) && light.writes.size()==restored_count+2);
  Backlight uninitialized; CHECK(!uninitialized.apply(10) && uninitialized.writes.empty());
  Backlight maximum; maximum.set_brightness(100); maximum.setup(); CHECK(maximum.writes.back().data[0] == 255);
  for (unsigned value : {0U, 101U}) {
    Backlight bad; bad.set_brightness(value); bad.setup(); CHECK(bad.is_failed() && bad.writes.empty());
  }
}
