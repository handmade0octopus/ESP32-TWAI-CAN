#pragma once
#include "FreeRTOS.h"
#include <cstddef>
struct StubQueue;
typedef StubQueue* QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned length, unsigned itemSize);
void vQueueDelete(QueueHandle_t queue);
BaseType_t xQueueReceive(QueueHandle_t queue, void* out, TickType_t timeout);
BaseType_t xQueueSendFromISR(QueueHandle_t queue, const void* item, BaseType_t* woken);
unsigned uxQueueMessagesWaiting(QueueHandle_t queue);
