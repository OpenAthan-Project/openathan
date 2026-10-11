#include "partition_audio.h"
#include "esphome/core/hal.h"
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <iostream>
#include <vector>

static bool present = true;
static bool map_fails = false;
static unsigned maps = 0, unmaps = 0;
static esp_partition_t partition{openathan::AUDIO_PARTITION_SIZE};
static std::vector<uint8_t> bytes;
#define check(condition) do { if (!(condition)) { std::cerr << __LINE__ << ": " << #condition << '\n'; std::abort(); } } while (false)
const esp_partition_t *esp_partition_find_first(int, int, const char *) { return present ? &partition : nullptr; }
int esp_partition_mmap(const esp_partition_t *, size_t, size_t, int, const void **out,
                       esp_partition_mmap_handle_t *handle) {
  ++maps;
  if (map_fails) return -1;
  *out = bytes.data(); *handle = 1;
  return 0;
}
void esp_partition_munmap(esp_partition_mmap_handle_t) { ++unmaps; }

int main(int argc, char **argv) {
  check(argc == 2);
  std::ifstream input(argv[1], std::ios::binary);
  bytes = {std::istreambuf_iterator<char>(input), {}};
  check(bytes.size() == openathan::AUDIO_PARTITION_SIZE);
  using esphome::openathan_audio::PartitionAudio;
  using openathan::Track;
  esphome::speaker::SpeakerMediaPlayer player;
  present = false;
  PartitionAudio absent;
  absent.set_player(&player); absent.setup();
  check(absent.is_failed() && !absent.ready() && !absent.play(Track::NORMAL) && maps == 0);
  present = true; partition.size = 1;
  PartitionAudio wrong_size;
  wrong_size.setup();
  check(wrong_size.is_failed() && maps == 0);
  partition.size = openathan::AUDIO_PARTITION_SIZE; map_fails = true;
  PartitionAudio map_failure;
  map_failure.setup();
  check(map_failure.is_failed() && !map_failure.ready() && maps == 1 && unmaps == 0);
  map_fails = false; bytes[4096] ^= 1;
  PartitionAudio corrupt;
  corrupt.setup();
  check(corrupt.is_failed() && !corrupt.ready() && unmaps == 1);
  bytes[4096] ^= 1;
  PartitionAudio valid;
  valid.set_player(&player); valid.setup();
  check(!valid.is_failed() && valid.ready());
  check(valid.volume_percent() == 70);
  player.drop_volume = true;
  check(valid.request_volume_percent(25) && valid.volume_percent() == 70);
  player.drop_volume = false;
  check(valid.request_volume_percent(25) && valid.volume_percent() == 25);
  check(!valid.request_volume_percent(101));
  check(valid.request_volume_percent(0) && valid.volume_percent() == 0 && player.is_muted());
  check(valid.request_volume_percent(70) && valid.volume_percent() == 70 && !player.is_muted());
  check(valid.play(Track::NORMAL));
  check(player.queued.empty() && valid.source() == openathan::PlaybackSource::ATHAN);
  player.drain_commands(); valid.loop();
  check(player.queued.size() == 1);
  check(valid.play(Track::FAJR));
  player.drain_commands(); valid.loop();
  check(!valid.play(static_cast<Track>(3)) && !valid.play(static_cast<Track>(0)));
  check(player.queued.size() == 2 && player.queued[0] != player.queued[1]);
  check(player.queued[0]->data == bytes.data() + 4096 && player.queued[0]->length > 0);
  check(player.queued[1]->data > player.queued[0]->data && player.queued[1]->length > 0);
  check(valid.play(Track::NORMAL));
  player.drain_commands(); valid.loop();
  check(player.queued[0] == player.queued[2] && unmaps == 1);
  player.commands.clear();
  check(valid.start(Track::FAJR));
  check(player.commands == std::vector<std::string>({"stop media", "stop announcement"}));
  player.drain_commands(); valid.loop();
  check(player.commands == std::vector<std::string>({"stop media", "stop announcement", "play"}));
  auto epoch = valid.playback_epoch();
  check(!valid.start_stream("http://cdn.mp3quran.net/test/001.mp3"));
  check(valid.playback_epoch() == epoch);
  check(valid.start_stream("https://cdn.mp3quran.net/audio/test/001.mp3"));
  check(valid.source() == openathan::PlaybackSource::QURAN);
  check(valid.playback_epoch() != epoch);
  player.drain_commands(); valid.loop();
  player.pipeline_state = esphome::speaker::AudioPipelineState::ERROR_DECODING;
  valid.loop();
  check(valid.stream_state() == openathan::StreamState::ERROR);
  player.pipeline_state = esphome::speaker::AudioPipelineState::STOPPED;
  check(valid.start_stream("https://cdn.mp3quran.net/audio/test/001.mp3"));
  esphome::mock_millis += 30000;
  valid.loop();
  check(valid.stream_state() == openathan::StreamState::ERROR && valid.source() == openathan::PlaybackSource::NONE);
  epoch = valid.playback_epoch();
  check(valid.start(Track::NORMAL));
  check(valid.source() == openathan::PlaybackSource::ATHAN && valid.playback_epoch() != epoch);
  valid.stop();
  check(valid.source() == openathan::PlaybackSource::NONE);
  player.state = esphome::media_player::MEDIA_PLAYER_STATE_ANNOUNCING;
  check(valid.playing());
  player.initialized = false;
  player.commands.clear();
  check(!valid.ready() && !valid.start(Track::NORMAL));
  valid.stop();
  check(player.commands.empty());

  using Source = openathan::PlaybackSource;
  using Stream = openathan::StreamState;
  using Pipeline = esphome::speaker::AudioPipelineState;
  using State = esphome::media_player::MediaPlayerState;
  const std::string url = "https://cdn.mp3quran.net/audio/test/001.mp3";
  esphome::speaker::SpeakerMediaPlayer handoff;
  PartitionAudio audio; audio.set_player(&handoff); audio.setup();
  // Outgoing Quran -> stopped -> new Athan must retain its reservation. Drain
  // one command at a time and independently delay each asynchronous pipeline.
  handoff.pipeline_state = Pipeline::PLAYING; handoff.state = State::MEDIA_PLAYER_STATE_ANNOUNCING;
  check(audio.start(Track::NORMAL)); epoch = audio.playback_epoch(); audio.loop();
  check(audio.source() == Source::ATHAN && handoff.queued.empty());
  handoff.pipeline_state = Pipeline::STOPPED; handoff.state = State::MEDIA_PLAYER_STATE_IDLE;
  handoff.drain_command(); audio.loop(); check(handoff.queued.empty());
  handoff.drain_command(); handoff.media_pipeline_state = Pipeline::PLAYING;
  audio.loop(); check(handoff.queued.empty() && audio.source() == Source::ATHAN);
  handoff.media_pipeline_state = Pipeline::STOPPED; audio.loop();
  check(handoff.queued.size() == 1 && audio.source() == Source::ATHAN);
  audio.loop(); check(audio.source() == Source::ATHAN); // New start has not played yet.
  handoff.drain_commands(); handoff.pipeline_state = Pipeline::PLAYING;
  handoff.state = State::MEDIA_PLAYER_STATE_ANNOUNCING; audio.loop();
  check(audio.source() == Source::ATHAN && audio.playback_epoch() == epoch);
  handoff.pipeline_state = Pipeline::STOPPED; handoff.state = State::MEDIA_PLAYER_STATE_IDLE;
  audio.loop(); check(audio.source() == Source::NONE);

  // Direct play() and rapid stored/stream replacements use the same boundary;
  // the latest accepted recording is the only start submitted.
  handoff.commands.clear(); handoff.queued.clear();
  check(audio.start_stream(url)); check(audio.play(Track::NORMAL)); check(audio.play(Track::FAJR));
  epoch = audio.playback_epoch();
  check(!audio.play(static_cast<Track>(3)) && !audio.start_stream("http://cdn.mp3quran.net/a.mp3"));
  check(audio.playback_epoch() == epoch && audio.source() == Source::ATHAN);
  audio.loop(); check(handoff.queued.empty());
  handoff.drain_commands(); audio.loop();
  check(handoff.queued.size() == 1 && handoff.queued[0]->data > bytes.data() + 4096);
  check(handoff.commands.back() == "play");
  handoff.drain_commands();
  handoff.commands.clear(); check(audio.play(Track::NORMAL)); check(audio.start_stream(url));
  handoff.drain_commands(); audio.loop(); check(handoff.commands.back() == url);

  const auto submitted_files = handoff.queued.size();
  check(audio.start(Track::NORMAL)); epoch = audio.playback_epoch(); audio.stop();
  check(audio.source() == Source::NONE && audio.playback_epoch() != epoch);
  handoff.drain_commands(); audio.loop(); check(handoff.queued.size() == submitted_files);

  // Stop revokes a pending replacement before it can submit a URL; clearing
  // the old speaker state or recovering a failed speaker cannot revive it.
  check(audio.start_stream(url)); epoch = audio.playback_epoch(); audio.stop();
  check(audio.source() == Source::NONE && audio.playback_epoch() != epoch);
  handoff.commands.clear(); handoff.drain_commands(); audio.loop(); check(handoff.commands.empty());
  check(audio.start_stream(url)); handoff.initialized = false; audio.loop();
  check(audio.source() == Source::NONE && audio.stream_state() == Stream::ERROR);
  handoff.initialized = true; handoff.commands.clear(); handoff.drain_commands();
  audio.loop(); check(handoff.commands.empty());
  check(audio.start(Track::FAJR)); handoff.initialized = false; audio.loop();
  check(audio.source() == Source::NONE);
  handoff.initialized = true; handoff.commands.clear(); handoff.drain_commands();
  audio.loop(); check(handoff.commands.empty() && handoff.queued.size() == submitted_files);

  // The startup deadline includes time waiting for Stop, including when the
  // outgoing session reports PLAYING. Submission does not reset that deadline.
  check(audio.start_stream(url)); handoff.pipeline_state = Pipeline::PLAYING;
  esphome::mock_millis += 29999; audio.loop(); check(audio.stream_state() == Stream::LOADING);
  esphome::mock_millis += 1; audio.loop();
  check(audio.stream_state() == Stream::ERROR && audio.source() == Source::NONE);
  handoff.pipeline_state = Pipeline::STOPPED; handoff.drain_commands();
  check(audio.start_stream(url)); esphome::mock_millis += 25000;
  handoff.drain_commands(); audio.loop();
  esphome::mock_millis += 4999; audio.loop(); check(audio.stream_state() == Stream::LOADING);
  esphome::mock_millis += 1; audio.loop(); check(audio.stream_state() == Stream::ERROR);

  // Old reader/decoder errors are ignored while stopping. Errors observed
  // after submission end the new session and cannot restart it.
  for (auto failure : {Pipeline::ERROR_READING, Pipeline::ERROR_DECODING}) {
    check(audio.start_stream(url)); handoff.pipeline_state = failure; audio.loop();
    check(audio.stream_state() == Stream::LOADING && audio.source() == Source::QURAN);
    handoff.drain_commands(); audio.loop(); check(audio.stream_state() == Stream::LOADING);
    handoff.pipeline_state = Pipeline::STOPPED; audio.loop();
    handoff.pipeline_state = failure; audio.loop();
    check(audio.stream_state() == Stream::ERROR && audio.source() == Source::NONE);
    handoff.commands.clear(); handoff.drain_commands(); handoff.pipeline_state = Pipeline::STOPPED;
    audio.loop(); check(handoff.commands.empty());
    check(audio.start(Track::NORMAL)); handoff.drain_commands(); audio.loop();
    handoff.pipeline_state = failure; audio.loop(); check(audio.source() == Source::NONE);
    handoff.pipeline_state = Pipeline::STOPPED; handoff.drain_commands();
  }
}
