#include "rtc.h"
#include "esphome/core/log.h"
#include "openathan/scheduler.h"
#include <array>

namespace esphome::waveshare_box_rtc {
namespace {
constexpr uint8_t MARKER = 0xA7;
constexpr uint8_t STOP = 0x20;
// Reject external-test, STOP, software-reset and 12-hour modes. On write,
// preserve only crystal-load selection and correction-interrupt enable.
constexpr uint8_t INVALID_CONTROL = 0xB2;
constexpr uint8_t PRESERVE_CONTROL = 0x05;
static const char *const TAG = "waveshare_rtc";

bool bcd(uint8_t raw, uint8_t mask, unsigned &out) {
  if ((raw & ~mask) || (raw & 0x0F) > 9 || (raw >> 4) > 9) return false;
  out = (raw >> 4) * 10 + (raw & 0x0F);
  return true;
}
uint8_t encoded(unsigned value) { return uint8_t((value / 10 << 4) | value % 10); }

bool decode(const std::array<uint8_t, 11> &raw, ESPTime &out) {
  if ((raw[0] & INVALID_CONTROL) || (raw[4] & 0x80)) return false;
  unsigned second, minute, hour, day, month, year;
  if (!bcd(raw[4], 0x7F, second) || !bcd(raw[5], 0x7F, minute) ||
      !bcd(raw[6], 0x3F, hour) || !bcd(raw[7], 0x3F, day) ||
      !bcd(raw[9], 0x1F, month) || !bcd(raw[10], 0xFF, year) ||
      second > 59 || minute > 59 || hour > 23 || raw[8] > 6) return false;
  const ::openathan::CivilDate date{int(year + 2000), month, day};
  if (date.year < 2019 || !::openathan::valid_date(date)) return false;
  const int64_t epoch = int64_t(::openathan::day_number(date)) * 86400 + hour * 3600 + minute * 60 + second;
  out = ESPTime::from_epoch_utc(epoch);
  return out.is_valid() && out.day_of_week == raw[8] + 1;
}
}  // namespace

void RTC::setup() {
  network_time_->add_on_time_sync_callback([this]() { this->save_(); });
  // I2C is initialized; restore before SNTP/network startup and scheduler setup.
  restore_();
}

void RTC::restore_() {
  if (utcnow().is_valid()) { restore_result_ = "system_valid"; return; }
  std::array<uint8_t, 11> raw{};
  if (!read_bytes(0, raw.data(), raw.size())) { restore_result_ = "io_error"; return; }
  if (raw[3] != MARKER) { restore_result_ = "uninitialized"; return; }
  ESPTime stored{};
  if (!decode(raw, stored)) { restore_result_ = "invalid"; return; }
  // A network result may have arrived during the bounded bus read.
  if (utcnow().is_valid()) { restore_result_ = "system_valid"; return; }
  synchronize_epoch_(uint32_t(stored.timestamp));
  restore_result_ = utcnow().is_valid() ? "restored" : "time_invalid";
  ESP_LOGI(TAG, "Startup RTC: %s", restore_result_);
}

void RTC::save_() {
  const auto now = network_time_->utcnow();
  if (!now.is_valid() || now.year > 2099 || now.second > 59) { save_result_ = "time_invalid"; return; }
  std::array<uint8_t, 11> raw{};
  if (!read_bytes(0, raw.data(), raw.size())) { save_result_ = "io_error"; return; }
  // Invalidate first, then stop the counters. Partial/interrupted writes cannot
  // leave a trusted date. Offset, Control_2, alarms and timer are untouched.
  const uint8_t control = raw[0] & PRESERVE_CONTROL;
  const std::array<uint8_t, 7> value{encoded(now.second), encoded(now.minute), encoded(now.hour),
      encoded(now.day_of_month), uint8_t(now.day_of_week - 1), encoded(now.month), encoded(now.year - 2000)};
  save_result_ = "io_error";
  if (!write_byte(3, 0) || !write_byte(0, control | STOP) ||
      !write_bytes(4, value.data(), value.size()) || !write_byte(0, control)) return;
  ESPTime verified{};
  if (!read_bytes(0, raw.data(), raw.size())) return;
  if (!decode(raw, verified) || verified.timestamp < now.timestamp || verified.timestamp > now.timestamp + 2) {
    save_result_ = "readback_error";
    return;
  }
  // Commit the UTC/year convention only after a complete verified write.
  if (!write_byte(3, MARKER)) return;
  uint8_t marker{};
  if (!read_bytes(3, &marker, 1)) return;
  save_result_ = marker == MARKER ? "verified" : "readback_error";
  ESP_LOGI(TAG, "SNTP to RTC: %s", save_result_);
}
}  // namespace esphome::waveshare_box_rtc
