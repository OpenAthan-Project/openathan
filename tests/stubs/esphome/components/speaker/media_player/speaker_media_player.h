#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <optional>
#include <deque>
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
enum class AudioPipelineState { STOPPED, PLAYING, ERROR_READING, ERROR_DECODING };
struct SpeakerMediaPlayer {
  AudioPipelineState pipeline_state{AudioPipelineState::STOPPED};
  AudioPipelineState media_pipeline_state{AudioPipelineState::STOPPED};
  AudioPipelineState announcement_state() const { return pipeline_state; }
  bool playback_stopped() const {
    return initialized && pending_commands.empty() && pipeline_state == AudioPipelineState::STOPPED &&
           media_pipeline_state == AudioPipelineState::STOPPED;
  }
  bool initialized{true};
  float volume{0.7f};
  bool muted{}, drop_volume{};
  bool is_muted() const { return muted; }
  bool is_ready() const { return initialized; }
  media_player::MediaPlayerState state{media_player::MEDIA_PLAYER_STATE_IDLE};
  std::vector<std::string> commands;
  std::deque<std::string> pending_commands;
  // Consuming a Stop command does not synchronously stop a worker pipeline.
  void drain_command() { if (!pending_commands.empty()) pending_commands.pop_front(); }
  void drain_commands() { pending_commands.clear(); }
  void queue(const std::string &command) { commands.push_back(command); pending_commands.push_back(command); }
  struct Call {
    SpeakerMediaPlayer *player;
    bool announcement{};
    std::optional<float> volume;
    std::string url;
    void set_volume(float value) { volume = value; }
    void set_command(media_player::MediaPlayerCommand) {}
    void set_announcement(bool value) { announcement = value; }
    void set_media_url(const std::string &value) { url = value; }
    void perform() {
      if (!url.empty()) player->queue(url);
      else if (volume) {
        if (!player->drop_volume) { player->volume = *volume; player->muted = *volume == 0; }
      } else player->queue(announcement ? "stop announcement" : "stop media");
    }
  };
  Call make_call() { return {this}; }
  std::vector<audio::AudioFile *> queued;
  void play_file(audio::AudioFile *file, bool, bool) { queued.push_back(file); queue("play"); }
};
}
}
