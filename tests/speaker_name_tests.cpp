#include "openathan/speaker_name.h"
#include "nvs_settings_store.h"
#include "nvs_memory.h"
#include <cstdio>
#include <cstdlib>
using namespace openathan;
namespace openathan_storage = esphome::openathan_storage;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); std::abort(); } } while (0)
int main() {
  std::string normalized;
  CHECK(normalize_speaker_name(" \tBedroom \r\n", normalized) && normalized == "Bedroom");
  CHECK(normalize_speaker_name("غرفة النوم 🕌", normalized));
  CHECK(normalize_speaker_name("", normalized) && normalized.empty());
  std::string emoji;
  for (unsigned i=0;i<32;++i) emoji += "🕌";
  CHECK(normalize_speaker_name(emoji, normalized) && normalized.size()==128);
  for (const auto &invalid : {std::string(33,'a'), emoji+"a", std::string("a\nb"),
       std::string("a\0b",3), std::string("\xC0\xAF"), std::string("\xED\xA0\x80"),
       std::string("\xF4\x90\x80\x80"), std::string("\x80"), std::string("\xE2\x82"),
       std::string("\xC2\x85"), std::string("a\x7F")})
    CHECK(!normalize_speaker_name(invalid, normalized));
  nvs_test::reset();
  esphome::openathan_component::NvsSpeakerNameStore store;
  SpeakerNamePreferences preferences(store); preferences.begin();
  CHECK(preferences.saved()->name.empty() && preferences.saved()->revision == 1 && nvs_test::writes == 0);
  CHECK(preferences.update(std::string(33, 'a'), 1) == SpeakerNameResult::INVALID);
  CHECK(preferences.update("Bedroom", 0) == SpeakerNameResult::CONFLICT);
  CHECK(preferences.update("", 1) == SpeakerNameResult::UNCHANGED && nvs_test::writes == 0);
  nvs_test::committed[{openathan_storage::PRAYER, "settings"}] = {1,2,3};
  nvs_test::committed[{openathan_storage::PRAYER, "scheduler"}] = {4,5,6};
  const auto before = nvs_test::committed;
  CHECK(preferences.update("Bedroom", 1) == SpeakerNameResult::SAVED);
  for (const auto &[key, value] : before) CHECK(nvs_test::committed.at(key) == value);
  CHECK(nvs_test::committed.contains({openathan_storage::PRAYER, "speaker_name"}));
  CHECK(!nvs_test::committed.contains({openathan_storage::TEST_MODE ? "openathan" : "oa_test", "speaker_name"}));
  nvs_test::power_cycle();
  esphome::openathan_component::NvsSpeakerNameStore reboot_store;
  SpeakerNamePreferences reboot(reboot_store); reboot.begin();
  CHECK(reboot.saved()->name == "Bedroom" && reboot.saved()->revision == 2);
  CHECK(reboot.update("Living room", 1) == SpeakerNameResult::CONFLICT);
  nvs_test::fail_commit = true;
  CHECK(reboot.update("Living room", 2) == SpeakerNameResult::STORAGE);
  CHECK(reboot.saved()->name == "Bedroom" && !reboot.writable());
  nvs_test::fail_commit = false; nvs_test::power_cycle();
  // Either permitted outcome of an interrupted write reloads a valid name.
  for (bool early : {false, true}) {
    nvs_test::reset();
    esphome::openathan_component::NvsSpeakerNameStore interrupted_store;
    SpeakerNamePreferences interrupted(interrupted_store); interrupted.begin();
    nvs_test::early_persist=early; nvs_test::cut_after_write=true;
    bool cut=false;
    try { interrupted.update("Bedroom",1); } catch (const std::runtime_error &) { cut=true; }
    CHECK(cut); nvs_test::power_cycle(); nvs_test::cut_after_write=false;
    esphome::openathan_component::NvsSpeakerNameStore restored_store;
    SpeakerNamePreferences restored(restored_store); restored.begin();
    CHECK(restored.writable() && restored.saved()->name==(early ? "Bedroom" : ""));
  }
  auto record = encode_speaker_name({2,"Bedroom"}); SavedSpeakerName decoded;
  CHECK(decode_speaker_name(record,decoded) && decoded.name == "Bedroom");
  for (size_t i = 0; i < record.size(); ++i) {
    auto broken = record; broken[i] ^= 1; CHECK(!decode_speaker_name(broken,decoded));
  }
  CHECK(!decode_speaker_name(encode_speaker_name({2,std::string(33, 'a')}),decoded));
  CHECK(!decode_speaker_name(encode_speaker_name({0,"Bedroom"}),decoded));
  nvs_test::committed[{openathan_storage::PRAYER,"speaker_name"}] = {0};
  esphome::openathan_component::NvsSpeakerNameStore broken_store;
  SpeakerNamePreferences broken(broken_store); broken.begin();
  CHECK(!broken.saved() && !broken.writable());
  CHECK(broken.update("Bedroom",1) == SpeakerNameResult::STORAGE);
  CHECK(nvs_test::committed.at({openathan_storage::PRAYER,"speaker_name"}) == std::vector<uint8_t>{0});
}
