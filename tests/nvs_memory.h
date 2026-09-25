#pragma once
#include <nvs.h>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <cstring>
#include <stdexcept>

// Models both permitted outcomes of an unacknowledged NVS write. A commit
// success always survives power loss; implementations may persist earlier.
namespace nvs_test {
using Key = std::pair<std::string, std::string>;
inline std::map<Key, std::vector<uint8_t>> committed, pending;
inline std::map<nvs_handle_t, std::string> handles;
inline std::set<std::string> erased;
inline unsigned cut_write_number{}, cut_commit_number{};
inline unsigned next_handle = 1, commits = 0, writes = 0;
inline bool fail_open, fail_read, fail_write, fail_commit, early_persist, cut_after_write, cut_after_commit;
inline void reset() {
  committed.clear(); pending.clear(); handles.clear(); next_handle = 1; commits = writes = 0;
  erased.clear(); cut_write_number = cut_commit_number = 0;
  fail_open = fail_read = fail_write = fail_commit = early_persist = cut_after_write = cut_after_commit = false;
}
inline void power_cycle() { pending.clear(); erased.clear(); }
}
int nvs_open(const char *name, int, nvs_handle_t *handle) {
  if (nvs_test::fail_open) return -1;
  *handle = nvs_test::next_handle++; nvs_test::handles[*handle] = name; return ESP_OK;
}
void nvs_close(nvs_handle_t handle) { nvs_test::handles.erase(handle); }
int nvs_get_blob(nvs_handle_t handle, const char *key, void *data, size_t *length) {
  if (nvs_test::fail_read) return -1;
  const auto found = nvs_test::committed.find({nvs_test::handles.at(handle), key});
  if (found == nvs_test::committed.end()) return ESP_ERR_NVS_NOT_FOUND;
  if (data) {
    if (*length < found->second.size()) return -1;
    std::memcpy(data, found->second.data(), found->second.size());
  }
  *length = found->second.size(); return ESP_OK;
}
int nvs_set_blob(nvs_handle_t handle, const char *key, const void *data, size_t length) {
  ++nvs_test::writes;
  if (nvs_test::fail_write) return -1;
  const nvs_test::Key k{nvs_test::handles.at(handle), key};
  const auto *bytes = static_cast<const uint8_t *>(data);
  nvs_test::pending[k] = {bytes, bytes+length};
  if (nvs_test::early_persist) nvs_test::committed[k] = nvs_test::pending[k];
  if (nvs_test::cut_after_write || nvs_test::writes == nvs_test::cut_write_number) throw std::runtime_error("power loss during write");
  return ESP_OK;
}
int nvs_commit(nvs_handle_t handle) {
  ++nvs_test::commits;
  if (nvs_test::fail_commit) return -1;
  const auto name = nvs_test::handles.at(handle);
  if (nvs_test::erased.erase(name)) {
    for (auto it = nvs_test::committed.begin(); it != nvs_test::committed.end();) {
      if (it->first.first == name) it = nvs_test::committed.erase(it); else ++it;
    }
  }
  for (auto it = nvs_test::pending.begin(); it != nvs_test::pending.end();) {
    if (it->first.first == name) {
      nvs_test::committed[it->first] = it->second; it = nvs_test::pending.erase(it);
    } else ++it;
  }
  if (nvs_test::cut_after_commit || nvs_test::commits == nvs_test::cut_commit_number) throw std::runtime_error("power loss after commit");
  return ESP_OK;
}
int nvs_erase_all(nvs_handle_t handle) {
  ++nvs_test::writes;
  if (nvs_test::fail_write) return -1;
  const auto name = nvs_test::handles.at(handle);
  nvs_test::erased.insert(name);
  for (auto it = nvs_test::pending.begin(); it != nvs_test::pending.end();) {
    if (it->first.first == name) it = nvs_test::pending.erase(it); else ++it;
  }
  if (nvs_test::early_persist) {
    for (auto it = nvs_test::committed.begin(); it != nvs_test::committed.end();) {
      if (it->first.first == name) it = nvs_test::committed.erase(it); else ++it;
    }
  }
  if (nvs_test::cut_after_write || nvs_test::writes == nvs_test::cut_write_number) throw std::runtime_error("power loss during erase");
  return ESP_OK;
}
