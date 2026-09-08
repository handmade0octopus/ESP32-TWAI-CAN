#include "sdk_stub.hpp"
#include <cstdlib>
#include <cassert>
#include <mutex>

StubSDK sdk;
int failures;
void* stubCalloc(size_t count, size_t size) {
    if(sdk.failShadow) return nullptr;
    void* p = calloc(count, size);
    if(p) sdk.liveShadows++;
    return p;
}
void stubFree(void* p) { if(p) { sdk.liveShadows--; free(p); } }

struct StubQueue { unsigned capacity, itemSize, head, count; uint8_t items[128][32]; };
QueueHandle_t xQueueCreate(unsigned length, unsigned itemSize) {
    if(sdk.failRxQueue) return nullptr;
    assert(length && length <= 128 && itemSize <= 32);
    StubQueue* q = new StubQueue{};
    q->capacity = length; q->itemSize = itemSize; sdk.liveQueues++;
    return q;
}
void vQueueDelete(QueueHandle_t q) { assert(q); delete q; sdk.liveQueues--; }
BaseType_t xQueueReceive(QueueHandle_t q, void* out, TickType_t timeout) {
    sdk.rxTicks = timeout;
    if(!q) { sdk.invalidQueueUses++; return pdFALSE; }
    if(!q->count) return pdFALSE;
    memcpy(out, q->items[q->head], q->itemSize);
    q->head = (q->head + 1) % q->capacity; q->count--;
    return pdTRUE;
}
BaseType_t xQueueSendFromISR(QueueHandle_t q, const void* item, BaseType_t* woken) {
    if(!q) { sdk.invalidQueueUses++; return pdFALSE; }
    if(q->count == q->capacity) return pdFALSE;
    memcpy(q->items[(q->head + q->count++) % q->capacity], item, q->itemSize);
    *woken = pdTRUE;
    return pdTRUE;
}
unsigned uxQueueMessagesWaiting(QueueHandle_t q) { assert(q); return q->count; }

#ifndef TWAI_CAN_NEW_DRIVER
esp_err_t twai_driver_install(const twai_general_config_t* g, const twai_timing_config_t* t, const twai_filter_config_t* f) {
    sdk.installs++;
    if(sdk.installed) return ESP_ERR_INVALID_STATE;
    if(sdk.installResult != ESP_OK) return sdk.installResult;
    sdk.general = *g; sdk.timing = *t; sdk.filter = *f;
    sdk.installed = true; sdk.status.state = TWAI_STATE_STOPPED;
    return ESP_OK;
}
esp_err_t twai_driver_uninstall() {
    sdk.uninstalls++;
    if(!sdk.installed) return ESP_ERR_INVALID_ARG;
    if(sdk.status.state != TWAI_STATE_STOPPED && sdk.status.state != TWAI_STATE_BUS_OFF) return ESP_ERR_INVALID_STATE;
    if(sdk.uninstallResult != ESP_OK) return sdk.uninstallResult;
    sdk.installed = false;
    return ESP_OK;
}
esp_err_t twai_start() {
    sdk.starts++;
    if(!sdk.installed || sdk.status.state != TWAI_STATE_STOPPED) return ESP_ERR_INVALID_STATE;
    if(sdk.startResult != ESP_OK) return sdk.startResult;
    sdk.status.state = TWAI_STATE_RUNNING;
    return ESP_OK;
}
esp_err_t twai_stop() {
    sdk.stops++;
    if(!sdk.installed || sdk.status.state != TWAI_STATE_RUNNING) return ESP_ERR_INVALID_STATE;
    if(sdk.stopResult != ESP_OK) return sdk.stopResult;
    sdk.status.state = TWAI_STATE_STOPPED;
    return ESP_OK;
}
esp_err_t twai_get_status_info(twai_status_info_t* out) {
    sdk.statusOutput = out; sdk.statusCalls++;
    if(!sdk.installed) return ESP_ERR_INVALID_ARG;
    if(sdk.infoResult != ESP_OK) return sdk.infoResult;
    *out = sdk.status;
    return ESP_OK;
}
esp_err_t twai_initiate_recovery() {
    sdk.recoveries++;
    if(!sdk.installed || sdk.status.state != TWAI_STATE_BUS_OFF) return ESP_ERR_INVALID_STATE;
    if(sdk.recoverResult != ESP_OK) return sdk.recoverResult;
    sdk.status.state = TWAI_STATE_RECOVERING;
    return ESP_OK;
}
esp_err_t twai_receive(twai_message_t* frame, TickType_t ticks) {
    sdk.rxTicks = ticks;
    if(!sdk.installed) return ESP_ERR_INVALID_STATE;
    *frame = sdk.received;
    return ESP_OK;
}
esp_err_t twai_transmit(const twai_message_t* frame, TickType_t ticks) {
    sdk.txTicks = ticks; sdk.transmitted = *frame;
    return sdk.installed ? sdk.txResult : ESP_ERR_INVALID_STATE;
}
#else
struct StubNode {
    twai_error_state_t state = TWAI_ERROR_BUS_OFF;
    twai_event_callbacks_t callbacks = {};
    void* context = nullptr;
    const twai_frame_t* pending[32] = {};
    unsigned count = 0;
    std::mutex txMutex;
};
esp_err_t twai_new_node_onchip(const twai_onchip_node_config_t* cfg, twai_node_handle_t* out) {
    sdk.installs++;
    if(sdk.installResult != ESP_OK) return sdk.installResult;
    *out = new StubNode;
    sdk.node = *out; sdk.config = *cfg; sdk.liveNodes++;
    return ESP_OK;
}
esp_err_t twai_node_register_event_callbacks(twai_node_handle_t n, const twai_event_callbacks_t* cb, void* ctx) {
    sdk.registrations++;
    if(sdk.callbackResult != ESP_OK) return sdk.callbackResult;
    n->callbacks = *cb; n->context = ctx;
    return ESP_OK;
}
esp_err_t twai_node_enable(twai_node_handle_t n) {
    sdk.starts++;
    if(n->state != TWAI_ERROR_BUS_OFF) return ESP_ERR_INVALID_STATE;
    if(sdk.startResult != ESP_OK) return sdk.startResult;
    n->state = TWAI_ERROR_ACTIVE;
    return ESP_OK;
}
esp_err_t twai_node_disable(twai_node_handle_t n) {
    sdk.stops++;
    if(n->state == TWAI_ERROR_BUS_OFF) return ESP_ERR_INVALID_STATE;
    if(sdk.stopResult != ESP_OK) return sdk.stopResult;
    n->state = TWAI_ERROR_BUS_OFF;
    return ESP_OK;
}
esp_err_t twai_node_delete(twai_node_handle_t n) {
    sdk.uninstalls++;
    if(n->state != TWAI_ERROR_BUS_OFF) return ESP_ERR_INVALID_STATE;
    if(sdk.uninstallResult != ESP_OK) return sdk.uninstallResult;
    delete n; sdk.liveNodes--;
    if(sdk.node == n) sdk.node = nullptr;
    return ESP_OK;
}
esp_err_t twai_node_get_info(twai_node_handle_t n, twai_node_status_t* st, twai_node_record_t* rec) {
    if(sdk.infoResult != ESP_OK) return sdk.infoResult;
    if(st) { *st = {}; st->state = n->state; }
    if(rec) *rec = {};
    return ESP_OK;
}
esp_err_t twai_node_recover(twai_node_handle_t n) {
    sdk.recoveries++;
    if(n->state != TWAI_ERROR_BUS_OFF) return ESP_ERR_INVALID_STATE;
    // Real IDF leaves state BUS_OFF until the later state-change IRQ.
    return sdk.recoverResult;
}
void stubState(twai_node_handle_t n, twai_error_state_t state) {
    twai_state_change_event_data_t event = {n->state, state};
    n->state = state;
    if(n->callbacks.on_state_change) n->callbacks.on_state_change(n, &event, n->context);
}
esp_err_t twai_node_transmit(twai_node_handle_t n, const twai_frame_t* f, int timeout) {
    std::lock_guard<std::mutex> lock(n->txMutex);
    sdk.txTimeoutMs = timeout;
    if(sdk.txResult != ESP_OK) return sdk.txResult;
    if(n->state == TWAI_ERROR_BUS_OFF) return ESP_ERR_INVALID_STATE;
    assert(n->count < 32);
    for(unsigned i = 0; i < n->count; i++) assert(n->pending[i] != f);
    n->pending[n->count++] = f;
    return ESP_OK;
}
unsigned stubPending(twai_node_handle_t n) { return n->count; }
const twai_frame_t* stubFrame(twai_node_handle_t n, unsigned i) { assert(i < n->count); return n->pending[i]; }
bool stubComplete(twai_node_handle_t n, unsigned i, bool success) {
    assert(i < n->count);
    twai_tx_done_event_data_t event = {success, n->pending[i]};
    for(unsigned j = i + 1; j < n->count; j++) n->pending[j - 1] = n->pending[j];
    n->count--;
    return n->callbacks.on_tx_done(n, &event, n->context);
}
esp_err_t twai_node_receive_from_isr(twai_node_handle_t, twai_frame_t* f) {
    f->header.id = sdk.received.identifier;
    f->header.ide = sdk.received.extd; f->header.rtr = sdk.received.rtr;
    f->header.dlc = sdk.received.data_length_code;
    if(!f->header.rtr) memcpy(f->buffer, sdk.received.data, f->header.dlc > 8 ? 8 : f->header.dlc);
    return ESP_OK;
}
bool stubRx(twai_node_handle_t n) {
    twai_rx_done_event_data_t event = {};
    return n->callbacks.on_rx_done && n->callbacks.on_rx_done(n, &event, n->context);
}
int64_t esp_timer_get_time() { return 123456000; }
#endif
