#include "athan.h"
#include "dua.h"
#include "partition_audio.h"
#include "esphome/core/application.h"
#include <esp_ota_ops.h>
#include <esp_http_client.h>
#include <freertos/task.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>

using namespace esphome::openathan_device;
using namespace openathan;
static const std::string BASE = "https://github.com/OpenAthan-Project/athan-audio/releases/";
static const std::string VERSION = BASE + "download/v1.0.0/";
static std::vector<uint8_t> flash, original;
static esp_partition_t audio_partition{AUDIO_PARTITION_SIZE, 0x410000};
static std::map<std::string, std::vector<uint8_t>> durable, pending;
static std::map<nvs_handle_t, std::string> namespaces;
static unsigned next_handle{};
static int flash_operations{}, commits{}, reads{}, cut_flash{-1}, cut_commit{-1}, fail_commit{-1}, fail_read{-1};
static size_t cut_header_prefix{2048};
static bool fail_nvs_readback{}, commit_then_fail{}, fail_identity{};
static uint8_t firmware_identity{0x5a};
static int64_t now_us{};
static std::function<void()> on_flash;
static std::string normal_data(4097, 'n'), fajr_data(8193, 'f');
struct PowerCut {};
int64_t esp_timer_get_time() { return now_us; }
int nvs_open(const char *name, int, nvs_handle_t *handle) {
  *handle = ++next_handle; namespaces[*handle] = name; return ESP_OK;
}
void nvs_close(nvs_handle_t) {}
std::string key(nvs_handle_t handle, const char *name) { return namespaces.at(handle) + "/" + name; }
int nvs_get_blob(nvs_handle_t handle, const char *name, void *out, size_t *size) {
  const auto found = durable.find(key(handle, name));
  if (found == durable.end()) return ESP_ERR_NVS_NOT_FOUND;
  if (fail_nvs_readback && key(handle, name) == "oa_athan/record") return -1;
  if (out) { if (*size < found->second.size()) return -1; std::memcpy(out, found->second.data(), found->second.size()); }
  *size = found->second.size(); return ESP_OK;
}
int nvs_set_blob(nvs_handle_t handle, const char *name, const void *data, size_t size) {
  assert(key(handle, name) == "oa_athan/record" && size <= 1024);
  const auto *first = static_cast<const uint8_t *>(data); pending[key(handle, name)] = {first, first + size}; return ESP_OK;
}
int nvs_commit(nvs_handle_t) {
  ++commits;
  if (commits == fail_commit && !commit_then_fail) return -1;
  for (const auto &[name, value] : pending) durable[name] = value;
  pending.clear();
  if (commits == cut_commit) throw PowerCut{};
  return commits == fail_commit ? -1 : ESP_OK;
}
int nvs_erase_all(nvs_handle_t) { assert(false); return -1; }
const esp_partition_t *esp_partition_find_first(int, int, const char *name) { return name ? &audio_partition : &otadata; }
int esp_partition_get_sha256(const esp_partition_t *partition, uint8_t *out) {
  assert(partition == &running); std::fill_n(out, 32, firmware_identity); return fail_identity ? -1 : ESP_OK;
}
int esp_partition_mmap(const esp_partition_t *partition, size_t, size_t, int, const void **out, esp_partition_mmap_handle_t *handle) {
  assert(partition == &audio_partition); *out = flash.data(); *handle = 1; return ESP_OK;
}
void esp_partition_munmap(esp_partition_mmap_handle_t) {}
int esp_partition_read(const esp_partition_t *partition, size_t at, void *out, size_t size) {
  if (partition == &otadata) { std::memset(out, 0xff, size); return ESP_OK; }
  assert(partition == &audio_partition && at + size <= flash.size());
  if (++reads == fail_read) return -1;
  std::memcpy(out, flash.data() + at, size); return ESP_OK;
}
int esp_partition_erase_range(const esp_partition_t *partition, size_t at, size_t size) {
  assert(partition == &audio_partition && at % 4096 == 0 && size == 4096 && at + size <= DUA_BANK_START);
  ++flash_operations; std::fill_n(flash.begin() + at, flash_operations == cut_flash ? size / 2 : size, 0xff);
  if (flash_operations == cut_flash) throw PowerCut{};
  if (on_flash) on_flash(); return ESP_OK;
}
int esp_partition_write(const esp_partition_t *partition, size_t at, const void *data, size_t size) {
  assert(partition == &audio_partition && at + size <= DUA_BANK_START && size <= 4096);
  ++flash_operations; const auto *bytes = static_cast<const uint8_t *>(data);
  const size_t count = flash_operations == cut_flash ? (at == 0 ? cut_header_prefix : size / 2) : size;
  for (size_t i = 0; i < count; ++i) {
    assert((flash[at + i] & bytes[i]) == bytes[i]); flash[at + i] &= bytes[i];
  }
  if (flash_operations == cut_flash) throw PowerCut{};
  if (on_flash) on_flash(); return ESP_OK;
}
std::string digest(const std::string &value) {
  uint8_t bytes[32]; mbedtls_sha256(reinterpret_cast<const uint8_t *>(value.data()), value.size(), bytes, 0);
  std::string result; for (auto byte : bytes) { result += "0123456789abcdef"[byte >> 4]; result += "0123456789abcdef"[byte & 15]; } return result;
}
std::string catalog(const std::function<void(JsonObject)> &mutate = {}) {
  JsonDocument payload; auto root = payload.to<JsonObject>();
  root["kind"] = "athan_catalog";
  auto recordings = root["recordings"].to<JsonArray>();
  for (unsigned i = 0; i < 2; ++i) {
    auto recording = recordings.add<JsonObject>(); const auto &data = i ? fajr_data : normal_data;
    recording["attribution"] = "Synthetic host fixture"; recording["bytes"] = data.size();
    recording["file"] = i ? "fajr.mp3" : "normal.mp3"; recording["id"] = i ? "fajr-one" : "normal-one";
    recording["license"] = "CC0-1.0"; recording["name"] = i ? "Fajr One" : "Regular One";
    recording["role"] = i ? "fajr" : "normal"; recording["sha256"] = digest(data); recording["source"] = "https://example.test/fixture";
  }
  root["schema"] = 1; root["version"] = "v1.0.0";
  if (mutate) mutate(root);
  std::string encoded; serializeJson(payload, encoded);
  JsonDocument envelope; envelope["payload"] = encoded; envelope["signature"] = std::string(128, '0');
  std::string result; serializeJson(envelope, result); return result;
}
void reset() {
  assert(!pending_worker && !live_http_clients && !live_read_buffers);
  reset_http(); flash = original; durable.clear(); pending.clear(); namespaces.clear(); next_handle = 0;
  flash_operations = commits = reads = 0; cut_flash = cut_commit = fail_commit = fail_read = -1;
  cut_header_prefix = 2048;
  now_us = 0;
  fail_nvs_readback = commit_then_fail = fail_identity = false; firmware_identity = 0x5a;
  signature_valid = task_available = true; read_buffer_allocation_fails = spiram_allocation_fails = false;
  running_state = ESP_OTA_IMG_VALID; boot_partition = &running; boot_query_fails = metadata_erased = false;
  esphome::App.reboots = 0; on_flash = {};
  http_replies[BASE + "latest/download/catalog.json"] = {200, "", catalog()};
  http_replies[VERSION + "catalog.json"] = {200, "", catalog()};
  http_replies[VERSION + "normal.mp3"] = {200, "", normal_data};
  http_replies[VERSION + "fajr.mp3"] = {200, "", fajr_data};
}
void preserved() { assert(std::equal(original.begin() + DUA_BANK_START, original.end(), flash.begin() + DUA_BANK_START)); }
struct Speaker {
  esphome::speaker::SpeakerMediaPlayer player;
  esphome::openathan_audio::PartitionAudio audio;
  esphome::openathan_component::OpenAthan athan;
  DuaMedia dua;
  AthanMedia media;
  Speaker() {
    audio.set_player(&player); audio.setup(); athan.audio = &audio; athan.health = audio.ready();
    athan.dua_prefs.begin(); dua.begin(&athan); media.begin(&athan);
  }
  JsonDocument snapshot() { JsonDocument out; media.snapshot(out.to<JsonObject>()); return out; }
  std::string state() { return snapshot()["state"].as<std::string>(); }
  int action(const std::string &name, const std::string &json = "{}") {
    JsonDocument input; assert(!deserializeJson(input, json)); std::string error;
    return media.action(name, input.as<JsonObjectConst>(), error);
  }
  void tick(bool connected = true, bool permitted = true, bool firmware_idle = true, bool confirmed = true) {
    dua.loop(false, false); media.loop(connected, permitted, firmware_idle, confirmed, dua.startup_ready());
  }
  void load() { assert(action("catalog") == 202); tick(); assert(pending_worker); run_worker(); assert(snapshot()["catalog"]["state"] == "ready"); }
  void select(uint32_t revision = 0) {
    assert(action("selection", "{\"schema\":1,\"expected_revision\":" + std::to_string(revision) + ",\"normal\":\"normal-one\",\"fajr\":\"fajr-one\"}") == 202);
  }
  void download() { tick(); assert(pending_worker); run_worker(); assert(flash_operations == 0); }
  void install() {
    tick(); assert(media.owns_storage() && !pending_worker);
    player.drain_commands(); tick(); assert(pending_worker); run_worker(); tick();
    assert(state() == "restarting" && esphome::App.reboots == 1);
  }
};
void complete_boot() {
  Speaker boot; boot.tick();
  if (boot.state() != "ready") { std::string message; serializeJson(boot.snapshot(), message); std::cerr << message << '\n'; }
  assert(boot.state() == "ready");
  const auto snapshot = boot.snapshot(); assert(snapshot["installed"]["normal"]["id"] == "normal-one" && snapshot["requested"].isNull());
  assert(boot.audio.dua_ready() && !boot.media.owns_storage());
  assert(boot.audio.start(Track::FAJR)); boot.player.drain_commands(); boot.audio.loop();
  const auto *file = boot.player.queued.back();
  assert(file->length == fajr_data.size() && file->data == flash.data() + ((4096 + normal_data.size() + 15) & ~size_t(15)));
  assert(std::memcmp(file->data, fajr_data.data(), fajr_data.size()) == 0);
  preserved();
}
void normal_and_noop() {
  reset();
  {
    Speaker speaker; assert(speaker.snapshot()["installed"]["normal"]["id"] == "");
    speaker.load(); speaker.select(); speaker.download(); speaker.install();
    assert(!speaker.audio.ready() && !speaker.audio.start(Track::NORMAL));
    assert(speaker.snapshot()["installed"]["normal"]["id"] == "");
  }
  complete_boot(); const auto operations = flash_operations; const auto reboots = esphome::App.reboots;
  {
    Speaker speaker; speaker.load(); speaker.select(1); speaker.tick();
    assert(speaker.state() == "ready" && !pending_worker && flash_operations == operations && esphome::App.reboots == reboots);
  }
}
void flash_cuts() {
  reset();
  { Speaker speaker; speaker.load(); speaker.select(); speaker.download(); speaker.install(); }
  const auto count = flash_operations;
  for (int cut = 1; cut <= count; ++cut) {
    reset();
    {
      Speaker speaker; speaker.load(); speaker.select(); speaker.download(); cut_flash = cut;
      try { speaker.install(); assert(false); } catch (const PowerCut &) {}
    }
    preserved(); cut_flash = -1; pending.clear();
    {
      Speaker boot; boot.tick(true, true, true, false);
      if (pending_worker) {
        assert(!boot.athan.health && boot.state() == "downloading"); run_worker();
        boot.player.drain_commands(); boot.tick(true, true, true, false); assert(pending_worker); run_worker(); boot.tick();
      }
    }
    complete_boot();
  }
  std::cout << "Athan media recovered from " << count << " torn erase, payload, and header operations\n";
}
void boundary_pair() {
  normal_data.assign(0x180001, 'n');
  const size_t offset = (4096 + normal_data.size() + 15) & ~size_t(15);
  fajr_data.assign(DUA_BANK_START - offset, 'f');
  reset();
  { Speaker speaker; speaker.load(); speaker.select(); speaker.download(); speaker.install(); }
  const int operations = flash_operations;
  assert(read_u32(flash.data() + 8) == DUA_BANK_START && read_u32(flash.data() + 68) == offset);
  complete_boot();
  for (int cut : {1, 384, 768, 769, operations / 2 + 100, operations - 1, operations}) {
    reset();
    {
      Speaker speaker; speaker.load(); speaker.select(); speaker.download(); cut_flash = cut;
      try { speaker.install(); assert(false); } catch (const PowerCut &) {}
    }
    preserved(); cut_flash = -1;
    {
      Speaker boot; boot.tick(true, true, true, false);
      if (pending_worker) {
        run_worker(); boot.player.drain_commands(); boot.tick(true, true, true, false); assert(pending_worker); run_worker(); boot.tick();
      }
    }
    complete_boot();
  }
  std::cout << "Athan pair ending exactly at 0x300000 passed all " << operations << " operations and seven sampled cuts\n";
  normal_data.assign(4097, 'n'); fajr_data.assign(8193, 'f');
  reset();
  { Speaker speaker; speaker.load(); speaker.select(); speaker.download(); speaker.install(); }
  const auto header_operation = flash_operations;
  for (size_t prefix : {8, 112, 128, 143, 144, 4095}) {
    reset();
    {
      Speaker speaker; speaker.load(); speaker.select(); speaker.download(); cut_flash = header_operation; cut_header_prefix = prefix;
      try { speaker.install(); assert(false); } catch (const PowerCut &) {}
    }
    cut_flash = -1; preserved();
    {
      Speaker boot; boot.tick(true, true, true, false);
      if (pending_worker) { run_worker(); boot.player.drain_commands(); boot.tick(true, true, true, false); run_worker(); boot.tick(); }
    }
    complete_boot();
  }
  std::cout << "Athan header recovered from six cuts around metadata and checksum boundaries\n";
}
void nvs_faults() {
  for (int cut = 1; cut <= 4; ++cut) {
    reset(); cut_commit = cut;
    try {
      Speaker speaker; speaker.load(); speaker.select(); speaker.download(); speaker.install();
      complete_boot(); assert(false);
    } catch (const PowerCut &) {}
    preserved(); cut_commit = -1; pending.clear();
    {
      Speaker boot; boot.tick();
      for (unsigned i = 0; i < 8 && boot.state() != "ready" && !esphome::App.reboots; ++i) {
        if (pending_worker) run_worker(); boot.player.drain_commands(); boot.tick();
      }
    }
    complete_boot();
  }
  for (bool committed : {false, true}) {
    reset();
    {
      Speaker speaker; speaker.load(); speaker.select(); speaker.download(); fail_commit = 2; commit_then_fail = committed;
      speaker.tick(); assert(flash_operations == 0 && speaker.state() == "unavailable" && speaker.media.owns_storage());
    }
  }
  reset();
  {
    Speaker speaker; speaker.load(); speaker.select(); speaker.download(); fail_nvs_readback = true; speaker.tick();
    assert(!flash_operations && speaker.state() == "unavailable" && speaker.media.owns_storage());
  }
}
void prewrite_failures() {
  for (unsigned failure = 0; failure < 8; ++failure) {
    reset();
    {
      Speaker speaker; speaker.load(); speaker.select();
      if (failure == 0) signature_valid = false;
      if (failure == 1) http_replies[VERSION + "normal.mp3"].data = "short";
      if (failure == 2) http_replies[VERSION + "fajr.mp3"].data[0] ^= 1;
      if (failure == 3) spiram_allocation_fails = true;
      if (failure == 4) http_replies[VERSION + "catalog.json"].data = catalog([](JsonObject root) { root["recordings"][0]["name"] = "Changed immutable catalog"; });
      if (failure == 5) http_replies[VERSION + "normal.mp3"].data += "long";
      if (failure == 6) http_replies[VERSION + "normal.mp3"].read_steps = {-1};
      if (failure == 7) http_replies[VERSION + "normal.mp3"] = {302, "http://github.com/insecure", ""};
      speaker.tick(); assert(pending_worker); run_worker();
      assert(speaker.state() == "failed" && speaker.snapshot()["can_retry"] == true && !flash_operations && flash == original && speaker.audio.ready());
      signature_valid = true; spiram_allocation_fails = false;
      http_replies[VERSION + "catalog.json"] = {200, "", catalog()};
      http_replies[VERSION + "normal.mp3"] = {200, "", normal_data}; http_replies[VERSION + "fajr.mp3"] = {200, "", fajr_data};
      assert(speaker.action("retry") == 202); speaker.download(); speaker.install();
    }
    complete_boot();
  }
}
void catalog_failures() {
  std::vector<std::function<void(JsonObject)>> mutations{
    [](JsonObject root) { root["extra"] = true; },
    [](JsonObject root) { root["schema"] = 2; },
    [](JsonObject root) { root["recordings"][0]["id"] = "bad/id"; },
    [](JsonObject root) { root["recordings"][0]["role"] = "fajr"; },
    [](JsonObject root) { root["recordings"][0]["file"] = "../normal.mp3"; },
    [](JsonObject root) { root["recordings"][0]["bytes"] = DUA_BANK_START; },
    [](JsonObject root) { root["recordings"][0]["bytes"] = DUA_BANK_START - 4096; },
    [](JsonObject root) { root["recordings"][0]["name"] = std::string(81, 'x'); },
    [](JsonObject root) { root["recordings"][0]["sha256"] = "invalid"; },
    [](JsonObject root) { root["recordings"][1]["id"] = "normal-one"; },
  };
  for (const auto &mutate : mutations) {
    reset(); http_replies[BASE + "latest/download/catalog.json"].data = catalog(mutate);
    Speaker speaker; assert(speaker.action("catalog") == 202); speaker.tick(); run_worker();
    assert(speaker.snapshot()["catalog"]["state"] == "failed" && !flash_operations && !commits);
  }
}
void ownership_and_cancel() {
  reset();
  {
    Speaker speaker; speaker.load(); speaker.select();
    speaker.tick(false); assert(!pending_worker && speaker.audio.ready());
    speaker.tick(true, false); assert(!pending_worker);
    speaker.tick(true, true, false); assert(!pending_worker);
    speaker.tick(true, true, true, false); assert(!pending_worker);
    speaker.athan.clock.valid = false; speaker.tick(); assert(!pending_worker);
    speaker.athan.clock.valid = true; speaker.athan.sample.next->utc = 1200; speaker.tick(); assert(!pending_worker);
    speaker.athan.sample.next->utc = 9000;
    assert(speaker.audio.start(Track::NORMAL)); speaker.tick(); assert(!pending_worker);
    speaker.audio.stop(); speaker.player.drain_commands();
    on_each_read = [&] { assert(speaker.action("cancel") == 202); on_each_read = {}; };
    speaker.tick(); run_worker(); speaker.tick();
    assert(!pending_worker && !flash_operations && flash == original && speaker.audio.ready() && speaker.snapshot()["revision"] == 2);
    assert(speaker.action("selection", R"({"schema":1,"expected_revision":1,"normal":"normal-one","fajr":"fajr-one"})") == 409);
    speaker.select(2); speaker.download();
    speaker.player.pending_commands.push_back("play"); speaker.tick(); assert(!speaker.media.owns_storage() && !pending_worker);
    speaker.player.drain_commands(); speaker.player.pipeline_state = esphome::speaker::AudioPipelineState::PLAYING;
    speaker.tick(); assert(!speaker.media.owns_storage()); speaker.player.pipeline_state = esphome::speaker::AudioPipelineState::STOPPED;
    on_flash = [&] {
      speaker.tick(false, false, false, false);
      assert(speaker.action("cancel") == 409 && !speaker.audio.start(Track::NORMAL));
    };
    speaker.install();
  }
  complete_boot();
}
void repair_guards() {
  reset();
  {
    Speaker speaker; speaker.load(); speaker.select(); speaker.download(); cut_flash = 2;
    try { speaker.install(); assert(false); } catch (const PowerCut &) {}
  }
  cut_flash = -1; const auto partial = flash; const auto journal = durable;
  for (unsigned guard = 0; guard < 4; ++guard) {
    flash = partial; durable = journal;
    running_state = guard == 0 ? ESP_OTA_IMG_PENDING_VERIFY : ESP_OTA_IMG_VALID;
    boot_partition = guard == 1 ? &inactive : &running;
    boot_query_fails = guard == 2; firmware_identity = guard == 3 ? 0x6b : 0x5a;
    Speaker boot; const auto before = flash_operations; boot.tick(true, true, true, false);
    assert(!pending_worker && flash_operations == before && boot.state() == "repair_needed");
  }
  running_state = ESP_OTA_IMG_VALID; boot_partition = &running; boot_query_fails = false; firmware_identity = 0x5a;
  {
    Speaker boot; assert(!boot.audio.ready() && !boot.athan.health); boot.tick(true, true, false, false); assert(!pending_worker);
    boot.tick(true, true, true, false); assert(pending_worker); run_worker();
    boot.player.drain_commands(); boot.tick(true, true, true, false); run_worker(); boot.tick();
  }
  complete_boot();
  reset();
  { Speaker speaker; speaker.load(); speaker.select(); }
  firmware_identity = 0x7c;
  { Speaker boot; boot.download(); boot.install(); }
  complete_boot();
}
void startup_and_storage_failures() {
  for (unsigned failure = 0; failure < 3; ++failure) {
    reset();
    {
      Speaker speaker; speaker.load(); speaker.select(); speaker.download();
      speaker.tick(); speaker.player.drain_commands();
      if (failure == 0) task_available = false;
      speaker.tick();
      if (failure == 1) read_buffer_allocation_fails = true;
      if (failure == 2) fail_read = reads + 1;
      if (pending_worker) run_worker();
      assert(speaker.state() == "repair_needed" && speaker.snapshot()["can_retry"] == true && speaker.media.owns_storage());
      if (failure < 2) assert(flash == original && !flash_operations);
      task_available = true; read_buffer_allocation_fails = false; fail_read = -1;
      assert(speaker.action("retry") == 202); speaker.tick(); assert(pending_worker); run_worker(); speaker.tick();
    }
    complete_boot();
  }
  reset();
  { Speaker speaker; speaker.load(); speaker.select(); speaker.download(); speaker.install(); }
  {
    Speaker boot; boot.athan.health = false; boot.tick(); assert(boot.state() == "restarting");
    now_us = 90000000; boot.tick(); assert(boot.state() == "failed" && boot.media.owns_storage() && !boot.snapshot()["can_retry"].as<bool>());
    const auto before = commits; boot.athan.health = true; fail_nvs_readback = true; boot.tick();
    assert(boot.state() == "unavailable" && boot.snapshot()["supported"] == true && boot.media.owns_storage() && commits == before + 1);
  }
  fail_nvs_readback = false; complete_boot();
  for (bool corrupt : {false, true}) {
    reset(); std::fill(flash.begin() + DUA_BANK_START, flash.end(), 0xff);
    if (corrupt) flash[DUA_BANK_START + 200] = 0;
    const auto protected_tail = std::vector<uint8_t>(flash.begin() + DUA_BANK_START, flash.end());
    { Speaker speaker; speaker.load(); speaker.select(); speaker.download(); speaker.install(); }
    Speaker boot; boot.tick(); assert(boot.state() == "ready" && boot.audio.ready() && !boot.audio.dua_ready());
    assert(std::equal(protected_tail.begin(), protected_tail.end(), flash.begin() + DUA_BANK_START));
  }
  reset();
  { Speaker speaker; speaker.load(); speaker.select(); speaker.download(); speaker.install(); }
  flash[4096] ^= 1;
  {
    Speaker boot; boot.tick(true, true, true, false); assert(pending_worker); run_worker();
    boot.player.drain_commands(); boot.tick(true, true, true, false); assert(pending_worker); run_worker(); boot.tick();
  }
  complete_boot();
}
void canonical_catalog(const char *path) {
  reset(); std::ifstream input(path); const std::string encoded{std::istreambuf_iterator<char>(input), {}};
  http_replies[BASE + "latest/download/catalog.json"].data = encoded;
  http_replies[VERSION + "catalog.json"].data = encoded;
  {
    Speaker speaker; speaker.load();
    assert(speaker.snapshot()["catalog"]["recordings"][0]["name"].as<std::string>() == "أذان \"الفجر\" 🌙");
    speaker.select(); speaker.download(); speaker.install();
  }
  complete_boot();
  for (unsigned mutation = 0; mutation < 4; ++mutation) {
    reset(); JsonDocument envelope; assert(!deserializeJson(envelope, catalog()));
    auto payload = envelope["payload"].as<std::string>();
    if (mutation == 0) payload.insert(1, " ");
    if (mutation == 1) payload.insert(1, "\"kind\":\"athan_catalog\",");
    if (mutation == 2) payload.replace(payload.find("Regular One"), 11, "\\u0052egular One");
    if (mutation == 3) payload.replace(payload.find("Regular One"), 11, "Bad\\u007fname");
    envelope["payload"] = payload; std::string invalid; serializeJson(envelope, invalid);
    http_replies[BASE + "latest/download/catalog.json"].data = invalid;
    Speaker speaker; assert(speaker.action("catalog") == 202); speaker.tick(); run_worker();
    assert(speaker.snapshot()["catalog"]["state"] == "failed" && !flash_operations);
  }
}
int main(int argc, char **argv) {
  assert(argc == 3); std::ifstream input(argv[1], std::ios::binary);
  original = {std::istreambuf_iterator<char>(input), {}}; assert(original.size() == AUDIO_PARTITION_SIZE);
#if defined(OPENATHAN_PROVISIONING_TEST_STORAGE) || (defined(OPENATHAN_UPDATES_ENABLED) && !OPENATHAN_UPDATES_ENABLED)
  reset(); { Speaker speaker; assert(speaker.action("catalog") == 503); speaker.tick(); assert(!pending_worker && !flash_operations && !commits); }
  std::cout << "Isolated and disabled profiles cannot replace production Athan audio\n"; return 0;
#endif
  normal_and_noop(); flash_cuts(); nvs_faults(); prewrite_failures(); catalog_failures(); ownership_and_cancel(); repair_guards();
  startup_and_storage_failures(); boundary_pair(); canonical_catalog(argv[2]);
  assert(!pending_worker && !live_http_clients && !live_read_buffers);
  std::cout << "Athan production service passed durable recovery, reboot rebinding, dua preservation, trust, ownership, and cancellation checks\n";
}
