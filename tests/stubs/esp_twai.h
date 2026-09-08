#pragma once
#include <cstddef>
#include <cstdint>
#include "esp_err.h"

struct StubNode;
typedef StubNode* twai_node_handle_t;
typedef enum { TWAI_ERROR_ACTIVE, TWAI_ERROR_WARNING, TWAI_ERROR_PASSIVE, TWAI_ERROR_BUS_OFF } twai_error_state_t;
struct twai_frame_header_t { uint32_t id; uint8_t dlc; bool ide, rtr, fdf, brs; };
struct twai_frame_t { twai_frame_header_t header; uint8_t* buffer; size_t buffer_len; };
struct twai_node_status_t { twai_error_state_t state; uint16_t tx_error_count, rx_error_count; uint32_t tx_queue_remaining; };
struct twai_node_record_t { uint32_t bus_err_num; };
struct twai_rx_done_event_data_t {};
struct twai_tx_done_event_data_t { bool is_tx_success; const twai_frame_t* done_tx_frame; };
struct twai_state_change_event_data_t { twai_error_state_t old_sta, new_sta; };
struct twai_event_callbacks_t {
    bool (*on_tx_done)(twai_node_handle_t, const twai_tx_done_event_data_t*, void*);
    bool (*on_rx_done)(twai_node_handle_t, const twai_rx_done_event_data_t*, void*);
    bool (*on_state_change)(twai_node_handle_t, const twai_state_change_event_data_t*, void*);
};
esp_err_t twai_node_register_event_callbacks(twai_node_handle_t, const twai_event_callbacks_t*, void*);
esp_err_t twai_node_enable(twai_node_handle_t);
esp_err_t twai_node_disable(twai_node_handle_t);
esp_err_t twai_node_delete(twai_node_handle_t);
esp_err_t twai_node_recover(twai_node_handle_t);
esp_err_t twai_node_get_info(twai_node_handle_t, twai_node_status_t*, twai_node_record_t*);
esp_err_t twai_node_transmit(twai_node_handle_t, const twai_frame_t*, int);
esp_err_t twai_node_receive_from_isr(twai_node_handle_t, twai_frame_t*);
