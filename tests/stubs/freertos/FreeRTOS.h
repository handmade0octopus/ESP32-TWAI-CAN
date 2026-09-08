#pragma once
#include <cstdint>
#include <mutex>

typedef uint32_t TickType_t;
typedef int BaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY UINT32_MAX
#define configTICK_RATE_HZ 1000U
// Match the legacy IDF kernel's overflow-prone conversion to catch sentinel bugs.
#define pdMS_TO_TICKS(ms) ((TickType_t)(((TickType_t)(ms) * (TickType_t)configTICK_RATE_HZ) / 1000U))
struct portMUX_TYPE { std::recursive_mutex mutex; };
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(mux) (mux)->mutex.lock()
#define portEXIT_CRITICAL(mux) (mux)->mutex.unlock()
#define portENTER_CRITICAL_ISR(mux) portENTER_CRITICAL(mux)
#define portEXIT_CRITICAL_ISR(mux) portEXIT_CRITICAL(mux)
