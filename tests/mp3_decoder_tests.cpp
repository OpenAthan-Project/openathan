#include "audio_decoder.h"
#include "esphome/core/hal.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#define check(condition) do { if (!(condition)) { std::cerr << __LINE__ << ": " << #condition << '\n'; std::abort(); } } while (false)
using namespace esphome::audio;
using esphome::ring_buffer::RingBuffer;

struct Sink : AudioSinkCallback {
  std::vector<uint8_t> pcm;
  bool blocked = false;
  size_t audio_sink_write(uint8_t *data, size_t length, TickType_t) override {
    if (blocked) return 0;
    pcm.insert(pcm.end(), data, data + length);
    return length;
  }
};
static AudioDecoderState finish(AudioDecoder &decoder) {
  for (unsigned count = 0; count < 200000; ++count) {
    auto state = decoder.decode(true);
    if (state != AudioDecoderState::DECODING) return state;
  }
  check(false); return AudioDecoderState::FAILED;
}
static std::vector<uint8_t> read(const char *path) {
  std::ifstream file(path, std::ios::binary);
  check(file.good()); return {std::istreambuf_iterator<char>(file), {}};
}
static std::vector<uint8_t> complete(const std::vector<uint8_t> &bytes, bool chunked) {
  AudioDecoder decoder(127, 512); Sink sink;
  check(decoder.start(AudioFileType::MP3) == ESP_OK && decoder.add_sink(&sink) == ESP_OK);
  if (chunked) {
    auto ring = std::make_shared<RingBuffer>(); std::weak_ptr<RingBuffer> weak = ring;
    check(decoder.add_source(weak) == ESP_OK);
    // A still-open empty reader is not EOF or malformed media.
    for (unsigned i = 0; i < 3; ++i) check(decoder.decode(false) == AudioDecoderState::DECODING);
    size_t chunk_index = 0;
    for (size_t offset = 0; offset < bytes.size();) {
      const size_t sizes[] = {1, 7, 127, 1024};
      size_t length = std::min(sizes[chunk_index++ % 4], bytes.size() - offset);
      check(ring->write_without_replacement(const_cast<uint8_t *>(bytes.data() + offset), length, 0) == length);
      offset += length;
      check(decoder.decode(false) == AudioDecoderState::DECODING);
    }
    check(finish(decoder) == AudioDecoderState::FINISHED);
  } else {
    check(decoder.add_source(bytes.data(), bytes.size()) == ESP_OK);
    check(finish(decoder) == AudioDecoderState::FINISHED);
  }
  check(decoder.get_audio_stream_info().has_value() && !sink.pcm.empty());
  return sink.pcm;
}
static void invalid(const std::vector<uint8_t> &bytes) {
#ifdef OPENATHAN_QURAN
  constexpr auto expected = AudioDecoderState::FAILED;
#else
  constexpr auto expected = AudioDecoderState::FINISHED;
#endif
  for (bool chunked : {false, true}) {
    AudioDecoder decoder(7, 512); Sink sink;
    check(decoder.start(AudioFileType::MP3) == ESP_OK && decoder.add_sink(&sink) == ESP_OK);
    auto ring = std::make_shared<RingBuffer>(); std::weak_ptr<RingBuffer> weak = ring;
    if (chunked) {
      check(decoder.add_source(weak) == ESP_OK);
      for (unsigned i = 0; i < 3; ++i) check(decoder.decode(false) == AudioDecoderState::DECODING);
      for (size_t offset = 0; offset < bytes.size();) {
        size_t length = std::min(size_t{3}, bytes.size() - offset);
        ring->write_without_replacement(const_cast<uint8_t *>(bytes.data() + offset), length, 0);
        offset += length;
        check(decoder.decode(false) == AudioDecoderState::DECODING);
      }
    } else check(decoder.add_source(bytes.data(), bytes.size()) == ESP_OK);
    const auto terminal = finish(decoder);
    if (terminal != expected)
      std::cerr << "Invalid MP3 bytes=" << bytes.size() << ", chunked=" << chunked
                << ", terminal=" << static_cast<unsigned>(terminal) << '\n';
    check(terminal == expected);
    check(sink.pcm.empty());
  }
}
int main(int argc, char **argv) {
  check(argc == 4);
  auto short_clip = read(argv[1]), full_clip = read(argv[2]);
  check(short_clip.size() > 128 && full_clip.size() > short_clip.size());
  const std::string text = "Deliberately invalid MP3 input. No audio frames.\n";
  invalid({text.begin(), text.end()}); invalid({});
  // A complete ID3v2 metadata tag and a valid but incomplete first MP3 frame
  // do not count as decoded audio.
  invalid({'I','D','3',4,0,0,0,0,0,0});
  invalid({short_clip.begin(), short_clip.begin() + 128});
  auto single_frame = read(argv[3]);
  check(single_frame.size() > 128 && single_frame.size() < short_clip.size());
  check(complete(single_frame, false) == complete(single_frame, true));
  check(complete(short_clip, false) == complete(short_clip, true));
  check(complete(full_clip, false) == complete(full_clip, true));
  // Truncation after valid PCM retains the existing graceful partial EOF.
  auto partial = std::vector<uint8_t>(full_clip.begin(), full_clip.begin() + full_clip.size() / 2);
  check(complete(partial, false) == complete(partial, true));

  AudioDecoder reused(127, 512); Sink sink;
  check(reused.start(AudioFileType::MP3) == ESP_OK && reused.add_sink(&sink) == ESP_OK);
  check(reused.add_source(short_clip.data(), short_clip.size()) == ESP_OK);
  sink.blocked = true;
  for (unsigned i = 0; i < 3; ++i) check(reused.decode(true) == AudioDecoderState::DECODING);
  check(sink.pcm.empty()); sink.blocked = false;
  check(finish(reused) == AudioDecoderState::FINISHED && !sink.pcm.empty());
  size_t previous = sink.pcm.size();
  check(reused.start(AudioFileType::MP3) == ESP_OK);
  check(reused.add_source(reinterpret_cast<const uint8_t *>(text.data()), text.size()) == ESP_OK);
#ifdef OPENATHAN_QURAN
  check(finish(reused) == AudioDecoderState::FAILED);
#else
  check(finish(reused) == AudioDecoderState::FINISHED);
#endif
  check(sink.pcm.size() == previous);
  std::cout << "MP3 EOF, real PCM, chunked input, partial EOF, backpressure and reuse passed\n";
}
