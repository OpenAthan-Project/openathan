#include "esphome/components/voice_pyramid_audio/audio.h"
#include "nvs_memory.h"
#include <cassert>
#include <limits>

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
static void adapter() {
  switch_::Switch enable; Clock clock; Amplifier amplifier; es8311::ES8311 dac;
  openathan_audio::PartitionAudio playback; Audio audio;
  audio.set_enable(&enable); audio.set_clock(&clock); audio.set_amplifier(&amplifier);
  audio.set_dac(&dac); audio.set_playback(&playback); audio.setup();
  assert(!enable.state && !audio.ready() && !audio.start(::openathan::Track::NORMAL));
  assert(audio.request_volume_percent(64)); audio.loop(); assert(dac.volumes.empty());
  playback.available = true; audio.loop(); assert(!audio.ready());
  amplifier.setup(); audio.loop(); assert(!audio.ready() && !enable.state);
  audio.loop(); audio.loop(); assert(audio.ready() && enable.state && audio.volume_percent() == 64);
  assert(audio.start(::openathan::Track::NORMAL) && audio.playing());
  assert(!audio.request_volume_percent(101));
  playback.apply_requests = false;
  assert(audio.request_volume_percent(100) && !enable.state);
  const auto gain_writes = dac.volumes.size();
  audio.loop(); assert(dac.volumes.size() == gain_writes && !audio.ready());
  playback.level = 0; audio.loop(); assert(!audio.ready() && !enable.state);
  audio.loop(); assert(!audio.ready());  // Dropped software-volume request stays gated.
  playback.level = 1; audio.loop();
  assert(audio.ready() && audio.volume_percent() == 100 && dac.volumes.back() == 0.75f);
  playback.apply_requests = true;
  assert(audio.request_volume_percent(0)); audio.loop(); audio.loop();
  assert(audio.volume_percent() == 0 && playback.level == 0);
  assert(audio.request_volume_percent(1)); audio.loop(); audio.loop();
  assert(audio.volume_percent() == 1 && dac.volumes.back() < 0.75f);
  audio.stop(); assert(!audio.playing());
  playback.available = false; audio.loop(); assert(!enable.state && !audio.ready());
  playback.available = true; audio.loop(); assert(audio.ready());
  audio.on_shutdown(); assert(!enable.state);
  for (unsigned failure = 0; failure < 6; ++failure) {
    switch_::Switch failed_enable; Clock failed_clock; Amplifier failed_amp;
    es8311::ES8311 failed_dac; openathan_audio::PartitionAudio failed_playback; Audio failed;
    failed.set_enable(&failed_enable); failed.set_clock(&failed_clock); failed.set_amplifier(&failed_amp);
    failed.set_dac(&failed_dac); failed.set_playback(&failed_playback);
    failed_amp.setup(); failed_playback.available = true; failed.setup();
    failed_dac.mute_write_ok = failure != 0; failed_dac.volume_write_ok = failure != 1;
    failed_dac.unmute_write_ok = failure != 2;
    failed_dac.register_write_ok = failure != 3;
    failed_dac.register_read_ok = failure != 4;
    failed_dac.wrong_readback = failure == 5;
    assert(failed.request_volume_percent(80)); failed.loop();
    assert(failed.is_failed() && !failed_enable.state && !failed.ready());
    assert(!failed.request_volume_percent(20) && !failed.start(::openathan::Track::FAJR));
  }
}
int main() { curves(); storage(); interrupted_saves(); isolated_storage(); adapter(); }
