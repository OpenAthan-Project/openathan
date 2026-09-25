#include "openathan/audio_image.h"
#include <fstream>
#include <iterator>
#include <vector>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#endif
int main(int argc, char **argv) {
  if (argc != 2) return 99;
  std::ifstream input(argv[1], std::ios::binary);
  std::vector<uint8_t> image{std::istreambuf_iterator<char>(input), {}};
  auto hash = [](const uint8_t *data, size_t length, const uint8_t *expected) {
    uint8_t digest[32];
#ifdef __APPLE__
    CC_SHA256(data, static_cast<CC_LONG>(length), digest);
#else
    SHA256(data, length, digest);
#endif
    return std::memcmp(digest, expected, 32) == 0;
  };
  openathan::AudioCatalog result;
  const auto error = openathan::validate_audio_image(image.data(), image.size(), hash, result);
  if (error != openathan::AudioError::OK && (result[0].length || result[1].length)) return 98;
  return static_cast<int>(error);
}
