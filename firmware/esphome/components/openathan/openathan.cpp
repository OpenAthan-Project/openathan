#include "openathan.h"
#include "esphome/core/log.h"
#include <esp_timer.h>
#include <cstdio>
#include <sys/time.h>

namespace esphome::openathan_component {
static const char *const TAG = "openathan";
void OpenAthan::setup() {
  scheduler_ = std::make_unique<::openathan::Scheduler>(*this, *calculator_source_, *state_store_, *playback_);
  scheduler_->begin(settings_);
  clock_->add_on_time_sync_callback([this]() { this->update(); });
  update();
}
void OpenAthan::reload_schedule() {
  if (scheduler_) { scheduler_->begin(settings_); update(); }
}
::openathan::ClockSample OpenAthan::read() {
  // ESPHome time sources synchronize the system clock. Convert this exact sample
  // using ESPHome's parsed timezone, so date/UTC/subseconds cannot straddle a tick.
  timeval wall{};
  if (gettimeofday(&wall, nullptr) != 0) return {};
  const auto local = ESPTime::from_epoch_local(wall.tv_sec);
  return {local.is_valid(), wall.tv_sec, static_cast<uint64_t>(esp_timer_get_time() / 1000),
          {local.year, local.month, local.day_of_month}, static_cast<uint16_t>(wall.tv_usec / 1000)};
}
::openathan::SchedulerStatus OpenAthan::status() const {
  return scheduler_ ? scheduler_->status() : ::openathan::SchedulerStatus{};
}
void OpenAthan::update() {
  if (!scheduler_) return;
  scheduler_->tick();
  log_status_();
}
void OpenAthan::stop() {
  if (scheduler_) scheduler_->stop();
  log_status_();
}
bool OpenAthan::skip_next() {
  const bool ok = scheduler_ && scheduler_->skip_next();
  if (!ok) ESP_LOGW(TAG, "Skip request rejected; check clock, next prayer and storage");
  log_status_();
  return ok;
}
bool OpenAthan::cancel_skip() {
  const bool ok = scheduler_ && scheduler_->cancel_skip();
  if (!ok) ESP_LOGW(TAG, "Cancel skip rejected; durable state unavailable");
  log_status_();
  return ok;
}
void OpenAthan::log_status_() {
  const auto s = status();
  char message[256];
  std::snprintf(message, sizeof(message), "clock=%s automatic=%s playing=%s fault=%s next=%s UTC=%lld skip=%s day=%ld",
      s.clock_ready ? "ready" : "waiting", s.automatic_ready ? "ready" : "blocked", s.playing ? "yes" : "no",
      ::openathan::fault_name(s.fault), s.next ? ::openathan::prayer_name(s.next->key.prayer) : "none",
      s.next ? static_cast<long long>(s.next->utc) : 0LL,
      s.skip ? ::openathan::prayer_name(s.skip->prayer) : "none", s.skip ? static_cast<long>(s.skip->day) : 0L);
  if (last_status_ != message) {
    last_status_ = message;
    if (s.fault == ::openathan::Fault::NONE) ESP_LOGI(TAG, "%s", message);
    else ESP_LOGW(TAG, "%s", message);
  }
  std::string conflicts;
  for (const auto &conflict : s.conflicts) conflicts += ::openathan::describe_conflict(conflict) + "\n";
  if (last_conflicts_ != conflicts) {
    last_conflicts_ = conflicts;
    if (conflicts.empty()) ESP_LOGI(TAG, "No Isha/Fajr conflicts in the schedule window");
    else for (const auto &conflict : s.conflicts)
      ESP_LOGW(TAG, "%s", ::openathan::describe_conflict(conflict).c_str());
  }
}
}  // namespace esphome::openathan_component
