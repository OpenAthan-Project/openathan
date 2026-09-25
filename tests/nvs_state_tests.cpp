#include "nvs_state_store.h"
#include <cstdlib>
#include <cstring>
#include <optional>
#include <vector>

static void check(bool value) { if (!value) std::abort(); }
static std::optional<std::vector<uint8_t>> blob;
static std::optional<std::vector<uint8_t>> isolated_blob;
static bool fail_open=false, fail_read=false, fail_write=false, fail_commit=false;
static int commits=0;
static const char *expected_namespace="openathan";
int nvs_open(const char *name, int, nvs_handle_t *handle) {
  check(std::strcmp(name,expected_namespace)==0);
  *handle=std::strcmp(name,"openathan")==0 ? 1 : 2;
  return fail_open ? -1 : ESP_OK;
}
void nvs_close(nvs_handle_t) {}
int nvs_get_blob(nvs_handle_t handle, const char *key, void *data, size_t *length) {
  const auto &blob = handle == 1 ? ::blob : isolated_blob;
  check(std::strcmp(key,"scheduler")==0);
  if (fail_read) return -1;
  if (!blob) return ESP_ERR_NVS_NOT_FOUND;
  if (data) {
    if (*length < blob->size()) return -1;
    std::memcpy(data,blob->data(),blob->size());
  }
  *length=blob->size();
  return ESP_OK;
}
int nvs_set_blob(nvs_handle_t handle, const char *, const void *data, size_t length) {
  auto &blob = handle == 1 ? ::blob : isolated_blob;
  if (fail_write) return -1;
  auto *bytes=static_cast<const uint8_t *>(data);
  blob=std::vector<uint8_t>(bytes,bytes+length);
  return ESP_OK;
}
int nvs_commit(nvs_handle_t) { ++commits; return fail_commit ? -1 : ESP_OK; }

int main() {
  using namespace openathan;
  using esphome::openathan_component::NvsStateStore;
  DurableState out;
  NvsStateStore store;
  check(!store.save(out));
  fail_open=true; check(store.load(out)==LoadResult::ERROR);
  fail_open=false; check(store.load(out)==LoadResult::EMPTY);
  DurableState value;
  value.skip=EventKey{day_number({2026,4,2}),Prayer::FAJR};
  check(store.save(value) && commits==1);
  NvsStateStore restarted;
  check(restarted.load(out)==LoadResult::LOADED && out==value);
  fail_read=true; check(restarted.load(out)==LoadResult::ERROR); fail_read=false;
  blob->at(5)^=1; check(restarted.load(out)==LoadResult::ERROR);
  blob=std::vector<uint8_t>(1,0); check(restarted.load(out)==LoadResult::ERROR);
  blob=std::vector<uint8_t>(100,0); check(restarted.load(out)==LoadResult::ERROR);
  fail_write=true; check(!store.save(value) && commits==1); fail_write=false;
  fail_commit=true; check(!store.save(value) && commits==2); fail_commit=false;
  check(store.save(value) && commits==3);
  check(restarted.load(out)==LoadResult::LOADED && out==value);
  expected_namespace="oa_validation";
  NvsStateStore isolated("oa_validation");
  check(isolated.load(out)==LoadResult::EMPTY);
  check(isolated.save(DurableState{}));
  check(restarted.load(out)==LoadResult::LOADED && out==value);
  check(isolated.load(out)==LoadResult::LOADED && out==DurableState{});
}
