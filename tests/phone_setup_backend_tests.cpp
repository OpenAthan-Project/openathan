// The HTTP/admission/lifecycle/USB bodies are production source, extracted at
// test time. Only ESP Wi-Fi, clock, task delivery and unrelated services are
// adapters. Credential encoding, NVS commits and password verification are real.
#include "esphome/components/json/json_util.h"
#include "esphome/components/openathan/storage_config.h"
#include "credential_store.h"
#include "digest.h"
#include "protocol.h"
#include "wifi_attempt.h"
#include "phone_setup.h"
#include "upgrade_policy.h"
#include "test_maintenance.h"
#include "openathan/usb_upgrade.h"
#include "openathan/scheduler.h"
#include "nvs_memory.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>

int64_t now_us{};
int64_t esp_timer_get_time() { return now_us; }
void esp_fill_random(void *buffer, size_t size) { std::memset(buffer, 0xAB, size); }
void xSemaphoreGive(void *) {}
namespace esphome {
using StringRef = std::string;
struct AppProbe { bool complete{true}; bool is_setup_complete() const { return complete; } } App;
namespace wifi {
constexpr unsigned SSID_BUFFER_SIZE = 33;
struct WiFiAP {
  std::string ssid, password;
  void set_ssid(const std::string &value) { ssid=value; }
  void set_password(const std::string &value) { password=value; }
  void set_hidden(bool) {}
};
struct WiFiProbe {
  bool ap{}, connected{}, enabled{true};
  std::string ssid, ip{"192.168.1.42"};
  WiFiAP access_point, candidate;
  unsigned scans{}, joins{}, disables{};
  bool is_ap_active() const { return ap; }
  bool is_connected() const { return connected; }
  void disable() { ++disables; ap=connected=enabled=false; }
  void clear_sta() { candidate={}; }
  void set_sta(const WiFiAP &value) { candidate=value; }
  void set_ap(const WiFiAP &value) { access_point=value; }
  void enable() {
    if (enabled) return;  // Pinned WiFiComponent::enable() is idempotent.
    enabled=true;
    ap=candidate.ssid.empty() && !access_point.ssid.empty();
    if (!candidate.ssid.empty()) ++joins;
  }
  void start_scanning() { ++scans; }
  void start_connecting(const WiFiAP &value) { candidate=value; ++joins; ap=false; connected=false; }
  const char *wifi_ssid_to(char *out) const { std::strcpy(out,ssid.c_str()); return out; }
} wifi_probe;
auto *global_wifi_component=&wifi_probe;
}
namespace openathan_component {
struct SettingsProbe { bool ok{true}; bool healthy() const { return ok; } };
struct OpenAthan {
  bool incomplete{true},busy{}; SettingsProbe settings;
  const char *setup_state() const { return incomplete ? "incomplete" : "active"; }
  SettingsProbe *settings_service() { return &settings; }
  bool playback_busy() const { return busy; }
  bool activated() const { return !incomplete; }
  struct Sample { bool automatic_ready{true}; std::optional<::openathan::Event> next; };
  Sample status() const { return {}; }
  ::openathan::ClockSample read() const { return {true, 1000, 0, {2026, 10, 18}, 0}; }
  void quiesce_for_maintenance() {}
};
}
namespace openathan_device {
struct HttpExchange {
  std::string method{"GET"},uri{"/setup/status"},body,response;
  std::string host{"192.168.4.1"}, origin, site{"same-origin"}, content_type{"application/json"}, local_ip{"192.168.4.1"};
  std::string authorization,challenge,type{"application/json"},redirect;
  bool gzip{},phone_connect{},cancelled{};
  int code{200}; void *done{};
  std::atomic<unsigned> response_result{};
  std::mutex mutex;
};
struct UpgradeProbe {
  bool busy{},blocked{},working{},background_busy{}; unsigned commands{};
  bool usb_busy() const { return busy; }
  bool streaming_blocked() const { return busy || blocked || working; }
  bool active() const { return working; }
  void set_background_busy(bool value) { background_busy=value; }
  bool confirmed() const { return true; }
  void loop(bool) {}
  std::vector<std::string> usb_info() { return {"info"}; }
  uint8_t usb_command(uint8_t,const std::vector<std::string>&,std::vector<std::string>&) { ++commands; return 0; }
  bool usb_chunk(const ::openathan::usb_upgrade::Chunk &) { return false; }
};
struct DuaProbe {
  bool running{},start_next{};
  bool active() const { return running; }
  void shutdown() {}
  void loop(bool, bool permitted) { if(start_next && permitted) { running=true;start_next=false; } }
};
struct QuranProbe { bool playback_pending() const { return false; } void loop(bool,bool) {} };
struct ApiProbe {
  unsigned calls{};
  void set_context(const std::string &,bool) {}
  void handle(HttpExchange &request) { ++calls; request.code=200; }
};
struct Device {
  void loop(); void serial_request(const Frame &);
  void on_wifi_connect_state(StringRef,std::span<const uint8_t,6>);
  void handle_http_(HttpExchange &); bool handle_phone_http_(HttpExchange &);
  bool valid_host_(const std::string &) const;
  void phone_loop_(uint64_t); bool phone_eligible_() const; bool phone_setup_active() const;
  void restore_wifi_(); uint8_t wifi_state_() const;
  void send_(bool,uint8_t,const std::vector<uint8_t>&);
  void result_(bool,uint8_t,const std::vector<std::string>&);
  void phone_dns_() {} // UDP codec and interface binding are separate checks.
  void phone_stop_dns_() { ++dns_stops; }
  bool connected() const { return wifi::wifi_probe.connected; }
  std::string ipv4_address() const { return wifi::wifi_probe.connected ? wifi::wifi_probe.ip : ""; }
  std::string ip_() const { return wifi::wifi_probe.ip; }
  std::vector<std::string> urls_() const { return {"http://"+hostname_+"/", "http://"+ip_()+"/"}; }
  openathan_component::OpenAthan *athan_{};
  CredentialStore store_; WifiCredentials saved_wifi_,phone_candidate_; WifiAttempt wifi_attempt_;
  PasswordVerifier password_; DigestAuth auth_;
  ::openathan::LoadResult wifi_record_{::openathan::LoadResult::EMPTY},password_record_{::openathan::LoadResult::EMPTY};
  std::string realm_{"OpenAthan-test"},hostname_{"openathan-a1b2c3.local"};
  std::function<void(const std::vector<uint8_t>&)> sender_;
  bool maintenance_{},scanning_{},phone_scan_{},phone_attempt_{},phone_session_{},phone_initialized_{};
  uint64_t scan_deadline_{},auth_window_{},phone_response_at_{},phone_auth_window_{};
  unsigned auth_attempts_{},phone_auth_attempts_{},dns_stops{};
  phone_setup::State phone_state_{phone_setup::State::CLOSED};
  std::string phone_ssid_{"OpenAthan-Setup-a1b2c3"},phone_error_;
  struct Network { std::string ssid; int rssi; bool secured; }; std::vector<Network> phone_networks_;
  struct Asset { const uint8_t *data{};size_t size{};const char *type{}; } assets_[6];
  std::shared_ptr<HttpExchange> phone_response_;
  DuaProbe dua_;
  bool playback_busy() const { return athan_->playback_busy(); }
  UpgradeProbe upgrade_; QuranProbe quran_; ApiProbe api_storage; ApiProbe *api_{&api_storage};
  std::mutex queue_mutex_; std::deque<std::shared_ptr<HttpExchange>> requests_;
};
namespace {
#include "phone_device_helpers.inc"
}
#include "phone_http.inc"
#include "phone_device_methods.inc"
}
}
using namespace esphome::openathan_device;
using openathan::LoadResult;

struct Fixture {
  esphome::openathan_component::OpenAthan athan;
  Device device;
  std::vector<std::vector<uint8_t>> sent;
  explicit Fixture(bool start=true) {
    nvs_test::reset(); now_us=0; esphome::wifi::wifi_probe={}; esphome::App.complete=true;
    nvs_test::committed[{"openathan","settings"}]={1,2,3};
    nvs_test::committed[{"openathan","scheduler"}]={4,5,6};
    device.athan_=&athan;
    device.sender_=[this](const auto &bytes){ sent.push_back(bytes); };
    static const uint8_t asset[]{'s','e','t','u','p'};
    for (auto &entry:device.assets_) entry={asset,sizeof(asset),"text/plain"};
    if (start) { device.loop(); assert(device.phone_setup_active()); }
  }
  std::shared_ptr<HttpExchange> call(const char *method,const char *path,const std::string &body="",const char *local="192.168.4.1") {
    auto request=std::make_shared<HttpExchange>();request->method=method;request->uri=path;request->body=body;request->local_ip=local;
    if (request->method=="POST") request->origin="http://192.168.4.1";
    device.requests_.push_back(request); device.loop(); return request;
  }
  std::shared_ptr<HttpExchange> connect(const char *password="device password",const char *wifi_password="wifi password") {
    JsonDocument input;input["password"]=password;input["ssid"]="Home";input["wifi_password"]=wifi_password;
    std::string body;serializeJson(input,body);return call("POST","/setup/connect",body);
  }
  void sent_response(const std::shared_ptr<HttpExchange> &request,unsigned result=1) {
    request->response_result=result;device.loop();
  }
  void join() {
    auto &wifi=esphome::wifi::wifi_probe;wifi.connected=true;wifi.ssid="Home";
    std::array<uint8_t,6> bssid{};device.on_wifi_connect_state("Home",bssid);device.loop();
  }
  void preserved() const {
    assert(nvs_test::committed.at({"openathan","settings"})==std::vector<uint8_t>({1,2,3}));
    assert(nvs_test::committed.at({"openathan","scheduler"})==std::vector<uint8_t>({4,5,6}));
  }
};

void admissions() {
  Fixture f;
  assert(f.call("GET","/setup/")->code==200);
  auto probe=f.call("GET","/generate_204");assert(probe->code==302 && probe->redirect==phone_setup::URL);
  const auto writes=nvs_test::writes;
  assert(f.call("POST","/setup/connect","{}","192.168.1.42")->code==403);
  auto cross=std::make_shared<HttpExchange>();cross->method="POST";cross->uri="/setup/scan";cross->body="{}";cross->origin="http://other.test";
  f.device.requests_.push_back(cross);f.device.loop();assert(cross->code==403 && cross->body.empty());
  f.device.upgrade_.busy=true;assert(f.call("POST","/setup/scan","{}")->code==403);f.device.upgrade_.busy=false;
  f.device.phone_session_=true;esphome::wifi::wifi_probe.ap=true;
  esphome::wifi::wifi_probe.connected=true;
  assert(f.call("POST","/setup/scan","{}")->code==403);
  esphome::wifi::wifi_probe.connected=true;esphome::wifi::wifi_probe.ip="192.168.4.1";
  assert(f.call("POST","/setup/scan","{}")->code==403);
  assert(nvs_test::writes==writes && f.device.api_storage.calls==0);f.preserved();
}
void closed_startup() {
  { Fixture f(false);f.athan.incomplete=false;f.device.loop();assert(!f.device.phone_setup_active()); }
  { Fixture f(false);f.device.wifi_record_=LoadResult::LOADED;f.device.loop();assert(!f.device.phone_setup_active()); }
  { Fixture f(false);f.device.wifi_record_=LoadResult::ERROR;f.device.loop();assert(!f.device.phone_setup_active()); }
  { Fixture f(false);f.device.password_record_=LoadResult::ERROR;f.device.loop();assert(!f.device.phone_setup_active()); }
  { Fixture f(false);f.athan.settings.ok=false;f.device.loop();assert(!f.device.phone_setup_active()); }
}
void busy_admission() {
  for (unsigned kind=0;kind<3;++kind) {
    Fixture f(false);
    if(kind==0)f.athan.busy=true; // Includes the silent pause.
    if(kind==1)f.device.dua_.running=true;
    if(kind==2)f.device.upgrade_.working=true;
    f.device.loop();assert(!f.device.phone_initialized_ && !f.device.phone_setup_active());
    const auto writes=nvs_test::writes;
    for(const auto &request:std::vector<Frame>{{false,3,rpc(1,{"Other","other password"})},
        {false,3,rpc(4,{})},{true,3,rpc(2,{"replacement password"})}}) {
      f.sent.clear();f.device.serial_request(request);
      assert(f.sent.size()==1 && f.sent[0]==frame(request.extension,2,std::vector<uint8_t>{255}));
      assert(nvs_test::writes==writes && !f.device.wifi_attempt_.active() && !f.device.scanning_);
    }
    f.athan.busy=false;f.device.dua_.running=false;f.device.upgrade_.working=false;
    f.device.loop();assert(f.device.phone_initialized_ && f.device.phone_setup_active());f.preserved();
  }
  // The HTTP request must see a dua worker started in this same iteration.
  Fixture f(false);f.athan.incomplete=false;f.device.dua_.start_next=true;
  f.call("GET","/api/status");
  assert(f.device.dua_.running && f.device.upgrade_.background_busy);
  // A queued update must not trap an owner who needs USB to repair Wi-Fi.
  Fixture queued(false);queued.athan.incomplete=false;queued.device.upgrade_.blocked=true;
  queued.device.serial_request({false,3,rpc(1,{"Home","wifi password"})});
  assert(queued.device.wifi_attempt_.active());queued.device.loop();
  assert(queued.device.upgrade_.background_busy && esphome::wifi::wifi_probe.joins==1);queued.preserved();
}
bool usb_before_first_loop() {
  bool passed=true;
  for (bool succeeds : {false,true}) {
    Fixture f(false);
    f.device.serial_request({false,3,rpc(1,{"Home","wifi password"})});
    assert(f.device.wifi_attempt_.active());
    f.device.loop();
    if (f.device.phone_initialized_ || f.device.phone_setup_active() || esphome::wifi::wifi_probe.joins!=1) {
      std::cerr << "USB Wi-Fi before first Device::loop: initialized=" << f.device.phone_initialized_
                << ", AP=" << esphome::wifi::wifi_probe.ap << ", joins=" << esphome::wifi::wifi_probe.joins << '\n';
      passed=false;continue;
    }
    if (succeeds) {
      f.join();f.device.loop();
      assert(f.device.wifi_record_==LoadResult::LOADED && f.device.phone_initialized_ && !f.device.phone_setup_active());
    } else {
      now_us=30000000;f.device.loop();f.device.loop();
      assert(f.device.wifi_record_==LoadResult::EMPTY && f.device.phone_initialized_ && f.device.phone_setup_active());
    }
    f.preserved();
  }
  {
    Fixture f(false);
    f.device.serial_request({false,3,rpc(4,{})});
    assert(f.device.scanning_);
    f.device.loop();
    if (f.device.phone_initialized_ || f.device.phone_setup_active()) {
      std::cerr << "USB scan before first Device::loop: initialized=" << f.device.phone_initialized_
                << ", AP=" << esphome::wifi::wifi_probe.ap << ", scanning=" << f.device.scanning_ << '\n';
      passed=false;
    } else {
      now_us=10000000;f.device.loop();f.device.loop();
      assert(!f.device.scanning_ && f.device.phone_initialized_ && f.device.phone_setup_active());
      f.preserved();
    }
  }
  {
    Fixture f(false);f.device.upgrade_.busy=true;f.device.loop();
    if (f.device.phone_initialized_ || f.device.phone_setup_active()) {
      std::cerr << "USB update before first Device::loop consumed the initializer\n";
      passed=false;
    } else {
      f.device.upgrade_.busy=false;f.device.loop();
      assert(f.device.phone_initialized_ && f.device.phone_setup_active());f.preserved();
    }
  }
  return passed;
}
void success_and_usb_race() {
  Fixture f;auto request=f.connect();assert(request->code==202 && request->body.empty());
  assert(nvs_test::commits==1 && f.device.password_record_==LoadResult::LOADED);
  assert(esphome::wifi::wifi_probe.joins==0 && !f.device.wifi_attempt_.active());
  const auto writes=nvs_test::writes;
  f.device.serial_request({true,3,rpc(2,{"replacement password"})});
  f.device.serial_request({false,3,rpc(1,{"Other","other password"})});
  f.device.serial_request({true,3,rpc(openathan::usb_upgrade::BEGIN,{})});
  assert(nvs_test::writes==writes && f.device.upgrade_.commands==0);
  f.sent_response(request);assert(esphome::wifi::wifi_probe.joins==1 && f.device.phone_attempt_);
  assert(!esphome::wifi::wifi_probe.ap && f.device.phone_setup_active());
  assert(f.call("POST","/setup/scan","{}")->code==403);
  f.join();assert(f.device.wifi_record_==LoadResult::LOADED && !f.device.phone_session_ && !f.device.phone_setup_active());
  assert(nvs_test::commits==2);
  CredentialStore read;WifiCredentials saved;PasswordVerifier password;
  assert(read.load_wifi(saved)==LoadResult::LOADED && saved.ssid=="Home" && saved.password=="wifi password");
  assert(read.load_password(password)==LoadResult::LOADED && password.ha1==md5_hex("admin:OpenAthan-test:device password"));
  assert(f.call("POST","/setup/connect","{}")->code==403);f.preserved();
}
void failed_join_and_existing_password() {
  Fixture f;auto request=f.connect();f.sent_response(request);
  now_us=30000000;f.device.loop();assert(f.device.phone_setup_active() && f.device.phone_state_==phone_setup::State::FAILED);
  assert(f.device.wifi_record_==LoadResult::EMPTY && nvs_test::commits==1);
  const auto stored=nvs_test::committed;const auto writes=nvs_test::writes;
  auto wrong=f.connect("wrong device password");assert(wrong->code==403 && wrong->body.empty());
  assert(nvs_test::committed==stored && nvs_test::writes==writes);
  auto retry=f.connect();assert(retry->code==202 && nvs_test::writes==writes);f.sent_response(retry);f.join();
  assert(f.device.wifi_record_==LoadResult::LOADED);f.preserved();
}
void interruptions_and_storage_faults() {
  { Fixture f;auto request=f.connect();f.sent_response(request,2);
    assert(!f.device.wifi_attempt_.active() && esphome::wifi::wifi_probe.joins==0 && f.device.phone_setup_active());f.preserved(); }
  { Fixture f;auto request=f.connect();now_us=6000000;f.device.loop();
    assert(!f.device.phone_response_ && !f.device.wifi_attempt_.active() && f.device.phone_state_==phone_setup::State::FAILED);f.preserved(); }
  { Fixture f;nvs_test::fail_commit=true;auto request=f.connect();
    assert(request->code==503 && !f.device.phone_attempt_ && f.device.phone_state_==phone_setup::State::STORAGE_FAULT);
    assert(esphome::wifi::wifi_probe.joins==0);assert(f.call("POST","/setup/scan","{}")->code==503);f.preserved(); }
  { Fixture f;auto request=f.connect();f.sent_response(request);nvs_test::fail_commit=true;f.join();
    assert(f.device.phone_setup_active() && f.device.phone_state_==phone_setup::State::STORAGE_FAULT && f.device.wifi_record_==LoadResult::EMPTY);
    assert(f.call("GET","/setup/status")->code==200 && f.call("POST","/setup/scan","{}")->code==503);f.preserved(); }
  { Fixture f;f.athan.incomplete=false;f.device.loop();assert(!f.device.phone_session_ && !f.device.phone_setup_active());f.preserved(); }
}
int main() {
  if (!usb_before_first_loop()) return 1;
  closed_startup();busy_admission();admissions();success_and_usb_race();failed_join_and_existing_password();interruptions_and_storage_faults();
  std::cout << "Production phone HTTP, AP admission, password protection, deferred join, storage faults and USB exclusion passed\n";
}
