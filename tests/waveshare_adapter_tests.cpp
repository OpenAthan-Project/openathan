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
  assert(!audio.start(::openathan::Track::NORMAL) && playback.starts == 0);
  assert(!audio.request_volume_percent(30));
  playback.available = true;
  // Startup can apply saved volume before power is enabled; playback waits for it.
  assert(audio.request_volume_percent(30) && audio.volume_percent() == 30 && !audio.ready());
  audio.loop(); assert(audio.ready() && pin.levels.back());
  audio.loop(); assert(pin.levels.size() == 2);
  assert(audio.start(::openathan::Track::FAJR) && audio.playing());
  audio.stop(); assert(!audio.playing() && playback.stops == 1);
  playback.available = false; audio.loop(); assert(!audio.ready() && !pin.levels.back());
  playback.available = true; audio.loop(); assert(audio.ready());
  dac.mark_failed(); assert(!audio.ready() && !audio.start(::openathan::Track::NORMAL));
  audio.loop(); assert(!pin.levels.back());
  audio.on_shutdown(); assert(!pin.levels.back());
  output::FloatOutput pwm; display::Display canvas; waveshare_box_display::Backlight light;
  light.set_output(&pwm); light.set_display(&canvas); light.setup();
  assert(!light.is_failed() && pwm.levels.back() == 0.5f);
  assert(!light.apply(0) && !light.apply(101) && pwm.levels.size() == 1);
  assert(light.apply(1) && pwm.levels.back() == 0.01f);
  assert(light.apply(100) && pwm.levels.back() == 1.0f);
  canvas.mark_failed(); assert(!light.apply(50) && pwm.levels.size() == 3);
}
