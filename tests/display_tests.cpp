#include "presenter.h"
#include "render.h"
#include "backlight.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); std::abort(); } } while (0)
using namespace openathan;
using namespace openathan::screen;
bool equal(const auto &value, const char *expected) { return std::string_view(value.data()) == expected; }

void capture(const char *name, const Frame &frame, const char *directory) {
  std::array<uint32_t, 128 * 128> pixels{};
  render(frame, [&pixels](int x, int y, uint32_t rgb) {
    CHECK(x >= 0 && x < 128 && y >= 0 && y < 128);
    pixels[y * 128 + x] = rgb;
  });
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
  Inputs in;
  in.setup_complete = in.clock_valid = in.wifi_connected = in.prayers_enabled = true;
  in.local_time = "12:04"; in.next_time = "12:30";
  in.status.automatic_ready = true;
  const EventKey key{20729, Prayer::DHUHR};
  in.status.next = Event{key, 10000, true, {}};
  auto idle = present(in);
  CHECK(equal(idle.heading, "Dhuhr") && equal(idle.main, "12:30") && idle.main_scale == 3);
  CHECK(equal(idle.clock, "12:04") && equal(idle.detail, "Next Athan"));
  capture("idle", idle, directory);
  FrameCache cache;
  CHECK(cache.accept(idle)); CHECK(!cache.accept(idle));
  in.local_time = "12:05"; CHECK(cache.accept(present(in)));
  in.wifi_connected = false;
  auto offline = present(in);
  CHECK(equal(offline.footer, "Offline") && equal(offline.detail, "Next Athan"));
  capture("offline", offline, directory);
  in.wifi_connected = true; in.status.playing = true;
  CHECK(equal(present(in).main, "Playing")); capture("playback", present(in), directory);
  in.status.playing = false; in.status.skip = key;
  CHECK(equal(present(in).detail, "Will be skipped")); capture("skip", present(in), directory);
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
  CHECK(equal(present(in).detail, "Not ready yet")); capture("not-ready", present(in), directory);
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
    capture(prayer_name(static_cast<Prayer>(p)), present(in), directory);
  }
  using esphome::atom_s3r_display::Backlight;
  Backlight light; light.setup(); CHECK(!light.is_failed() && light.writes.size() == 9);
  CHECK(light.writes.back().reg == 0x0E && light.writes.back().data[0] == 26);
  for (const auto &write : light.writes) CHECK(write.reg <= 0x0F || write.reg == 0x70);
  for (unsigned fail = 1; fail <= 9; ++fail) {
    Backlight broken; broken.fail_at = fail; broken.setup();
    CHECK(broken.is_failed() && broken.writes.size() == fail); // Stop immediately after I2C failure.
  }
  Backlight maximum; maximum.set_brightness(100); maximum.setup(); CHECK(maximum.writes.back().data[0] == 255);
  for (unsigned value : {0U, 101U}) {
    Backlight bad; bad.set_brightness(value); bad.setup(); CHECK(bad.is_failed() && bad.writes.empty());
  }
}
