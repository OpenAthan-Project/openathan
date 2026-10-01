#pragma once
using Worker=void(*)(void *);
inline Worker pending_worker;
inline void *worker_argument;
inline bool task_available=true;
inline unsigned task_attempts{};
inline int xTaskCreate(Worker fn,const char *,unsigned,void *arg,unsigned,void *){++task_attempts;if(!task_available)return 0;pending_worker=fn;worker_argument=arg;return 1;}
inline void vTaskDelete(void *){}
inline void vTaskDelay(unsigned){}
inline void run_worker(){auto fn=pending_worker;pending_worker=nullptr;fn(worker_argument);}
inline unsigned worker_stack_free_bytes=4096;
inline unsigned uxTaskGetStackHighWaterMark(void *){return worker_stack_free_bytes;}
