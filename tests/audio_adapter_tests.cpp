#include "partition_audio.h"
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

static bool present = true;
static bool map_fails = false;
static unsigned maps = 0, unmaps = 0;
static esp_partition_t partition{openathan::AUDIO_PARTITION_SIZE};
static std::vector<uint8_t> bytes;
static void check(bool condition) { if (!condition) std::abort(); }
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
  check(valid.play(Track::NORMAL) && valid.play(Track::FAJR));
  check(!valid.play(static_cast<Track>(3)) && !valid.play(static_cast<Track>(0)));
  check(player.queued.size() == 2 && player.queued[0] != player.queued[1]);
  check(player.queued[0]->data == bytes.data() + 4096 && player.queued[0]->length > 0);
  check(player.queued[1]->data > player.queued[0]->data && player.queued[1]->length > 0);
  check(valid.play(Track::NORMAL) && player.queued[0] == player.queued[2] && unmaps == 1);
  player.commands.clear();
  check(valid.start(Track::FAJR));
  check(player.commands == std::vector<std::string>{"stop media", "stop announcement", "play"});
  player.state = esphome::media_player::MEDIA_PLAYER_STATE_ANNOUNCING;
  check(valid.playing());
  player.initialized = false;
  player.commands.clear();
  check(!valid.ready() && !valid.start(Track::NORMAL));
  valid.stop();
  check(player.commands.empty());
}
