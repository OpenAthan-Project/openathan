#include "phone_setup.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); std::abort(); } } while (0)
using namespace esphome::openathan_device::phone_setup;

static void enrollment() {
  for (bool incomplete : {false, true})
    for (bool healthy : {false, true})
      for (bool empty : {false, true})
        for (bool fault : {false, true})
          CHECK(eligible(incomplete, healthy, empty, fault) == (incomplete && healthy && empty && !fault));
  CHECK(ap_socket(true, "192.168.4.1", ""));
  CHECK(!ap_socket(true, "192.168.4.1", "192.168.1.20"));
  CHECK(!ap_socket(false, "192.168.4.1", ""));
  CHECK(!ap_socket(true, "192.168.1.20", "192.168.1.20"));
  CHECK(!ap_socket(true, "192.168.4.1", "192.168.4.1"));
  CHECK(canonical_host("192.168.4.1") && canonical_host("192.168.4.1:80"));
  for (const char *host : {"192.168.4.1.evil", "evil", "192.168.4.1:81", "192.168.004.1", "user@192.168.4.1"})
    CHECK(!canonical_host(host));
  CHECK(setup_path("/setup") && setup_path("/setup/") && setup_path("/setup/status"));
  CHECK(!setup_path("/api/status") && !setup_path("/setup-other") && !setup_path("https://192.168.4.1/setup/"));
  CHECK(equal_verifier(std::string(32, 'a'), std::string(32, 'a')));
  CHECK(!equal_verifier(std::string(32, 'a'), std::string(32, 'b')));
  CHECK(!equal_verifier("", "") && !equal_verifier(std::string(31, 'a'), std::string(32, 'a')));
}

static std::vector<uint8_t> question(uint16_t type = 1) {
  return {0x12, 0x34, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0,
          7, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0,
          uint8_t(type >> 8), uint8_t(type), 0, 1};
}
static void dns() {
  std::array<uint8_t, 512> output;
  const auto input = question();
  const auto size = dns_response(input, output);
  CHECK(size == input.size() + 16);
  CHECK(output[0] == 0x12 && output[1] == 0x34 && (output[2] & 0x80));
  CHECK(output[5] == 1 && output[7] == 1 && output[9] == 0 && output[11] == 0);
  CHECK(output[size - 4] == 192 && output[size - 3] == 168 && output[size - 2] == 4 && output[size - 1] == 1);
  for (size_t length = 0; length < input.size(); ++length)
    CHECK(dns_response(std::span(input).first(length), output) == 0);
  CHECK(dns_response(input, std::span(output).first(size - 1)) == 0);
  for (uint16_t type : {28, 12, 65}) {
    auto other = question(type);
    CHECK(dns_response(other, output) == other.size());
    CHECK(output[7] == 0 && !(output[3] & 0x0f));
  }
  CHECK(dns_response(question(255), output) == input.size() + 16);
  for (auto [offset, value] : std::array<std::pair<size_t, uint8_t>, 8>{{
      {2, 0x80}, {2, 0x08}, {3, 1}, {4, 1}, {5, 2}, {7, 1}, {12, 0xc0}, {12, 64}}}) {
    auto malformed = input;
    malformed[offset] = value;
    CHECK(dns_response(malformed, output) == 0);
  }
  auto wrong_class = input;
  wrong_class.back() = 3;
  CHECK(dns_response(wrong_class, output) == 0);
  // An EDNS trailer is discarded rather than reflected or amplified.
  auto edns = input;
  edns[11] = 1;
  edns.insert(edns.end(), {0, 0, 41, 2, 0, 0, 0, 0, 0, 0, 0});
  CHECK(dns_response(edns, output) == size && output[11] == 0);
  // Deterministic malformed datagrams exercise every supported input length.
  uint32_t seed = 0x34120399;
  for (size_t length = 0; length <= 513; ++length) {
    std::vector<uint8_t> bytes(length);
    for (auto &byte : bytes) { seed = seed * 1664525u + 1013904223u; byte = seed >> 24; }
    const auto result = dns_response(bytes, output);
    CHECK(result <= output.size());
  }
}
int main() { enrollment(); dns(); }
