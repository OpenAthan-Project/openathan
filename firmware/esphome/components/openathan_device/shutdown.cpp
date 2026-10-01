#include "device.h"

namespace esphome::openathan_device {
void Device::on_shutdown() {
  upgrade_.shutdown();
  // Shutdown runs on the main loop. httpd_stop can wait for a handler that
  // is itself waiting for this loop, exhausting the watchdog deadline.
  // Restart/power-down terminates the server tasks and sockets; keep this
  // hook nonblocking after revoking updater work.
}
}  // namespace esphome::openathan_device
