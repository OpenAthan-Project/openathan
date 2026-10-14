#pragma once
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

namespace esphome {
using std::make_unique;
using std::optional;
template<class T> struct RAMAllocator {
  T *allocate(size_t count) { return static_cast<T *>(std::malloc(count * sizeof(T))); }
  T *reallocate(T *pointer, size_t count) { return static_cast<T *>(std::realloc(pointer, count * sizeof(T))); }
  void deallocate(T *pointer, size_t) { std::free(pointer); }
};
inline bool str_contains_ignore_case(const char *value, const char *part) {
  std::string text(value), needle(part);
  auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
  std::transform(text.begin(), text.end(), text.begin(), lower);
  std::transform(needle.begin(), needle.end(), needle.begin(), lower);
  return text.find(needle) != std::string::npos;
}
inline bool str_endswith_ignore_case(const char *value, const char *suffix) {
  std::string text(value), end(suffix);
  return text.size() >= end.size() && str_contains_ignore_case(text.c_str() + text.size() - end.size(), suffix);
}
}
