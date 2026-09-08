#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"

typedef enum { TWAI_STATE_STOPPED, TWAI_STATE_RUNNING, TWAI_STATE_BUS_OFF, TWAI_STATE_RECOVERING } twai_state_t;
typedef struct {
    union {
        struct { uint32_t extd:1, rtr:1, ss:1, self:1, dlc_non_comp:1, reserved:27; };
        uint32_t flags;
    };
    uint32_t identifier;
    uint8_t data_length_code;
    uint8_t data[8];
} twai_message_t;
struct twai_status_info_t {
    twai_state_t state;
    uint32_t msgs_to_tx, msgs_to_rx, rx_error_counter, tx_error_counter;
    uint32_t rx_missed_count, tx_failed_count, bus_error_count;
};
struct twai_general_config_t {
    int mode;
    gpio_num_t tx_io, rx_io, clkout_io, bus_off_io;
    unsigned tx_queue_len, rx_queue_len, alerts_enabled, clkout_divider;
    int intr_flags;
};
struct twai_filter_config_t { uint32_t acceptance_code, acceptance_mask; bool single_filter; };
struct twai_timing_config_t { uint32_t bitrate; };
#define TWAI_MODE_NORMAL 0
#define TWAI_IO_UNUSED -1
#define TWAI_ALERT_NONE 0
#define TWAI_FILTER_CONFIG_ACCEPT_ALL() {0, UINT32_MAX, true}
#define TWAI_TIMING_CONFIG_1KBITS() {1000}
#define TWAI_TIMING_CONFIG_5KBITS() {5000}
#define TWAI_TIMING_CONFIG_10KBITS() {10000}
#define TWAI_TIMING_CONFIG_12_5KBITS() {12500}
#define TWAI_TIMING_CONFIG_16KBITS() {16000}
#define TWAI_TIMING_CONFIG_20KBITS() {20000}
#define TWAI_TIMING_CONFIG_50KBITS() {50000}
#define TWAI_TIMING_CONFIG_100KBITS() {100000}
#define TWAI_TIMING_CONFIG_125KBITS() {125000}
#define TWAI_TIMING_CONFIG_250KBITS() {250000}
#define TWAI_TIMING_CONFIG_500KBITS() {500000}
#define TWAI_TIMING_CONFIG_800KBITS() {800000}
#define TWAI_TIMING_CONFIG_1MBITS() {1000000}
esp_err_t twai_driver_install(const twai_general_config_t*, const twai_timing_config_t*, const twai_filter_config_t*);
esp_err_t twai_driver_uninstall();
esp_err_t twai_start();
esp_err_t twai_stop();
esp_err_t twai_get_status_info(twai_status_info_t*);
esp_err_t twai_initiate_recovery();
esp_err_t twai_receive(twai_message_t*, TickType_t);
esp_err_t twai_transmit(const twai_message_t*, TickType_t);
