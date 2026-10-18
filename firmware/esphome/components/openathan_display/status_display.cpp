#include "status_display.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/hal.h"
#include <algorithm>
#include <cstring>

namespace esphome::openathan_display {
void StatusDisplay::setup() {
  const int size = round_ ? 360 : 128;
  if (!athan_ || !display_ || display_->is_failed() || display_->get_width() != size || display_->get_height() != size) {
    mark_failed(); return;  // An optional screen never changes scheduler health.
  }
  if (connection_reader_)
    athan_->set_physical_control_handler([this](unsigned action) { handle_control(action); });
  update();
}
void StatusDisplay::handle_control(unsigned action) {
  using ::openathan::screen::Page;
  if (!athan_ || action > 2) return;
  // Snapshot before Stop, which can synchronously clear a pending/loading
  // session. The press that stopped playback must never open another page.
  const auto connection = connection_reader_ ? connection_reader_() : ::openathan::screen::ConnectionInfo{};
  const bool busy = connection.playback_busy || athan_->playback_busy();
  if (action == 0) {
    athan_->stop();
    if (busy || is_failed() || !display_ || display_->is_failed() || !connection_reader_ ||
        !connection_cache_.read(connection).available) page_ = Page::STATUS;
    else if (connection.setup_connecting) page_ = Page::STATUS;
    else if (connection.setup_active)
      page_ = page_ == Page::STATUS ? (round_ ? Page::ADDRESS : Page::QR) : Page::STATUS;
    else if (page_ == Page::STATUS) page_ = connection.connected ? Page::QR : Page::ADDRESS;
    else if (page_ == Page::QR && !round_) page_ = Page::ADDRESS;
    else page_ = Page::STATUS;
  } else {
    page_ = Page::STATUS;
    if (action == 1) athan_->skip_next();
    else athan_->cancel_skip();
  }
  interaction_at_ = millis();
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
  const auto connection = connection_reader_ ? connection_reader_() : ::openathan::screen::ConnectionInfo{};
  const bool busy = connection.playback_busy || athan_->playback_busy();
  const bool setup_visible = connection.setup_active || connection.setup_connecting;
  if (setup_active_ != setup_visible) {
    page_ = setup_active_ && connection.connected && ::openathan::screen::valid_ipv4(connection.ipv4) ?
        ::openathan::screen::Page::QR : ::openathan::screen::Page::STATUS;
    interaction_at_ = millis();
    setup_active_ = setup_visible;
  }
  if (page_ != ::openathan::screen::Page::STATUS &&
      (busy || static_cast<uint32_t>(millis() - interaction_at_) >= 60000))
    page_ = ::openathan::screen::Page::STATUS;
  if (connection.setup_connecting) page_ = ::openathan::screen::Page::STATUS;
  if (page_ == ::openathan::screen::Page::QR && !connection.connected && !setup_visible)
    page_ = ::openathan::screen::Page::ADDRESS;
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
  const auto &connection_frame = connection_cache_.read(connection);
  const ::openathan::screen::Inputs input{status, athan_->activated(),
      std::strcmp(athan_->setup_state(), "storage_fault") == 0 || (service && !service->healthy()),
      clock.valid, connection_reader_ ? connection.connected : wifi::global_wifi_component && wifi::global_wifi_component->is_connected(),
      enabled, hhmm(local), hhmm(next), athan_->time_format_preferences().hours(), stop_button_,
      round_, next_event, clock.utc,
      athan_->playback() && athan_->playback()->source() == ::openathan::PlaybackSource::QURAN,
      connection_reader_ ? &connection_frame : nullptr, page_,
      connection_reader_ && stop_button_ && !busy,
      athan_->playback() && athan_->playback()->source() == ::openathan::PlaybackSource::DUA,
      athan_->dua_sequence().phase() == ::openathan::DuaSequence::Phase::PAUSE};
  if (cache_.accept(::openathan::screen::present(input))) display_->update();
}
}
