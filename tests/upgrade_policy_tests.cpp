#include "upgrade_policy.h"
#include <cassert>
using namespace esphome::openathan_device;
int main() {
  assert(newer_release("v0.2.0", "v0.1.0"));
  assert(newer_release("v1.10.0", "v1.9.99"));
  assert(!newer_release("v0.2.0", "v0.2.0"));
  assert(!newer_release("v0.1.0", "v0.2.0"));
  for (const char *invalid : {"", "1.2.3", "v1.2.3-rc1", "v01.2.3", "v1.2.3.4", "v4294967296.0.0", "v1.2", "v1..2", "v-1.2.3"})
    assert(!release_version(invalid));
  assert(upgrade_window(false, true, true, true, 1000, 1900));
  assert(!upgrade_window(false, true, true, true, 1000, 1899));
  assert(!upgrade_window(false, true, true, true, 1000, 999));
  assert(!upgrade_window(true, true, true, true, 1000, 9000));
  assert(!upgrade_window(false, false, true, true, 1000, 9000));
  assert(!upgrade_window(false, true, true, false, 1000, 9000));
  assert(upgrade_window(false, true, true, true, 1000, {}));
  assert(upgrade_window(false, true, false, false, 1000, {}));
}
