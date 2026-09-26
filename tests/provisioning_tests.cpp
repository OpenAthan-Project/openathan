#include <cstdio>
#include <cstdlib>

#include "credential_store.h"
#include "digest.h"
#include "nvs_memory.h"
#include "protocol.h"
#include "setup_store.h"
#include "wifi_attempt.h"

#define CHECK(c)                                                   \
  do {                                                             \
    if (!(c)) {                                                    \
      std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); \
      std::abort();                                                \
    }                                                              \
  } while (0)
using namespace esphome::openathan_device;
using esphome::openathan_component::SetupStore;
using openathan::LoadResult;
void frames() {
  for (bool extension : {false, true}) {
    const auto data = rpc(extension ? 2 : 1, extension ? std::vector<std::string>{"a long test password"}
                                                       : std::vector<std::string>{"My network", "network-test"});
    const auto packet = frame(extension, 3, data);
    SerialParser parser;
    std::optional<Frame> decoded;
    for (char c : std::string("noise\nIMXX")) CHECK(!parser.feed(c, 1));
    for (auto byte : packet) {
      auto value = parser.feed(byte, 10);
      if (value) decoded = value;
    }
    CHECK(decoded && decoded->extension == extension && decoded->type == 3 && decoded->data == data);
    auto invalid = packet;
    invalid[invalid.size() - 2] ^= 1;
    for (auto byte : invalid) CHECK(!parser.feed(byte, 11));
    for (size_t i = 0; i < packet.size(); ++i) CHECK(!parser.feed(packet[i], i < 7 ? 20 : 3000));
    decoded.reset();
    for (auto byte : packet) {
      auto value = parser.feed(byte, 4000);
      if (value) decoded = value;
    }
    CHECK(decoded);
  }
  uint8_t command;
  std::vector<std::string> fields;
  CHECK(!parse_rpc({1, 3, 6, 1, 2}, command, fields));
  CHECK(!parse_rpc({1, 1}, command, fields));
  CHECK(!parse_rpc({1, 2, 1, 0}, command, fields));
  CHECK(parse_rpc(rpc(4, {}), command, fields) && command == 4 && fields.empty());
  CHECK(frame(false, 3, std::vector<uint8_t>(256)).empty());
  CHECK(rpc(2, {std::string(254, 'a')}).empty());
  CHECK(valid_wifi("hidden network", "correct horse"));
  CHECK(!valid_wifi(std::string(33, 'a'), "correct horse"));
  CHECK(!valid_wifi("network", "short"));
  CHECK(!valid_password("short") && valid_password("a longer password") && !valid_password(std::string(129, 'a')));
}
std::string authorization(const std::string& nonce, const std::string& count = "00000001",
                          const std::string& cnonce = "browser", const std::string& method = "POST",
                          const std::string& uri = "/api/settings",
                          const std::string& verifier = md5_hex("admin:device:a longer password")) {
  const auto response =
      md5_hex(verifier + ":" + nonce + ":" + count + ":" + cnonce + ":auth:" + md5_hex(method + ":" + uri));
  return "Digest username=\"admin\", realm=\"device\", nonce=\"" + nonce + "\", uri=\"" + uri +
         "\", algorithm=MD5, qop=auth, nc=" + count + ", cnonce=\"" + cnonce + "\", response=\"" + response + "\"";
}
void authentication() {
  CHECK(md5_hex("abc") == "900150983cd24fb0d6963f7d28e17f72");
  DigestAuth auth;
  const std::string nonce(48, 'a');
  CHECK(!auth.available() && auth.challenge(0, nonce).empty());
  auth.configure("device", md5_hex("admin:device:a longer password"));
  CHECK(auth.challenge(100, nonce).find(nonce) != std::string::npos);
  CHECK(auth.authorize(authorization(nonce), "POST", "/api/activate", 101) == DigestAuth::Result::REJECTED);
  CHECK(auth.authorize(authorization(nonce), "GET", "/api/settings", 101) == DigestAuth::Result::REJECTED);
  CHECK(auth.authorize(authorization(nonce) + ", nc=00000001", "POST", "/api/settings", 101) == DigestAuth::Result::REJECTED);
  CHECK(auth.authorize(authorization(nonce), "POST", "/api/settings", 101) == DigestAuth::Result::ACCEPTED);
  CHECK(auth.authorize(authorization(nonce), "POST", "/api/settings", 101) == DigestAuth::Result::REJECTED);
  CHECK(auth.authorize(authorization(nonce, "00000003"), "POST", "/api/settings", 101) == DigestAuth::Result::ACCEPTED);
  CHECK(auth.authorize(authorization(nonce, "00000002"), "POST", "/api/settings", 101) == DigestAuth::Result::ACCEPTED);
  CHECK(auth.authorize(authorization(nonce, "00000002"), "POST", "/api/settings", 101) == DigestAuth::Result::REJECTED);
  CHECK(auth.authorize(authorization(nonce, "00000004"), "POST", "/api/settings", 300100) == DigestAuth::Result::STALE);
  CHECK(auth.authorize(authorization(std::string(48, 'b')), "POST", "/api/settings", 102) == DigestAuth::Result::REJECTED);
  CHECK(auth.authorize(authorization(nonce, "00000000"), "POST", "/api/settings", 102) == DigestAuth::Result::REJECTED);
  CHECK(auth.authorize(authorization(nonce, "00000004"), "POST", "/api/settings", 99) == DigestAuth::Result::REJECTED);
  // A password reset clears all outstanding nonces, including authenticated sessions.
  auth.configure("device", md5_hex("admin:device:another password"));
  CHECK(auth.authorize(authorization(nonce, "00000004"), "POST", "/api/settings", 102) == DigestAuth::Result::REJECTED);
  CHECK(same_origin("http://device.local", "device.local", "same-origin"));
  CHECK(!same_origin("http://attacker.test", "device.local", "cross-site"));
  CHECK(!same_origin("null", "device.local", "same-origin"));
  CHECK(!same_origin("", "device.local", ""));
}
void authentication_rotation() {
  using Result = DigestAuth::Result;
  DigestAuth auth;
  const auto verifier = md5_hex("admin:device:a longer password");
  auth.configure("device", verifier);
  const std::string nonce(48, 'a'), replacement(48, 'b');
  CHECK(auth.challenge(100, nonce).find("stale=true") == std::string::npos);
  // Chromium rotates cnonce on every request while incrementing nc.
  for (unsigned i = 1; i <= 100; ++i) {
    char count[9]; std::snprintf(count, sizeof(count), "%08x", i);
    const auto header = authorization(nonce, count, "browser-" + std::to_string(i));
    CHECK(auth.authorize(header, "POST", "/api/settings", 101) == Result::ACCEPTED);
    CHECK(auth.authorize(header, "POST", "/api/settings", 101) == Result::REJECTED);
  }
  CHECK(auth.authorize(authorization(nonce, "00000064", "different-client-nonce"),
                       "POST", "/api/settings", 101) == Result::REJECTED);
  CHECK(auth.authorize(authorization(nonce), "POST", "/api/settings", 101) == Result::REJECTED);
  // Unseen out-of-order counts within the window are accepted exactly once.
  CHECK(auth.authorize(authorization(nonce, "00000067", "new"), "POST", "/api/settings", 102) == Result::ACCEPTED);
  CHECK(auth.authorize(authorization(nonce, "00000065", "another"), "POST", "/api/settings", 102) == Result::ACCEPTED);
  CHECK(auth.authorize(authorization(nonce, "00000065", "another"), "POST", "/api/settings", 102) == Result::REJECTED);
  // Verify credentials, target and replay state before returning STALE.
  const auto next = authorization(nonce, "00000068");
  CHECK(auth.authorize(next, "POST", "/api/settings", 300099) == Result::ACCEPTED);
  CHECK(auth.authorize(next, "POST", "/api/settings", 300100) == Result::REJECTED);
  const auto expired = authorization(nonce, "00000069");
  CHECK(auth.authorize(expired, "POST", "/api/settings", 300100) == Result::STALE);
  CHECK(auth.authorize(expired, "POST", "/api/activate", 300100) == Result::REJECTED);
  CHECK(auth.authorize(authorization(nonce, "00000069", "browser", "POST", "/api/settings", md5_hex("wrong")),
                       "POST", "/api/settings", 300100) == Result::REJECTED);
  CHECK(auth.challenge(300100, replacement, true).find(", stale=true") != std::string::npos);
  CHECK(auth.authorize(authorization(replacement), "POST", "/api/settings", 300101) == Result::ACCEPTED);
  // A bad digest must never consume a count needed by a legitimate request.
  CHECK(auth.authorize(authorization(replacement, "00000002", "browser", "POST", "/api/settings", md5_hex("wrong")),
                       "POST", "/api/settings", 300102) == Result::REJECTED);
  CHECK(auth.authorize(authorization(replacement, "00000002"), "POST", "/api/settings", 300102) == Result::ACCEPTED);
  auth.configure("device", md5_hex("admin:device:another password"));
  CHECK(auth.authorize(expired, "POST", "/api/settings", 300102) == Result::REJECTED);
  auth.configure("device", verifier);
  // Nonce storage stays bounded; eviction never makes an old nonce acceptable.
  for (char c = '0'; c <= '8'; ++c) auth.challenge(400000, std::string(48, c));
  CHECK(auth.authorize(authorization(std::string(48, '0')), "POST", "/api/settings", 700000) == Result::REJECTED);
  CHECK(auth.authorize(authorization(std::string(48, '8')), "POST", "/api/settings", 700000) == Result::STALE);
}
void credentials() {
  nvs_test::reset();
  nvs_test::committed[{"openathan", "scheduler"}] = {1, 2, 3};
  nvs_test::committed[{"openathan", "settings"}] = {4, 5, 6};
  CredentialStore store;
  WifiCredentials wifi;
  PasswordVerifier password;
  CHECK(store.load_wifi(wifi) == LoadResult::EMPTY && store.load_password(password) == LoadResult::EMPTY);
  CHECK(store.save_wifi({"home", "first password"}));
  CHECK(store.save_password({1, md5_hex("admin:device:a longer password")}));
  CHECK(store.load_wifi(wifi) == LoadResult::LOADED && wifi.ssid == "home" && wifi.password == "first password");
  CHECK(store.load_password(password) == LoadResult::LOADED && password.revision == 1);
  const auto history = nvs_test::committed.at({"openathan", "scheduler"});
  const auto settings = nvs_test::committed.at({"openathan", "settings"});
  nvs_test::fail_commit = true;
  CHECK(!store.save_wifi({"new home", "other password"}));
  CHECK(!store.healthy());
  nvs_test::fail_commit = false;
  CHECK(!store.save_password({2, md5_hex("new")}));
  nvs_test::power_cycle();
  CredentialStore reboot;
  CHECK(reboot.load_wifi(wifi) == LoadResult::LOADED && wifi.ssid == "home");
  CHECK(nvs_test::committed.at({"openathan", "scheduler"}) == history &&
        nvs_test::committed.at({"openathan", "settings"}) == settings);
  // Explicit USB password recovery may replace a corrupt record without touching other records.
  nvs_test::committed[{"oa_network", "password"}][9] ^= 1;
  CredentialStore corrupt;
  CHECK(corrupt.load_password(password) == LoadResult::ERROR);
  CHECK(corrupt.save_password({2, md5_hex("admin:device:new password")}));
  CHECK(corrupt.load_password(password) == LoadResult::LOADED && password.revision == 2);
  for (bool early : {false, true}) {
    nvs_test::reset();
    CredentialStore first;
    CHECK(first.load_wifi(wifi) == LoadResult::EMPTY);
    CHECK(first.save_wifi({"home", "first password"}));
    nvs_test::early_persist = early;
    nvs_test::cut_after_write = true;
    try {
      first.save_wifi({"new home", "other password"});
      CHECK(false);
    } catch (const std::runtime_error&) {
    }
    nvs_test::cut_after_write = false;
    nvs_test::power_cycle();
    CredentialStore next;
    CHECK(next.load_wifi(wifi) == LoadResult::LOADED && wifi.ssid == (early ? "new home" : "home"));
  }
}
void activation() {
  nvs_test::reset();
  SetupStore first;
  CHECK(first.begin(LoadResult::EMPTY, LoadResult::EMPTY) && !first.active());
  CHECK(nvs_test::committed.count({"oa_setup", "state"}) == 1);
  // Seeded settings after an interrupted boot never become an implicit activation.
  SetupStore resumed;
  CHECK(resumed.begin(LoadResult::LOADED, LoadResult::EMPTY) && !resumed.active());
  CHECK(resumed.activate() && resumed.active());
  SetupStore again;
  CHECK(again.begin(LoadResult::LOADED, LoadResult::LOADED) && again.active());
  nvs_test::committed[{"oa_setup", "state"}][5] ^= 1;
  SetupStore corrupt;
  CHECK(!corrupt.begin(LoadResult::LOADED, LoadResult::LOADED) && !corrupt.active());
  nvs_test::reset();
  SetupStore legacy;
  CHECK(legacy.begin(LoadResult::LOADED, LoadResult::LOADED) && legacy.active());
  nvs_test::reset();
  SetupStore missing;
  CHECK(!missing.begin(LoadResult::EMPTY, LoadResult::LOADED));
  CHECK(nvs_test::writes == 0);
  for (bool early : {false, true}) {
    nvs_test::reset();
    SetupStore pending;
    CHECK(pending.begin(LoadResult::EMPTY, LoadResult::EMPTY));
    nvs_test::early_persist = early;
    nvs_test::cut_after_write = true;
    try {
      pending.activate();
      CHECK(false);
    } catch (const std::runtime_error&) {
    }
    nvs_test::cut_after_write = false;
    nvs_test::power_cycle();
    SetupStore recovered;
    CHECK(recovered.begin(LoadResult::LOADED, LoadResult::EMPTY));
    CHECK(recovered.active() == early);
  }
  nvs_test::reset();
  nvs_test::fail_commit = true;
  SetupStore failed;
  CHECK(!failed.begin(LoadResult::EMPTY, LoadResult::EMPTY) && !failed.active() && !failed.activate());
}
void wifi_attempts() {
  WifiAttempt attempt;
  CHECK(attempt.begin({"same SSID", "replacement password"}, 100));
  CHECK(!attempt.begin({"other SSID", "another password"}, 100));
  attempt.joined("same SSID");  // old callback is ignored before disconnect
  CHECK(attempt.step(101, true, "same SSID") == WifiAttempt::Result::WAIT);
  CHECK(attempt.step(102, false, "") == WifiAttempt::Result::START_CONNECTION);
  CHECK(attempt.step(103, true, "same SSID") == WifiAttempt::Result::WAIT);
  attempt.joined("wrong network");
  CHECK(attempt.step(104, true, "wrong network") == WifiAttempt::Result::WAIT);
  attempt.joined("same SSID");
  CHECK(attempt.step(105, true, "same SSID") == WifiAttempt::Result::CONNECTED);
  attempt.finish();
  CHECK(!attempt.active());
  CHECK(attempt.begin({"hidden network", "wrong password"}, 200));
  CHECK(attempt.step(201, false, "") == WifiAttempt::Result::START_CONNECTION);
  CHECK(attempt.step(30200, false, "") == WifiAttempt::Result::TIMED_OUT);
  attempt.finish();
  CHECK(!attempt.active());
}
int main() {
  frames();
  authentication();
  authentication_rotation();
  credentials();
  activation();
  wifi_attempts();
}
