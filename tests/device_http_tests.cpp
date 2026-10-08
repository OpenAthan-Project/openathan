// Exercise the unchanged production HTTP entrypoint and Digest verifier with
// the real USB updater. LocalApi dispatch is observed here; its payload handling
// and playback/storage effects are exercised by settings_adapter_tests.cpp.
#define main existing_ota_tests
#include "upgrade_runtime_tests.cpp"
#undef main
#include "digest.h"
#include <cstdio>

void esp_fill_random(void *buffer, size_t size) {
  static uint8_t nonce{};
  auto *bytes = static_cast<uint8_t *>(buffer);
  for (size_t i = 0; i < size; ++i) bytes[i] = ++nonce;
}
namespace esphome::wifi {
struct WifiProbe { bool is_connected() const { return true; } };
inline WifiProbe wifi_probe;
inline WifiProbe *global_wifi_component = &wifi_probe;
}
namespace esphome::openathan_device {
// HTTP-task semaphore/cancellation plumbing is outside handle_http_.
struct HttpExchange {
  std::string method, uri, body, response, host, origin, site, authorization, content_type, challenge;
  std::string type{"application/json"};
  int code{200};
  bool gzip{};
};
struct ApiProbe {
  openathan_component::OpenAthan &athan;
  unsigned calls{}, stop_calls{};
  void set_context(const std::string &host, bool connected) {
    assert(host == "device.local" && connected);
  }
  void handle(HttpExchange &request) {
    ++calls;
    if (request.method == "POST") {
      assert(request.uri == "/api/stop" && request.body == "{}");
      ++stop_calls;
      athan.sample.playing = false;
    }
    JsonDocument status;
    status["playing"] = athan.sample.playing;
    serializeJson(status, request.response);
  }
};
struct Device {
  Device(Upgrade &upgrade, openathan_component::OpenAthan &athan) : api_storage{athan}, upgrade_(upgrade) {
    auth_.configure("OpenAthan-test", md5_hex("admin:OpenAthan-test:test password"));
  }
  void handle_http_(HttpExchange &request);
  bool valid_host_(const std::string &host) const;
  std::string ip_() const { return "192.0.2.1"; }
  bool maintenance_{};
  std::string hostname_{"device.local"};
  DigestAuth auth_;
  uint64_t auth_window_{};
  unsigned auth_attempts_{};
  struct Asset { const uint8_t *data{}; size_t size{}; const char *type{}; } assets_[3];
  ApiProbe api_storage;
  ApiProbe *api_{&api_storage};
  Upgrade &upgrade_;
};
namespace {
#include "device_http_helpers.inc"
}
#include "device_http_handler.inc"
}

struct Client {
  esphome::openathan_device::Device &device;
  std::string nonce;
  unsigned count{};
  static HttpExchange request(const std::string &method, const std::string &uri) {
    HttpExchange exchange;
    exchange.method = method; exchange.uri = uri; exchange.body = "{}";
    exchange.host = "device.local"; exchange.origin = "http://device.local";
    exchange.site = "same-origin"; exchange.content_type = "application/json";
    return exchange;
  }
  explicit Client(esphome::openathan_device::Device &value) : device(value) {
    auto exchange = request("GET", "/api/status");
    device.handle_http_(exchange);
    assert(exchange.code == 401 && device.api_storage.calls == 0);
    const auto start = exchange.challenge.find("nonce=\"");
    assert(start != std::string::npos);
    nonce = exchange.challenge.substr(start + 7, 48);
  }
  HttpExchange signed_request(const std::string &method, const std::string &uri, bool advance = true) {
    if (advance) now_us += 1000000;
    auto exchange = request(method, uri);
    char nc[9]; std::snprintf(nc, sizeof(nc), "%08x", ++count);
    const std::string cnonce = "http-test-client";
    const auto response = md5_hex(md5_hex("admin:OpenAthan-test:test password") + ":" + nonce + ":" + nc +
                                 ":" + cnonce + ":auth:" + md5_hex(method + ":" + uri));
    exchange.authorization = "Digest username=\"admin\", realm=\"OpenAthan-test\", nonce=\"" + nonce +
        "\", uri=\"" + uri + "\", algorithm=MD5, qop=auth, nc=" + nc + ", cnonce=\"" + cnonce +
        "\", response=\"" + response + "\"";
    return exchange;
  }
  HttpExchange call(const std::string &method, const std::string &uri) {
    auto exchange = signed_request(method, uri);
    device.handle_http_(exchange);
    return exchange;
  }
};

void check_http(Upgrade &upgrade, esphome::openathan_component::OpenAthan &athan) {
  assert(upgrade.usb_busy());
  const auto before_state = state(upgrade), journal = saved_record;
  const auto commits = nvs_commits, erase_count = erases, write_count = writes, selections = boot_selections;
  const auto abort_count = aborts;
  auto *const selected = boot_partition;
  esphome::openathan_device::Device device(upgrade, athan);
  Client client(device);
  athan.sample.playing = true;
  const auto status = client.call("GET", "/api/status");
  assert(status.code == 200);
  JsonDocument payload; assert(!deserializeJson(payload, status.response) && payload["playing"] == true);
  auto stop = client.call("POST", "/api/stop");
  std::cout << before_state << ": authenticated Stop=" << stop.code << std::endl;
  assert(stop.code == 200 && device.api_storage.stop_calls == 1 && !athan.sample.playing);
  assert(!deserializeJson(payload, stop.response) && payload["playing"] == false);
  assert(client.call("POST", "/api/stop").code == 200 && device.api_storage.stop_calls == 2);

  const auto calls = device.api_storage.calls;
  for (const char *path : {"/api/settings", "/api/preview", "/api/activate", "/api/skip", "/api/cancel-skip",
       "/api/lights", "/api/display", "/api/time-format", "/api/firmware/check", "/api/firmware/install",
       "/api/firmware/cancel", "/api/stop/", "/api/stop?ignored=true", "/api/stop-extra"}) {
    assert(client.call("POST", path).code == 409);
  }
  assert(device.api_storage.calls == calls);
  athan.sample.playing = true;
  auto denied = [&](HttpExchange exchange, int code) {
    const auto before_calls = device.api_storage.calls;
    device.handle_http_(exchange);
    assert(exchange.code == code && athan.sample.playing && device.api_storage.calls == before_calls);
  };
  denied(Client::request("POST", "/api/stop"), 401);
  auto invalid = client.signed_request("POST", "/api/stop"); invalid.authorization = "Digest invalid"; denied(invalid, 401);
  invalid = client.signed_request("POST", "/api/stop"); invalid.host = "other.local"; denied(invalid, 403);
  invalid = client.signed_request("POST", "/api/stop"); invalid.origin = "http://other.local"; denied(invalid, 403);
  invalid = client.signed_request("POST", "/api/stop"); invalid.origin.clear(); invalid.site = "cross-site"; denied(invalid, 403);
  invalid = client.signed_request("POST", "/api/stop"); invalid.content_type = "text/plain"; denied(invalid, 403);
  denied(client.signed_request("PUT", "/api/stop"), 400);
  device.maintenance_ = true; denied(client.signed_request("POST", "/api/stop"), 503); device.maintenance_ = false;

  // A signed request cannot be replayed to stop a later playback.
  auto once = client.signed_request("POST", "/api/stop"); device.handle_http_(once); assert(once.code == 200);
  athan.sample.playing = true;
  auto replay = Client::request("POST", "/api/stop"); replay.authorization = once.authorization; denied(replay, 401);
  device.auth_.configure("OpenAthan-test", ""); denied(Client::request("POST", "/api/stop"), 503);

  esphome::openathan_device::Device limited(upgrade, athan); Client rapid(limited);
  for (unsigned i = 0; i < 20; ++i) {
    auto read = rapid.signed_request("GET", "/api/status", false); limited.handle_http_(read); assert(read.code == 200);
  }
  auto excess = rapid.signed_request("POST", "/api/stop", false); limited.handle_http_(excess);
  assert(excess.code == 429 && limited.api_storage.stop_calls == 0 && athan.sample.playing);
  assert(upgrade.usb_busy() && state(upgrade) == before_state && saved_record == journal);
  assert(nvs_commits == commits && erases == erase_count && writes == write_count && boot_selections == selections);
  assert(aborts == abort_count && boot_partition == selected && esphome::App.reboots == 0 && http_requests.empty());
}

using namespace ::openathan::usb_upgrade;
std::string descriptor(Upgrade &upgrade) {
  std::vector<std::string> reply;
  assert(upgrade.usb_command(BEGIN, {std::to_string(descriptor_response.size())}, reply) == 0);
  assert(reply.size() == 1);
  return reply[0];
}
uint64_t verified(Upgrade &upgrade) {
  const auto token_text = descriptor(upgrade);
  const auto token = std::stoull(token_text, nullptr, 16);
  for (size_t offset = 0; offset < descriptor_response.size(); offset += CHUNK) {
    const auto bytes = std::span(reinterpret_cast<const uint8_t *>(descriptor_response.data() + offset),
                                 std::min(CHUNK, descriptor_response.size() - offset));
    assert(upgrade.usb_chunk({token, 0, uint32_t(offset), bytes}));
  }
  std::vector<std::string> reply;
  assert(upgrade.usb_command(VERIFY, {token_text}, reply) == 0);
  return token;
}
void app_chunk(Upgrade &upgrade, uint64_t token, size_t offset) {
  const auto bytes = std::span(reinterpret_cast<const uint8_t *>(application_response.data() + offset),
                               std::min(CHUNK, application_response.size() - offset));
  assert(upgrade.usb_chunk({token, 1, uint32_t(offset), bytes}));
}
int main() {
  // An actual partial transfer leaves its journal intact across restart. Normal
  // scheduled playback can resume long after the interruption.
  reset(); esphome::openathan_component::OpenAthan athan;
  {
    Upgrade transfer; begin(transfer, athan);
    const auto token = verified(transfer); app_chunk(transfer, token, 0);
    assert(state(transfer) == "usb_receiving" && writes == 1);
  }
  {
    Upgrade restarted; begin(restarted, athan); now_us += 1000000000; restarted.loop(true);
    assert(state(restarted) == "usb_interrupted"); check_http(restarted, athan);
  }
  for (const char *stage : {"usb_descriptor", "usb_receiving", "usb_selection_uncertain", "awaiting_power"}) {
    reset(); esphome::openathan_component::OpenAthan current;
    Upgrade upgrade; begin(upgrade, current);
    if (std::string(stage) == "usb_descriptor") {
      descriptor(upgrade);
    } else {
      const auto token = verified(upgrade); app_chunk(upgrade, token, 0);
      if (std::string(stage) != "usb_receiving") {
        for (size_t offset = CHUNK; offset < application_response.size(); offset += CHUNK) app_chunk(upgrade, token, offset);
        selection_fails = selection_writes_before_failure = std::string(stage) == "usb_selection_uncertain";
        char text[17]; std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(token));
        std::vector<std::string> reply;
        assert(upgrade.usb_command(FINISH, {text}, reply) == (selection_fails ? 255 : 0));
      }
    }
    assert(state(upgrade) == stage); check_http(upgrade, current);
  }
  std::cout << "Production HTTP Stop access and USB ownership checks passed" << std::endl;
}
