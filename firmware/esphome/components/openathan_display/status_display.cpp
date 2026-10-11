#include "status_display.h"
#include "esphome/components/wifi/wifi_component.h"
#include <algorithm>
#include <cstring>

namespace esphome::openathan_display {
void StatusDisplay::setup() {
  const int size = round_ ? 360 : 128;
  if (!athan_ || !display_ || display_->is_failed() || display_->get_width() != size || display_->get_height() != size) {
    mark_failed(); return;  // An optional screen never changes scheduler health.
  }
  update();
}
void StatusDisplay::draw(display::Display &canvas) const {
  canvas.fill(Color::BLACK);
  const auto pixel = [&canvas](int x, int y, uint32_t rgb) {
    canvas.draw_pixel_at(x, y, Color((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255));
  };
  if (round_) ::openathan::screen::render_round(cache_.frame(), pixel);
  else ::openathan::screen::render(cache_.frame(), pixel);
}
void StatusDisplay::update() {
  if (is_failed() || !display_ || display_->is_failed()) return;
  const auto clock = athan_->read();
  const auto status = athan_->status();
  const auto *service = athan_->settings_service();
  const auto *saved = service && service->saved() ? &*service->saved() : nullptr;
  const auto next_event = round_ ? athan_->next_visual_prayer(clock) : status.next;
  std::string local, next;
  if (saved && clock.valid) {
    local = athan_->format_local(clock.utc, saved->value.timezone);
    if (next_event) next = athan_->format_local(next_event->utc, saved->value.timezone);
  }
  // format_local uses the saved timezone and its recurring DST rules, never the SNTP UTC zone.
  const auto hhmm = [](const std::string &value) -> std::string_view {
    return value.size() == 16 ? std::string_view(value).substr(11, 5) : std::string_view{};
  };
  const bool enabled = saved && std::any_of(saved->value.prayer.enabled.begin(), saved->value.prayer.enabled.end(),
                                         [](bool value) { return value; });
  const ::openathan::screen::Inputs input{status, athan_->activated(),
      std::strcmp(athan_->setup_state(), "storage_fault") == 0 || (service && !service->healthy()),
      clock.valid, wifi::global_wifi_component && wifi::global_wifi_component->is_connected(),
      enabled, hhmm(local), hhmm(next), athan_->time_format_preferences().hours(), stop_button_,
      round_, next_event, clock.utc,
      athan_->playback() && athan_->playback()->source() == ::openathan::PlaybackSource::QURAN};
  if (cache_.accept(::openathan::screen::present(input))) display_->update();
}
}
