#include "validation.h"
#include "esphome/core/log.h"
#include <cstdio>
#include <cstring>

namespace esphome::openathan_validation {
static const char *const TAG = "oa_validation";
void Validation::setup() {
  scheduler_->set_calculator(this);
  scheduler_->set_state_store(this);
  if (nvs_open("oa_validation", NVS_READWRITE, &metadata_) != ESP_OK) return;
  char record[80]{};
  size_t size = sizeof(record);
  const auto err = nvs_get_str(metadata_, "timetable", record, &size);
  if (err == ESP_ERR_NVS_NOT_FOUND) { healthy_ = true; return; }
  long day;
  long long first;
  int end = 0;
  if (err != ESP_OK || std::sscanf(record, "OAT1 %ld %lld%n", &day, &first, &end) != 2 ||
      record[end] != '\0' || day < ::openathan::day_number({1900,1,1}) ||
      day > ::openathan::day_number({2100,12,31}) || first < 1 || first > 4133980799LL) return;
  const auto local = ESPTime::from_epoch_local(first);
  if (!local.is_valid() || ::openathan::day_number({local.year,local.month,local.day_of_month}) != day) return;
  day_ = day;
  first_ = first;
  healthy_ = true;
  ESP_LOGI(TAG, "Loaded timetable day=%ld first=%lld; no automatic restart", long(day_), first);
}
bool Validation::start_test() {
  const auto now = scheduler_->read();
  if (!healthy_ || !now.valid) return false;
  const auto last = ESPTime::from_epoch_local(now.utc + 180 + 4 * 120);
  const int32_t day = ::openathan::day_number(now.local_date);
  if (::openathan::day_number({last.year,last.month,last.day_of_month}) != day) return false;
  scheduler_->stop();
  // Invalidate the timetable first. A power failure during reset must not replay
  // an old timetable with cleared consumption. Only this test namespace is touched.
  const auto erase = nvs_erase_key(metadata_, "timetable");
  if (erase != ESP_OK && erase != ESP_ERR_NVS_NOT_FOUND) { healthy_ = false; return false; }
  if (nvs_commit(metadata_) != ESP_OK) { healthy_ = false; return false; }
  first_ = 0;
  ::openathan::DurableState empty;
  if (!store_.save(empty)) { healthy_ = false; scheduler_->reload_schedule(); return false; }
  char record[80];
  std::snprintf(record, sizeof(record), "OAT1 %ld %lld", long(day), static_cast<long long>(now.utc + 180));
  if (nvs_set_str(metadata_, "timetable", record) != ESP_OK || nvs_commit(metadata_) != ESP_OK) {
    healthy_ = false; scheduler_->reload_schedule(); return false;
  }
  first_ = now.utc + 180;
  day_ = day;
  ESP_LOGI(TAG, "START first=%lld spacing=120s day=%ld", static_cast<long long>(first_), long(day_));
  scheduler_->reload_schedule();
  return true;
}
bool Validation::calculate(const ::openathan::Settings &, ::openathan::CivilDate date, ::openathan::PrayerDay &out) {
  out = {};
  if (!healthy_) return false;
  if (first_ && ::openathan::day_number(date) == day_) {
    out = {first_, first_ + 1, first_ + 120, first_ + 240, first_ + 360, first_ + 480};
  }
  return true;
}
::openathan::LoadResult Validation::load(::openathan::DurableState &state) {
  return healthy_ ? store_.load(state) : ::openathan::LoadResult::ERROR;
}
bool Validation::save(const ::openathan::DurableState &state) {
  if (!healthy_ || !store_.save(state)) return false;
  ESP_LOGI(TAG, "Committed watermarks=%ld,%ld,%ld,%ld,%ld skip=%d", long(state.consumed_through[0]),
      long(state.consumed_through[1]), long(state.consumed_through[2]), long(state.consumed_through[3]),
      long(state.consumed_through[4]), state.skip ? static_cast<int>(state.skip->prayer) : -1);
  return true;
}
}  // namespace esphome::openathan_validation
