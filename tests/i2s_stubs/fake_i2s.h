#pragma once
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <vector>
struct i2s_event_data_t;
namespace fake_i2s {
struct Queue { size_t capacity, width; std::deque<std::vector<uint8_t>> entries; };
struct Exit {};
inline std::vector<Queue *> queues;
inline bool defer_isr_bits{};
inline int64_t now{};
inline bool allocation_failure{}, partial_write{}, drop_events{}, callback_failure{}, channel_failure{}, invalid_dma_layout{};
inline bool ring_allocation_failure{}, source_allocation_failure{}, task_creation_failure{}, preload_partial{}, enable_failure{};
inline std::function<void()> on_write, on_fill, on_disable;
inline std::function<void(size_t)> on_receive;
inline void (*task)(void *); inline void *task_arg{};
inline std::deque<std::vector<uint8_t>> dma;
inline std::vector<std::vector<uint8_t>> transmitted;
inline size_t dma_bytes = 8, written{}, next_transmit{}, current_write{};
inline bool current_valid{};
inline std::deque<size_t> free_descriptors;
inline std::function<void(size_t, const std::vector<uint8_t> &)> on_transmit;
inline bool (*overflow_callback)(void *, i2s_event_data_t *, void *);
inline bool (*callback)(void *, i2s_event_data_t *, void *); inline void *context{};
inline void reset() {
 now=0; dma_bytes=8; defer_isr_bits=false; allocation_failure=partial_write=drop_events=callback_failure=channel_failure=invalid_dma_layout=false;queues.clear();
 ring_allocation_failure=source_allocation_failure=task_creation_failure=preload_partial=enable_failure=false;
 on_write={}; on_fill={}; on_disable={}; on_receive={}; on_transmit={}; free_descriptors.clear(); next_transmit=current_write=0; current_valid=false; overflow_callback=nullptr; task=nullptr; dma.clear(); transmitted.clear(); written=0; callback=nullptr;
}
}
using TickType_t = uint32_t; using BaseType_t = int; using EventBits_t = uint32_t;
using QueueHandle_t = fake_i2s::Queue *; using EventGroupHandle_t = uint32_t *; using TaskHandle_t = void *;
constexpr int pdTRUE=1, pdFALSE=0; constexpr TickType_t portMAX_DELAY=UINT32_MAX;
#define pdMS_TO_TICKS(x) (x)
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portENTER_CRITICAL_ISR(x) ((void)(x))
#define portEXIT_CRITICAL_ISR(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
inline EventGroupHandle_t xEventGroupCreate() { return new uint32_t(0); }
inline uint32_t xEventGroupGetBits(EventGroupHandle_t g) { return *g; }
inline uint32_t xEventGroupSetBits(EventGroupHandle_t g,uint32_t b) { return *g|=b; }
inline uint32_t xEventGroupClearBits(EventGroupHandle_t g,uint32_t b) {auto old=*g; *g&=~b;return old;}
inline void xEventGroupSetBitsFromISR(EventGroupHandle_t g,uint32_t b,BaseType_t*) { if(!fake_i2s::defer_isr_bits)*g|=b; }
inline QueueHandle_t xQueueCreate(size_t n,size_t w) {
 if(fake_i2s::allocation_failure)return nullptr;
 auto q=new fake_i2s::Queue{n,w,{}};fake_i2s::queues.push_back(q);return q;
}
inline void xQueueReset(QueueHandle_t q) {q->entries.clear();}
inline int xQueueSend(QueueHandle_t q,const void *data,TickType_t) {
 if(q->entries.size()==q->capacity)return pdFALSE;
 auto p=static_cast<const uint8_t*>(data);q->entries.emplace_back(p,p+q->width);return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t q,void *data,TickType_t) {
 if(q->entries.empty())return pdFALSE;
 std::memcpy(data,q->entries.front().data(),q->width);q->entries.pop_front();
 if(fake_i2s::on_receive)fake_i2s::on_receive(q->width);return pdTRUE;
}
inline size_t uxQueueMessagesWaiting(QueueHandle_t q){return q->entries.size();}
inline int xQueueIsQueueFullFromISR(QueueHandle_t q){return q->entries.size()==q->capacity;}
inline int xQueueReceiveFromISR(QueueHandle_t q,void*d,BaseType_t*){return xQueueReceive(q,d,0);}
inline int xQueueSendToBackFromISR(QueueHandle_t q,const void*d,BaseType_t*){return xQueueSend(q,d,0);}
inline void xTaskCreate(void (*f)(void*),const char*,size_t,void *arg,int,TaskHandle_t*out) {
 if(fake_i2s::task_creation_failure){*out=nullptr;return;}
 fake_i2s::task=f;fake_i2s::task_arg=arg;*out=reinterpret_cast<void*>(1);
}
inline void vTaskDelete(TaskHandle_t) {}
inline void vTaskDelay(TickType_t) {throw fake_i2s::Exit{};}
