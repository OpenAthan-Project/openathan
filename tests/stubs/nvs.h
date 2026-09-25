#pragma once
#include <cstddef>
#include <cstdint>
using nvs_handle_t = uint32_t;
constexpr int ESP_OK = 0;
constexpr int ESP_ERR_NVS_NOT_FOUND = 1;
constexpr int NVS_READWRITE = 0;
int nvs_open(const char *, int, nvs_handle_t *);
void nvs_close(nvs_handle_t);
int nvs_get_blob(nvs_handle_t, const char *, void *, size_t *);
int nvs_set_blob(nvs_handle_t, const char *, const void *, size_t);
int nvs_commit(nvs_handle_t);
