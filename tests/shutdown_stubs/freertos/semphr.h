#pragma once
#include "FreeRTOS.h"
inline SemaphoreHandle_t xSemaphoreCreateBinary() { return nullptr; }
inline void vSemaphoreDelete(SemaphoreHandle_t) {}
