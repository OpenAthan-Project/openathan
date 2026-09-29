#include "lights.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); std::abort(); } } while (0)
using esphome::voice_pyramid::Lights;
int main() {
  Lights output; output.setup(); CHECK(output.writes.size()==32);
  for (const auto &w:output.writes) CHECK(w.reg<0xA0); // Never speaker reset, touch or flash commands.
  output.writes.clear();
  CHECK(output.apply({255,96,0,20})); CHECK(output.writes.size()==32);
  CHECK(output.writes[0].reg==0x10 && output.writes[0].data[0]==0);
  CHECK(output.writes[1].reg==0x11 && output.writes[1].data[0]==0);
  for (unsigned index=0;index<28;++index) {
    const auto &w=output.writes[index+2];
    CHECK(w.reg==(index<14?0x20:0x60)+(index%14)*4);
    CHECK(w.data==std::vector<uint8_t>({0,96,255,0}));
  }
  output.writes.clear(); CHECK(output.apply({255,96,0,20})); CHECK(output.writes.empty());
  CHECK(output.apply({255,96,0,10})); CHECK(output.writes.size()==2); // Pulse changes only brightness.
  for (unsigned fail=1;fail<=32;++fail) {
    Lights broken; broken.fail_at=fail;
    CHECK(!broken.apply({0,0,255,20}));
    broken.fail_at=0; broken.writes.clear();
    CHECK(broken.apply({0,0,255,20})); CHECK(broken.writes.size()==32);
    CHECK(broken.apply({}));
    CHECK(broken.writes.back().data[0]==0);
  }
  CHECK(!output.apply({0,0,0,101}));
}
