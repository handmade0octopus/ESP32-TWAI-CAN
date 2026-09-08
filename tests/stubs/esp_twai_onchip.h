#pragma once
#include "esp_twai.h"
#include "driver/gpio.h"
struct twai_onchip_node_config_t {
    struct { gpio_num_t tx, rx, quanta_clk_out, bus_off_indicator; } io_cfg;
    struct { uint32_t bitrate; } bit_timing;
    unsigned tx_queue_depth;
    int fail_retry_cnt;
};
esp_err_t twai_new_node_onchip(const twai_onchip_node_config_t*, twai_node_handle_t*);
