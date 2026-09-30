#pragma once
using Worker=void(*)(void *);
inline Worker pending_worker;
inline void *worker_argument;
inline int xTaskCreate(Worker fn,const char *,unsigned,void *arg,unsigned,void *){pending_worker=fn;worker_argument=arg;return 1;}
inline void vTaskDelete(void *){}
inline void vTaskDelay(unsigned){}
inline void run_worker(){auto fn=pending_worker;pending_worker=nullptr;fn(worker_argument);}
