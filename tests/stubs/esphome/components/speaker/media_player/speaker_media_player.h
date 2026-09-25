#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <optional>
namespace esphome {
namespace media_player {
enum MediaPlayerState { MEDIA_PLAYER_STATE_IDLE, MEDIA_PLAYER_STATE_PLAYING, MEDIA_PLAYER_STATE_ANNOUNCING };
enum MediaPlayerCommand { MEDIA_PLAYER_COMMAND_STOP };
}
namespace audio {
enum class AudioFileType { MP3 };
struct AudioFile { const uint8_t *data; size_t length; AudioFileType file_type; };
}
namespace speaker {
struct SpeakerMediaPlayer {
  bool initialized{true};
  float volume{0.7f};
  bool muted{}, drop_volume{};
  bool is_muted() const { return muted; }
  bool is_ready() const { return initialized; }
  media_player::MediaPlayerState state{media_player::MEDIA_PLAYER_STATE_IDLE};
  std::vector<std::string> commands;
  struct Call {
    SpeakerMediaPlayer *player;
    bool announcement{};
    std::optional<float> volume;
    void set_volume(float value) { volume = value; }
    void set_command(media_player::MediaPlayerCommand) {}
    void set_announcement(bool value) { announcement = value; }
    void perform() {
      if (volume) {
        if (!player->drop_volume) { player->volume = *volume; player->muted = *volume == 0; }
      } else player->commands.push_back(announcement ? "stop announcement" : "stop media");
    }
  };
  Call make_call() { return {this}; }
  std::vector<audio::AudioFile *> queued;
  void play_file(audio::AudioFile *file, bool, bool) { queued.push_back(file); commands.push_back("play"); }
};
}
}
