#pragma once
typedef int gpio_num_t;
inline void gpio_reset_pin(gpio_num_t) {}
#define ESP_INTR_FLAG_IRAM 1
#define ESP_INTR_FLAG_LEVEL1 2
