#include "esphome/components/waveshare_audio_board/board.h"
#include "esphome/components/waveshare_audio_board/ring.h"
#include <cassert>
#include <vector>
using namespace esphome;
int main() {
 using waveshare_audio_board::Board;
 Board b;
 b.regs[5]=0xf7; b.regs[7]=0xe1;
 b.setup();
 assert(b.ready() && b.regs[3]==0xfe && b.regs[7]==0xee && b.regs[5]==0xf1);
 assert(b.writes.front()==std::make_pair(uint8_t(3),uint8_t(0xfe)));
 assert(b.regs[2]==0xff && b.regs[6]==0xff); // Bank 0 untouched.
 assert(b.amplifier(true) && b.regs[3]==0xff);
 assert(b.amplifier(false) && b.regs[3]==0xfe);
 for (unsigned fail=1;fail<=3;++fail) {
  Board broken; broken.fail_at=fail; broken.setup();
  assert(!broken.ready() && !broken.amplifier(true));
  assert(!(broken.regs[3]&1));
 }
 Board missing; missing.fail_read=true; missing.setup(); assert(!missing.ready());
 Board stale; stale.bad_readback=true; stale.setup(); assert(!stale.ready());
 std::vector<unsigned> keys;
 b.set_key_handler([&](unsigned key){keys.push_back(key);});
 // Initial held key is ignored; only a stable release can arm it.
 b.regs[1]=0xfd;
 for(unsigned t=20;t<=200;t+=20)b.poll(t);
 assert(keys.empty());
 b.regs[1]=0xff;
 for(unsigned t=220;t<=300;t+=20)b.poll(t);
 b.regs[1]=0xfd; b.poll(320); b.regs[1]=0xff; b.poll(340);
 b.regs[1]=0xfd;
 for(unsigned t=360;t<=600;t+=20)b.poll(t);
 assert(keys==std::vector<unsigned>{0}); // Debounced, no hold-repeat.
 b.fail_read=true; b.poll(620); b.fail_read=false;
 for(unsigned t=640;t<=800;t+=20)b.poll(t);
 assert(keys.size()==1); // A held key through a bus error cannot repeat.
 b.regs[1]=0xff;
 for(unsigned t=820;t<=900;t+=20)b.poll(t);
 b.regs[1]=0xf3;
 for(unsigned t=920;t<=1040;t+=20)b.poll(t);
 assert((keys==std::vector<unsigned>{0,1,2}));
 Board io; io.setup();
 es8311::ES8311 dac; openathan_audio::PartitionAudio playback;
 waveshare_audio_board::Audio audio; audio.set_board(&io); audio.set_dac(&dac); audio.set_playback(&playback);
 audio.setup(); audio.loop(); assert(!audio.ready() && !(io.regs[3]&1));
 playback.available=true; audio.loop(); assert(audio.ready() && (io.regs[3]&1));
 audio.stop(); assert(playback.stops==1);
 playback.available=false;
 io.fail_at=io.writes.size()+1;
 audio.loop(); assert(audio.is_failed() && !audio.ready());
 playback.available=true; assert(!audio.start(::openathan::Track::NORMAL));
 Board absent; absent.fail_read=true; absent.setup();
 waveshare_audio_board::Audio silent; silent.set_board(&absent); silent.set_dac(&dac); silent.set_playback(&playback);
 silent.setup(); silent.loop(); assert(!silent.ready());
 light::LightState leds; Component driver; waveshare_audio_board::Ring ring;
 ring.set_light(&leds); ring.set_driver(&driver);
 assert(ring.apply({255,128,0,20}));
 assert(leds.on && leds.red==1.0f && leds.blue==0 && leds.brightness==0.2f && leds.transition==0);
 assert(ring.apply({0,0,0,0}) && !leds.on);
 driver.mark_failed(); assert(!ring.apply({255,0,0,100}));
}
