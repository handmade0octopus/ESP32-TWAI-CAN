#include "sdk_stub.hpp"
#include <thread>
#include <atomic>

static unsigned observations;
static bool lastTx;
static CanFrame observed;
static bool observe(const CanFrame& f, bool tx, uint32_t stamp, void* ctx) {
    CHECK(ctx == &observations && stamp == 123456);
    observed = f; lastTx = tx; observations++;
    return true;
}

static void checkDiagnostics(TwaiCAN& can, bool hasNode, unsigned queued = 0) {
    CHECK(can.getSpeed() == TWAI_SPEED_500KBPS && can.getSpeedNumeric() == 500);
    CHECK(can.inTxQueue() == queued && can.inRxQueue() == 0);
    CHECK(can.rxErrorCounter() == 0 && can.txErrorCounter() == 0);
    CHECK(can.rxMissedCounter() == 0 && can.txFailedCounter() == 0 && can.busErrCounter() == 0);
    CHECK(can.canState() == (hasNode ? TWAI_STATE_BUS_OFF : TWAI_STATE_STOPPED));
    TwaiDiagnostics d;
    CHECK(!can.getDiagnostics(nullptr));
    CHECK(can.getDiagnostics(&d) == hasNode);
    CHECK(d.errorState == (hasNode ? (int)TWAI_ERROR_BUS_OFF : -1));
    CHECK(d.rxMissed == 0 && d.busErrors == 0 && d.txErrors == 0 && d.rxErrors == 0);
}

int main(int argc, char** argv) {
    if(argc != 2) return 2;
    const char* test = argv[1];
    TwaiCAN can;
    if(!strcmp(test, "node_failure") || !strcmp(test, "shadow_failure") ||
       !strcmp(test, "rx_failure") || !strcmp(test, "callback_failure") || !strcmp(test, "enable_failure")) {
        if(!strcmp(test, "node_failure")) sdk.installResult = ESP_ERR_NO_MEM;
        if(!strcmp(test, "shadow_failure")) sdk.failShadow = true;
        if(!strcmp(test, "rx_failure")) sdk.failRxQueue = true;
        if(!strcmp(test, "callback_failure")) sdk.callbackResult = ESP_FAIL;
        if(!strcmp(test, "enable_failure")) sdk.startResult = ESP_FAIL;
        CHECK(!can.begin());
        CHECK(sdk.starts == (!strcmp(test, "enable_failure") ? 1U : 0U));
        if(sdk.failRxQueue) CHECK(sdk.registrations == 0);
        CHECK(!sdk.liveNodes && !sdk.liveQueues && !sdk.liveShadows);
        // Old RX-allocation bug leaves a running node: demonstrate the invalid ISR use safely.
        if(sdk.node && sdk.starts) stubRx(sdk.node);
        CHECK(sdk.invalidQueueUses == 0);
        checkDiagnostics(can, false);
        sdk.installResult = sdk.callbackResult = sdk.startResult = ESP_OK;
        sdk.failShadow = sdk.failRxQueue = false;
        CHECK(can.begin());
    } else if(!strcmp(test, "delete_failure")) {
        CHECK(can.begin());
        CanFrame f = {}; f.identifier = 0x123; f.data_length_code = 8;
        CHECK(can.writeFrame(f));
        sdk.uninstallResult = ESP_FAIL;
        CHECK(!can.end()); CHECK(sdk.liveNodes == 1 && sdk.liveQueues == 1 && sdk.liveShadows == 1);
        checkDiagnostics(can, true, 1);
        sdk.uninstallResult = ESP_OK;
        CHECK(can.end()); checkDiagnostics(can, false); CHECK(can.begin());
    } else if(!strcmp(test, "shadow_delete_failure") || !strcmp(test, "rx_delete_failure")) {
        sdk.failShadow = !strcmp(test, "shadow_delete_failure");
        sdk.failRxQueue = !strcmp(test, "rx_delete_failure");
        sdk.uninstallResult = ESP_FAIL;
        CHECK(!can.begin());
        CHECK(sdk.liveNodes == 1 && sdk.liveShadows == (sdk.failShadow ? 0U : 1U));
        CHECK(sdk.liveQueues == (sdk.failRxQueue ? 0U : 1U));
        CHECK(sdk.registrations == 0 && sdk.starts == 0);
        checkDiagnostics(can, true);
        CHECK(!can.end()); checkDiagnostics(can, true);
        sdk.uninstallResult = ESP_OK;
        CHECK(can.end()); checkDiagnostics(can, false);
        sdk.failShadow = sdk.failRxQueue = false;
        CHECK(can.begin());
    } else if(!strcmp(test, "recovery")) {
        CHECK(!can.recover()); CHECK(!can.restart());
        CHECK(can.begin()); CHECK(can.recover()); CHECK(sdk.recoveries == 0);
        stubState(sdk.node, TWAI_ERROR_BUS_OFF); sdk.recoverResult = ESP_FAIL;
        CHECK(!can.recover()); CHECK(can.canState() == TWAI_STATE_BUS_OFF);
        sdk.recoverResult = ESP_OK;
        CHECK(can.recover()); CHECK(can.recover()); CHECK(sdk.recoveries == 2);
        CHECK(can.canState() == TWAI_STATE_RECOVERING);
        // No canState() sample between successful recovery and the next bus-off.
        stubState(sdk.node, TWAI_ERROR_ACTIVE);
        stubState(sdk.node, TWAI_ERROR_BUS_OFF);
        CHECK(can.canState() == TWAI_STATE_BUS_OFF);
        CHECK(can.recover()); CHECK(sdk.recoveries == 3);
        stubState(sdk.node, TWAI_ERROR_ACTIVE); CHECK(can.canState() == TWAI_STATE_RUNNING);
        sdk.infoResult = ESP_FAIL; CHECK(!can.recover()); sdk.infoResult = ESP_OK;
    } else if(!strcmp(test, "tx_slots")) {
        can.setFrameObserver(observe, &observations);
        CHECK(can.begin(TWAI_SPEED_500KBPS, 18, 17, 4, 5));
        CanFrame f = {}; f.data_length_code = 8;
        for(unsigned i = 0; i < 4; i++) { f.identifier = 0x100 + i; f.data[0] = i; CHECK(can.writeFrame(f)); }
        memset(&f, 0xCC, sizeof(f));
        CHECK(can.inTxQueue() == 4 && stubPending(sdk.node) == 4);
        for(unsigned i = 0; i < 4; i++) {
            const twai_frame_t* pending = stubFrame(sdk.node, i);
            CHECK(pending->header.id == 0x100 + i && pending->buffer[0] == i);
        }
        f = {}; f.identifier = 0x456; f.data_length_code = 8;
        CHECK(!can.writeFrame(f, portMAX_DELAY)); CHECK(stubPending(sdk.node) == 4);
        const twai_frame_t* released = stubFrame(sdk.node, 1);
        CHECK(stubComplete(sdk.node, 1, true)); CHECK(observations == 1 && lastTx && observed.identifier == 0x101);
        CHECK(can.writeFrame(f)); CHECK(stubFrame(sdk.node, 3) == released);
        CHECK(!stubComplete(sdk.node, 0, false)); CHECK(observations == 1);
        sdk.txResult = ESP_ERR_TIMEOUT; CHECK(!can.writeFrame(f)); CHECK(can.inTxQueue() == 3);
        sdk.txResult = ESP_OK; CHECK(can.writeFrame(f));
        TwaiDiagnostics d; CHECK(can.getDiagnostics(&d));
        CHECK(d.accepted == 6 && d.completed == 1 && d.failed == 1 && d.rejected == 2);
    } else if(!strcmp(test, "io")) {
        can.setFrameObserver(observe, &observations);
        CHECK(can.begin());
        CanFrame f = {};
        CHECK(!can.readFrame((CanFrame*)nullptr)); CHECK(!can.writeFrame((const CanFrame*)nullptr));
        CHECK(!can.readFrame(f, portMAX_DELAY)); CHECK(sdk.rxTicks == portMAX_DELAY);
        CHECK(!can.readFrame(&f, 17)); CHECK(sdk.rxTicks == pdMS_TO_TICKS(17));
        CHECK(!can.readFrame(f, 0)); CHECK(sdk.rxTicks == 0);
        f.identifier = 0x1ABCDE; f.extd = 1; f.data_length_code = 3; f.data[2] = 0xA5;
        CHECK(can.writeFrame(f, portMAX_DELAY)); CHECK(sdk.txTimeoutMs == -1);
        CHECK(can.writeFrame(&f, 23)); CHECK(sdk.txTimeoutMs == 23);
        CHECK(can.writeFrame(f, 0)); CHECK(sdk.txTimeoutMs == 0);
        CHECK(stubFrame(sdk.node, 0)->header.ide && stubFrame(sdk.node, 0)->header.id == f.identifier);
        CHECK(stubFrame(sdk.node, 0)->header.dlc == 3 && stubFrame(sdk.node, 0)->buffer[2] == 0xA5);
        sdk.received = f; CHECK(stubRx(sdk.node)); CHECK(!lastTx);
        CHECK(can.readFrame(f, 0)); CHECK(f.extd && f.identifier == 0x1ABCDE && f.data[2] == 0xA5);
        CHECK(f.data[3] == 0 && f.data[7] == 0);
        sdk.received.rtr = 1; CHECK(stubRx(sdk.node)); CHECK(can.readFrame(f));
        CHECK(f.rtr && f.data_length_code == 3 && f.data[2] == 0);
        f = {}; f.identifier = 0x800; CHECK(!can.writeFrame(f));
        f.identifier = 0x100; f.data_length_code = 9; CHECK(!can.writeFrame(f));
    } else if(!strcmp(test, "concurrent")) {
        CHECK(can.begin(TWAI_SPEED_500KBPS, 18, 17, 4, 5));
        std::atomic<unsigned> ready{0}, accepted{0};
        std::atomic<bool> go{false};
        std::thread writers[8];
        for(unsigned i = 0; i < 8; i++) writers[i] = std::thread([&, i]() {
            ready++; while(!go.load()) std::this_thread::yield();
            CanFrame f = {}; f.identifier = 0x100 + i; f.data_length_code = 8; f.data[0] = i;
            if(can.writeFrame(f, 0)) accepted++;
        });
        while(ready.load() != 8) std::this_thread::yield();
        go = true; for(auto& writer : writers) writer.join();
        CHECK(accepted == 4 && can.inTxQueue() == 4 && stubPending(sdk.node) == 4);
        for(unsigned i = 0; i < 4; i++) CHECK(stubFrame(sdk.node, i)->header.id == 0x100U + stubFrame(sdk.node, i)->buffer[0]);
        while(stubPending(sdk.node)) stubComplete(sdk.node, 0, true);
        CHECK(can.inTxQueue() == 0);
    } else return 2;
    CHECK(can.end()); CHECK(can.end());
    CHECK(!sdk.liveNodes && !sdk.liveQueues && !sdk.liveShadows);
    printf("%s: %d failures\n", test, failures);
    return failures ? 1 : 0;
}
