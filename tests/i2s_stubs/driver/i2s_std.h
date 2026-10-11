#pragma once
#include "fake_i2s.h"
 #include "nvs.h"
using esp_err_t = int;
constexpr int ESP_ERR_NO_MEM=1,ESP_ERR_NOT_SUPPORTED=2,ESP_ERR_INVALID_STATE=3;
inline const char*esp_err_to_name(int){return "fake";}
using gpio_num_t=int; using i2s_port_t=int; using i2s_role_t=int; using i2s_slot_mode_t=int;
using i2s_std_slot_mask_t=int; using i2s_slot_bit_width_t=int; using i2s_mclk_multiple_t=int;
using i2s_clock_src_t=int; using i2s_data_bit_width_t=int; using i2s_chan_handle_t=void*;
constexpr int I2S_ROLE_SLAVE=1,I2S_CLK_SRC_DEFAULT=0,I2S_SLOT_MODE_MONO=1,I2S_SLOT_MODE_STEREO=2,
 I2S_STD_SLOT_BOTH=3,I2S_SLOT_BIT_WIDTH_AUTO=0,I2S_GPIO_UNUSED=-1;
struct i2s_std_gpio_config_t {int mclk{},bclk{},ws{},dout{},din{};struct {bool mclk_inv{},bclk_inv{},ws_inv{};} invert_flags;};
struct i2s_chan_config_t {int id,role;size_t dma_desc_num;uint32_t dma_frame_num;bool auto_clear;int intr_priority;};
struct i2s_std_clk_config_t {uint32_t sample_rate_hz;int clk_src,mclk_multiple;};
struct i2s_std_slot_config_t {int slot_bit_width{},ws_width{},slot_mask{};bool msb_right{};};
#define I2S_STD_PCM_SLOT_DEFAULT_CONFIG(...) i2s_std_slot_config_t{}
#define I2S_STD_MSB_SLOT_DEFAULT_CONFIG(...) i2s_std_slot_config_t{}
#define I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(...) i2s_std_slot_config_t{}
struct i2s_std_config_t {i2s_std_clk_config_t clk_cfg; i2s_std_slot_config_t slot_cfg;i2s_std_gpio_config_t gpio_cfg;};
struct i2s_chan_info_t {size_t total_dma_buf_size;};struct i2s_event_data_t {};
struct i2s_event_callbacks_t {bool(*on_sent)(i2s_chan_handle_t,i2s_event_data_t*,void*);};
inline int i2s_new_channel(const i2s_chan_config_t*,void **out,void*) {*out=fake_i2s::channel_failure?nullptr:reinterpret_cast<void*>(1);return fake_i2s::channel_failure?1:0;}
inline int i2s_channel_init_std_mode(void*,const i2s_std_config_t*){return 0;}
inline int i2s_del_channel(void*){fake_i2s::dma.clear();fake_i2s::written=0;return 0;}
inline int i2s_channel_disable(void*){if(fake_i2s::on_disable)fake_i2s::on_disable();return 0;}
inline int i2s_channel_enable(void*){return fake_i2s::enable_failure?1:0;}
inline int i2s_channel_get_info(void*,i2s_chan_info_t*out){out->total_dma_buf_size=5*fake_i2s::dma_bytes+(fake_i2s::invalid_dma_layout?1:0);return 0;}
inline int i2s_channel_preload_data(void*,const void*d,size_t n,size_t*out){auto p=static_cast<const uint8_t*>(d);fake_i2s::dma.emplace_back(p,p+n);*out=fake_i2s::preload_partial?n-1:n;return 0;}
inline int i2s_channel_register_event_callback(void*,const i2s_event_callbacks_t*c,void*ctx){
 fake_i2s::callback=c->on_sent;fake_i2s::context=ctx;return fake_i2s::callback_failure?1:0;
}
inline int i2s_channel_write(void*,const void*d,size_t n,size_t*out,TickType_t){
 using namespace fake_i2s;
 if(on_write)on_write();
 if(partial_write){*out=n-1;return 0;}
 if(!written){
  assert(dma.size()==5);transmitted.push_back(dma.front());dma.pop_front();
  now+=10000;if(!drop_events)callback(nullptr,nullptr,context);
  dma.emplace_back();
 }
 const auto*p=static_cast<const uint8_t*>(d);dma.back().insert(dma.back().end(),p,p+n);written+=n;
 assert(written<=dma_bytes);if(written==dma_bytes)written=0;*out=n;return 0;
}
