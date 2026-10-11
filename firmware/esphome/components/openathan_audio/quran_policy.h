#pragma once
#include <string>
#include <cctype>

namespace esphome::openathan_device {
// Explicit provider hosts, no credentials, ports, queries or fragments.
inline bool quran_audio_url(const std::string &url) {
  if (url.size() > 512) return false;
  const auto start = url.find('/', 8);
  if (url.rfind("https://", 0) != 0 || start == std::string::npos) return false;
  const auto host = url.substr(8, start - 8);
  bool allowed = host == "cdn.mp3quran.net";
  if (host.rfind("server", 0) == 0 && host.size() > 6 + 13 &&
      host.compare(host.size() - 13, 13, ".mp3quran.net") == 0) {
    const auto number = host.substr(6, host.size() - 6 - 13);
    allowed = !number.empty() && number.size() <= 3;
    for (char c : number) allowed = allowed && c >= '0' && c <= '9';
  }
  if (!allowed) return false;
  for (size_t i = start; i < url.size(); ++i) {
    const unsigned char c = url[i];
    if (!(std::isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.')) return false;
  }
  return url.find("..", start) == std::string::npos;
}
// Extract top-level array objects incrementally; bounds apply before JSON parsing.
class QuranObjectStream {
 public:
  explicit QuranObjectStream(std::string key) : prefix_("{\"" + key + "\":[") {}
  bool feed(char c, std::string &object) {
    object.clear();
    if (++bytes_ > 1048576 || failed_) { failed_ = true; return false; }
    if (prefix_pos_ < prefix_.size()) {
      if (std::isspace(static_cast<unsigned char>(c)) &&
          (prefix_pos_ <= 1 || prefix_pos_ >= prefix_.size() - 2)) return true;
      if (c != prefix_[prefix_pos_++]) return fail_();
      return true;
    }
    if (depth_) {
      if (item_.size() >= 16384) return fail_();
      item_ += c;
      if (quoted_) {
        if (escaped_) escaped_ = false;
        else if (c == '\\') escaped_ = true;
        else if (c == '"') quoted_ = false;
      } else if (c == '"') quoted_ = true;
      else if (c == '{') ++depth_;
      else if (c == '}' && --depth_ == 0) { object.swap(item_); separator_ = true; }
      return true;
    }
    if (std::isspace(static_cast<unsigned char>(c))) return true;
    if (finished_) return fail_();
    if (closed_) { if (c != '}') return fail_(); finished_ = true; return true; }
    if (separator_) {
      if (c == ',') { separator_ = false; comma_ = true; return true; }
      if (c == ']') { closed_ = true; return true; }
      return fail_();
    }
    if (c == ']' && !comma_) { closed_ = true; return true; }
    if (c != '{') return fail_();
    comma_ = false; depth_ = 1; item_ = "{"; return true;
  }
  bool complete() const { return finished_ && !failed_; }
 private:
  bool fail_() { failed_ = true; return false; }
  std::string prefix_, item_;
  size_t prefix_pos_{}, bytes_{};
  unsigned depth_{};
  bool quoted_{}, escaped_{}, separator_{}, comma_{}, closed_{}, finished_{}, failed_{};
};
}
