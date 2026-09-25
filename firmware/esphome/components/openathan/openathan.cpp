#include "openathan.h"
#include "esphome/core/log.h"
#include <esp_timer.h>
#include <cstdio>
#include <ctime>
#include <sys/time.h>

namespace esphome::openathan_component {
static const char *const TAG = "openathan";
static ::openathan::DstRule stored_rule(const time::DSTRule &r) {
  return {r.time_seconds, r.day, static_cast<::openathan::DstRuleType>(r.type), r.month, r.week, r.day_of_week};
}
static time::DSTRule applied_rule(const ::openathan::DstRule &r) {
  return {r.time_seconds, r.day, static_cast<time::DSTRuleType>(r.type), r.month, r.week, r.day_of_week};
}
void OpenAthan::setup() {
  scheduler_ = std::make_unique<::openathan::Scheduler>(*this, *calculator_source_, *state_store_, *playback_);
  settings_service_ = std::make_unique<::openathan::SettingsService>(*settings_store_);
  const auto &tz = time::get_global_tz();
  ::openathan::DeviceSettings defaults{settings_,
      {timezone_name_, tz.std_offset_seconds, tz.has_dst() ? tz.dst_offset_seconds : 0,
       stored_rule(tz.dst_start), stored_rule(tz.dst_end)},
      static_cast<uint8_t>(playback_->volume_percent().value_or(70))};
  // Load independently: a settings upgrade must never overwrite consumption.
  bool gate_ok = true;
  if (setup_gate_) {
    ::openathan::SavedSettings existing;
    ::openathan::DurableState history;
    gate_ok = setup_gate_->begin(settings_store_->load(existing), state_store_->load(history));
    defaults.volume = 70;
  }
  const bool settings_ok = gate_ok && settings_service_->begin(defaults);
  scheduler_->begin(settings_ok ? settings_service_->saved()->value.prayer : settings_);
  if (!settings_ok) scheduler_->block_storage();
  scheduler_->allow_playback(false);
  apply_volume_();
  if (settings_ok) {
    const auto &saved = *settings_service_->saved();
    const auto record = ::openathan::encode_settings(saved);
    ESP_LOGI(TAG, "Restored settings revision=%lu crc=%08lx clock=%s", static_cast<unsigned long>(saved.revision),
        static_cast<unsigned long>(::openathan::read_u32(record.data() + record.size() - 4)),
        read().valid ? "ready" : "waiting");
  } else {
    ESP_LOGE(TAG, "Settings unavailable; automatic playback blocked, NVS retained");
  }
  clock_->add_on_time_sync_callback([this]() { this->update(); });
  update();
}
void OpenAthan::reload_schedule() {
  if (scheduler_ && settings_service_ && settings_service_->healthy()) {
    scheduler_->begin(settings_service_->saved()->value.prayer);
    scheduler_->allow_playback(volume_applied_ && activated());
    update();
  }
}
bool OpenAthan::local_date(int64_t utc, const ::openathan::Timezone &tz, ::openathan::CivilDate &date) const {
  const time::ParsedTimezone parsed{tz.standard_offset, tz.daylight_offset, applied_rule(tz.start), applied_rule(tz.end)};
  tm local{};
  if (!time::epoch_to_local_tm(utc, parsed, &local)) return false;
  date = {local.tm_year+1900, static_cast<unsigned>(local.tm_mon+1), static_cast<unsigned>(local.tm_mday)};
  return ::openathan::valid_date(date);
}
bool OpenAthan::local_date(int64_t utc, ::openathan::CivilDate &date) const {
  return settings_service_ && settings_service_->saved() &&
      local_date(utc, settings_service_->saved()->value.timezone, date);
}
std::string OpenAthan::format_local(int64_t utc, const ::openathan::Timezone &tz) const {
  const time::ParsedTimezone parsed{tz.standard_offset, tz.daylight_offset, applied_rule(tz.start), applied_rule(tz.end)};
  tm local{};
  if (!time::epoch_to_local_tm(utc, parsed, &local)) return {};
  char text[32];
  if (std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M", &local) == 0) return {};
  return text;
}
::openathan::ClockSample OpenAthan::read() {
  timeval wall{};
  if (gettimeofday(&wall, nullptr) != 0) return {};
  ::openathan::CivilDate date;
  const bool valid = ESPTime::from_epoch_utc(wall.tv_sec).is_valid() && local_date(wall.tv_sec, date);
  return {valid, wall.tv_sec, static_cast<uint64_t>(esp_timer_get_time() / 1000), date,
          static_cast<uint16_t>(wall.tv_usec / 1000)};
}
::openathan::SettingsResult OpenAthan::change_settings(const ::openathan::DeviceSettings &candidate, uint32_t revision) {
  using ::openathan::SettingsResult;
  if (!settings_service_ || !scheduler_) return SettingsResult::STORAGE;
  const auto old = settings_service_->saved();
  const auto result = settings_service_->update(candidate, revision, [this, &old](const auto &value) {
    if (old && old->value.prayer == value.prayer && old->value.timezone == value.timezone) return true;
    const auto now = read();
    if (!now.valid) return true;
    ::openathan::CivilDate date;
    return local_date(now.utc, value.timezone, date) && scheduler_->validate_schedule(value.prayer, date);
  });
  if (result == SettingsResult::STORAGE) scheduler_->block_storage();
  if (result != SettingsResult::SAVED) return result;
  const bool schedule_changed = !old || old->value.prayer != candidate.prayer || old->value.timezone != candidate.timezone;
  if (schedule_changed) scheduler_->configure(candidate.prayer);
  if (!old || old->value.volume != candidate.volume) {
    volume_applied_ = false; volume_attempts_ = 0; next_volume_check_ = 0;
    apply_volume_();
  }
  // Do not introduce an extra scheduler poll/rearm for a volume-only save.
  if (schedule_changed) update();
  return result;
}
void OpenAthan::apply_volume_() {
  if (!settings_service_ || !settings_service_->healthy() || !scheduler_) return;
  const unsigned wanted = settings_service_->saved()->value.volume;
  volume_applied_ = playback_->volume_percent() == std::optional<unsigned>(wanted);
  scheduler_->allow_playback(volume_applied_ && activated());
  if (!volume_applied_) {
    ++volume_attempts_;
    playback_->request_volume_percent(wanted);
  } else volume_attempts_ = 0;
}
void OpenAthan::loop() {
  const auto now = static_cast<uint64_t>(esp_timer_get_time() / 1000);
  if (now >= next_volume_check_) {
    apply_volume_();
    next_volume_check_ = now + 250;
  }
}
const char *OpenAthan::settings_application_status() const {
  if (!settings_service_ || !settings_service_->healthy()) return "storage_fault";
  if (volume_applied_) return "applied";
  return volume_attempts_ >= 20 ? "volume_failed" : "volume_pending";
}
::openathan::SchedulerStatus OpenAthan::status() const {
  return scheduler_ ? scheduler_->status() : ::openathan::SchedulerStatus{};
}
void OpenAthan::update() {
  if (!scheduler_) return;
  apply_volume_();
  if (activated()) scheduler_->tick();
  log_status_();
}
void OpenAthan::stop() {
  if (scheduler_) scheduler_->stop();
  log_status_();
}
bool OpenAthan::skip_next() {
  const bool ok = activated() && scheduler_ && scheduler_->skip_next();
  if (!ok) { ESP_LOGW(TAG, "Skip request rejected; check clock, next prayer and storage"); }
  log_status_();
  return ok;
}
bool OpenAthan::cancel_skip() {
  const bool ok = activated() && scheduler_ && scheduler_->cancel_skip();
  if (!ok) { ESP_LOGW(TAG, "Cancel skip rejected; durable state unavailable"); }
  log_status_();
  return ok;
}
const char *OpenAthan::setup_state() const {
  if (!setup_gate_) return "active";
  if (!setup_gate_->healthy()) return "storage_fault";
  return setup_gate_->active() ? "active" : "incomplete";
}
bool OpenAthan::finish_setup(uint32_t revision) {
  if (!settings_service_ || !settings_service_->healthy() || !settings_service_->saved() ||
      settings_service_->saved()->revision != revision || !scheduler_ ||
      scheduler_->status().fault == ::openathan::Fault::STORAGE) return false;
  const auto now = read();
  if (now.valid && !scheduler_->validate_schedule(settings_service_->saved()->value.prayer, now.local_date)) return false;
  if (setup_gate_ && !setup_gate_->activate()) { scheduler_->block_storage(); return false; }
  update();
  return true;
}
bool OpenAthan::preview(const ::openathan::DeviceSettings &settings, ::openathan::PrayerDay &day,
                       std::vector<::openathan::Event> &events,
                       std::vector<::openathan::ScheduleConflict> &conflicts) {
  const auto now = read();
  ::openathan::CivilDate date;
  return scheduler_ && now.valid && local_date(now.utc, settings.timezone, date) &&
      scheduler_->preview(settings.prayer, date, day, events, conflicts);
}
bool OpenAthan::skip_occurrence(const ::openathan::Event &expected) {
  return activated() && scheduler_ && scheduler_->skip_next(expected);
}
bool OpenAthan::cancel_occurrence(::openathan::EventKey expected) {
  return activated() && scheduler_ && scheduler_->status().skip == expected && scheduler_->cancel_skip();
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
    if (s.fault == ::openathan::Fault::NONE) { ESP_LOGI(TAG, "%s", message); }
    else { ESP_LOGW(TAG, "%s", message); }
  }
  std::string conflicts;
  for (const auto &conflict : s.conflicts) conflicts += ::openathan::describe_conflict(conflict) + "\n";
  if (last_conflicts_ != conflicts) {
    last_conflicts_ = conflicts;
    if (conflicts.empty()) { ESP_LOGI(TAG, "No Isha/Fajr conflicts in the schedule window"); }
    else {
      for (const auto &conflict : s.conflicts) {
        ESP_LOGW(TAG, "%s", ::openathan::describe_conflict(conflict).c_str());
      }
    }
  }
}
}  // namespace esphome::openathan_component
