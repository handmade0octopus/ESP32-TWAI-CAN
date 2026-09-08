#pragma once
#include <cstdio>
#include <cstring>
#include "ESP32-TWAI-CAN.hpp"

struct StubSDK {
    esp_err_t installResult = ESP_OK, startResult = ESP_OK, stopResult = ESP_OK;
    esp_err_t uninstallResult = ESP_OK, infoResult = ESP_OK, recoverResult = ESP_OK;
    esp_err_t callbackResult = ESP_OK, txResult = ESP_OK;
    bool failShadow = false, failRxQueue = false;
    unsigned installs = 0, starts = 0, stops = 0, uninstalls = 0, recoveries = 0, registrations = 0;
    unsigned liveQueues = 0, liveShadows = 0, liveNodes = 0, invalidQueueUses = 0;
    TickType_t rxTicks = 0, txTicks = 0;
    int txTimeoutMs = 0;
    CanFrame received = {}, transmitted = {};
#ifdef TWAI_CAN_NEW_DRIVER
    twai_node_handle_t node = nullptr;
    twai_onchip_node_config_t config = {};
#else
    bool installed = false;
    twai_status_info_t status = {};
    twai_status_info_t* statusOutput = nullptr;
    unsigned statusCalls = 0;
    twai_general_config_t general = {};
    twai_timing_config_t timing = {};
    twai_filter_config_t filter = {};
#endif
};
extern StubSDK sdk;
extern int failures;
#define CHECK(test) do { if(!(test)) { printf("FAIL line %d: %s\n", __LINE__, #test); failures++; } } while(0)

#ifdef TWAI_CAN_NEW_DRIVER
void stubState(twai_node_handle_t node, twai_error_state_t state);
bool stubRx(twai_node_handle_t node);
bool stubComplete(twai_node_handle_t node, unsigned index, bool success);
unsigned stubPending(twai_node_handle_t node);
const twai_frame_t* stubFrame(twai_node_handle_t node, unsigned index);
#endif
