#pragma once
#include <stdexcept>
using esp_err_t = int;
using httpd_handle_t = void *;
struct httpd_req_t {};
inline bool pending_main_loop_request = false;
inline unsigned stop_calls = 0;
struct ShutdownWatchdog : std::runtime_error { ShutdownWatchdog() : std::runtime_error("HTTP stop waits for the main loop request timeout") {} };
inline esp_err_t httpd_stop(httpd_handle_t) {
  ++stop_calls;
  if (pending_main_loop_request) throw ShutdownWatchdog{};
  return 0;
}
