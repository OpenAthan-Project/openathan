// Line-oriented host adapter for browser tests of the production Digest code.
// This executable is never linked into firmware.
#include "digest.h"
#include <iomanip>
#include <iostream>
#include <sstream>

using esphome::openathan_device::DigestAuth;
using esphome::openathan_device::md5_hex;
int main() {
  DigestAuth auth;
  auth.configure("OpenAthan-test", md5_hex("admin:OpenAthan-test:browser test password"));
  std::string line;
  while (std::getline(std::cin, line)) {
    uint64_t now;
    std::string method, uri, header, random;
    std::istringstream input(line);
    if (!(input >> now >> std::quoted(method) >> std::quoted(uri) >> std::quoted(header) >> random)) return 1;
    const auto result = auth.authorize(header, method, uri, now);
    const auto challenge = result == DigestAuth::Result::ACCEPTED
        ? std::string{} : auth.challenge(now, random, result == DigestAuth::Result::STALE);
    std::cout << std::quoted(challenge) << std::endl;
  }
}
