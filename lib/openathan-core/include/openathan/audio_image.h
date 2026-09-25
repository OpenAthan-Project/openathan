#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace openathan {

// Little-endian v1 image: 4096-byte header, then aligned MP3 payloads.
// Hashes detect corruption; they do not authenticate an image's publisher.
constexpr size_t AUDIO_HEADER_SIZE = 4096;
constexpr size_t AUDIO_METADATA_SIZE = 112;
constexpr size_t AUDIO_PARTITION_SIZE = 0x380000;
enum class Track : uint32_t { NORMAL = 1, FAJR = 2 };
struct AudioTrack {
  uint32_t offset{};
  uint32_t length{};
};
using AudioCatalog = std::array<AudioTrack, 2>;
enum class AudioError { OK, HEADER, METADATA_HASH, BOUNDS, TRACK_HASH };

inline uint32_t read_u32(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

template<typename HashMatches>
AudioError validate_audio_image(const uint8_t *data, size_t size, HashMatches hash_matches, AudioCatalog &out) {
  out = {};
  if (!data || size < AUDIO_HEADER_SIZE || std::memcmp(data, "OAUDIO01", 8) != 0 || read_u32(data + 12) != 2)
    return AudioError::HEADER;
  if (!hash_matches(data, AUDIO_METADATA_SIZE, data + AUDIO_METADATA_SIZE))
    return AudioError::METADATA_HASH;
  const uint32_t used = read_u32(data + 8);
  if (used > size || used < AUDIO_HEADER_SIZE)
    return AudioError::BOUNDS;
  AudioCatalog candidate{};
  uint32_t end = AUDIO_HEADER_SIZE;
  for (size_t i = 0; i < candidate.size(); ++i) {
    const uint8_t *entry = data + 16 + i * 48;
    const uint32_t offset = read_u32(entry + 4), length = read_u32(entry + 8);
    // Fixed order prevents duplicate/unknown IDs. Subtraction avoids integer overflow.
    if (read_u32(entry) != i + 1 || read_u32(entry + 12) != 1 || offset % 16 != 0 ||
        offset < end || offset > used || length == 0 || length > used - offset)
      return AudioError::BOUNDS;
    if (!hash_matches(data + offset, length, entry + 16))
      return AudioError::TRACK_HASH;
    candidate[i] = {offset, length};
    end = offset + length;
  }
  if (end != used)
    return AudioError::BOUNDS;
  out = candidate;
  return AudioError::OK;
}
}  // namespace openathan
