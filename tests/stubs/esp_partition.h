#pragma once
#include <cstddef>
#include <cstdint>
using esp_err_t = int;
using esp_partition_mmap_handle_t = unsigned;
constexpr int ESP_OK = 0;
constexpr int ESP_PARTITION_TYPE_DATA = 1;
constexpr int ESP_PARTITION_SUBTYPE_ANY = 255;
constexpr int ESP_PARTITION_MMAP_DATA = 0;
struct esp_partition_t { size_t size; };
const esp_partition_t *esp_partition_find_first(int, int, const char *);
int esp_partition_mmap(const esp_partition_t *, size_t, size_t, int, const void **, esp_partition_mmap_handle_t *);
void esp_partition_munmap(esp_partition_mmap_handle_t);
inline const char *esp_err_to_name(int) { return "injected failure"; }
