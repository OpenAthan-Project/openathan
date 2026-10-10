#include "esphome/components/waveshare_box_audio/audio.h"
#include "esphome/components/waveshare_box_display/backlight.h"
#include <cassert>
int main() {
  using namespace esphome;
  GPIOPin pin; es8311::ES8311 dac; openathan_audio::PartitionAudio playback;
  waveshare_box_audio::Audio audio;
  audio.set_amplifier(&pin); audio.set_dac(&dac); audio.set_playback(&playback);
  audio.setup(); assert(pin.initialized && pin.levels == std::vector<bool>{false});
  audio.loop(); assert(!audio.ready() && pin.levels.size() == 1);
  assert(dac.volumes.empty());
  assert(!audio.start(::openathan::Track::NORMAL) && playback.starts == 0);
  assert(!audio.request_volume_percent(30));
  playback.available = true;
  // Wait for fixed codec gain before accepting volume or enabling playback.
  assert(!audio.request_volume_percent(30) && !audio.volume_percent());
  audio.loop(); assert(audio.ready() && pin.levels.back());
  assert(dac.volumes == std::vector<float>{0.75f} && dac.unmute_writes == 1);
  for (unsigned percent : {0U, 30U, 80U, 100U}) {
    assert(audio.request_volume_percent(percent) && audio.volume_percent() == percent);
  }
  audio.loop(); assert(pin.levels.size() == 2);
  assert(dac.volumes.size() == 1 && dac.unmute_writes == 1);  // Software owns volume/mute.
  assert(audio.start(::openathan::Track::FAJR) && audio.playing());
  audio.stop(); assert(!audio.playing() && playback.stops == 1);
  playback.available = false; audio.loop(); assert(!audio.ready() && !pin.levels.back());
  playback.available = true; audio.loop(); assert(audio.ready());
  assert(dac.volumes.size() == 1);
  dac.mark_failed(); assert(!audio.ready() && !audio.start(::openathan::Track::NORMAL));
  audio.loop(); assert(!pin.levels.back());
  audio.on_shutdown(); assert(!pin.levels.back());
  for (bool fail_gain : {true, false}) {
    GPIOPin failed_pin; es8311::ES8311 failed_dac;
    openathan_audio::PartitionAudio failed_playback;
    waveshare_box_audio::Audio failed_audio;
    failed_audio.set_amplifier(&failed_pin); failed_audio.set_dac(&failed_dac);
    failed_audio.set_playback(&failed_playback); failed_audio.setup();
    failed_playback.available = true;
    failed_dac.volume_write_ok = !fail_gain; failed_dac.unmute_write_ok = fail_gain;
    failed_audio.loop();
    assert(failed_audio.is_failed() && !failed_audio.ready());
    assert(failed_pin.levels == std::vector<bool>{false});
    assert(!failed_audio.request_volume_percent(80) && !failed_audio.volume_percent());
    assert(!failed_audio.start(::openathan::Track::NORMAL) && failed_playback.starts == 0);
    failed_dac.volume_write_ok = true; failed_dac.unmute_write_ok = true;
    failed_audio.loop();
    assert(failed_dac.volumes.size() == 1 && !failed_audio.ready());
    assert(failed_dac.unmute_writes == (fail_gain ? 0U : 1U));
  }
  output::FloatOutput pwm; display::Display canvas; waveshare_box_display::Backlight light;
  light.set_output(&pwm); light.set_display(&canvas); light.setup();
  assert(!light.is_failed() && pwm.levels.back() == 0.5f);
  assert(!light.apply(0) && !light.apply(101) && pwm.levels.size() == 1);
  assert(light.apply(1) && pwm.levels.back() == 0.01f);
  assert(light.apply(100) && pwm.levels.back() == 1.0f);
  canvas.mark_failed(); assert(!light.apply(50) && pwm.levels.size() == 3);
}
