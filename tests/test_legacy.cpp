#include "sdk_stub.hpp"

int main(int argc, char** argv) {
    if(argc != 2) return 2;
    const char* test = argv[1];
    TwaiCAN can;
    if(!strcmp(test, "install_failure")) {
        sdk.installResult = ESP_ERR_NO_MEM;
        CHECK(!can.begin()); CHECK(sdk.starts == 0); CHECK(sdk.uninstalls == 0);
        CHECK(can.end());
        sdk.installResult = ESP_OK;
        CHECK(can.begin()); CHECK(sdk.installed);
    } else if(!strcmp(test, "foreign_install")) {
        sdk.installed = true; sdk.status.state = TWAI_STATE_STOPPED;
        CHECK(!can.begin()); CHECK(sdk.starts == 0); CHECK(sdk.stops == 0);
        CHECK(sdk.uninstalls == 0); CHECK(sdk.installed);
        CHECK(can.end()); CHECK(sdk.installed);
        sdk.installed = false;
    } else if(!strcmp(test, "start_failure")) {
        sdk.startResult = ESP_FAIL;
        CHECK(!can.begin()); CHECK(!sdk.installed); CHECK(sdk.uninstalls == 1);
        sdk.startResult = ESP_OK;
        CHECK(can.begin());
    } else if(!strcmp(test, "end_stopped") || !strcmp(test, "end_busoff")) {
        CHECK(can.begin());
        sdk.status.state = !strcmp(test, "end_stopped") ? TWAI_STATE_STOPPED : TWAI_STATE_BUS_OFF;
        CHECK(can.end()); CHECK(!sdk.installed);
        CHECK(can.end()); CHECK(sdk.uninstalls == 1);
        CHECK(can.begin());
    } else if(!strcmp(test, "uninstall_failure")) {
        CHECK(can.begin()); sdk.uninstallResult = ESP_FAIL;
        CHECK(!can.end()); CHECK(sdk.installed);
        CHECK(!can.begin()); CHECK(sdk.installs == 1);
        sdk.uninstallResult = ESP_OK;
        CHECK(can.begin()); CHECK(sdk.installs == 2);
    } else if(!strcmp(test, "end_recovering")) {
        CHECK(can.begin()); sdk.status.state = TWAI_STATE_RECOVERING;
        CHECK(!can.end()); CHECK(sdk.installed);
        sdk.status.state = TWAI_STATE_STOPPED;
        CHECK(can.end()); CHECK(can.begin());
    } else if(!strcmp(test, "recovery")) {
        CHECK(!can.recover()); CHECK(!can.restart());
        CHECK(can.begin()); CHECK(!can.recover()); CHECK(!can.restart());
        sdk.status.state = TWAI_STATE_BUS_OFF; sdk.recoverResult = ESP_FAIL;
        CHECK(!can.recover());
        sdk.recoverResult = ESP_OK;
        CHECK(can.recover()); CHECK(can.canState() == TWAI_STATE_RECOVERING);
        CHECK(can.recover()); CHECK(sdk.recoveries == 2); CHECK(!can.restart());
        sdk.status.state = TWAI_STATE_STOPPED;
        CHECK(can.recover()); sdk.startResult = ESP_FAIL; CHECK(!can.restart());
        sdk.startResult = ESP_OK; CHECK(can.restart()); CHECK(can.canState() == TWAI_STATE_RUNNING);
        sdk.infoResult = ESP_FAIL; CHECK(!can.recover()); CHECK(!can.restart());
        sdk.infoResult = ESP_OK;
    } else if(!strcmp(test, "io")) {
        CHECK(can.begin());
        CanFrame f = {};
        CHECK(!can.readFrame((CanFrame*)nullptr)); CHECK(!can.writeFrame((const CanFrame*)nullptr));
        CHECK(can.readFrame(f, portMAX_DELAY)); CHECK(sdk.rxTicks == portMAX_DELAY);
        CHECK(can.writeFrame(&f, portMAX_DELAY)); CHECK(sdk.txTicks == portMAX_DELAY);
        CHECK(can.readFrame(&f, 0)); CHECK(sdk.rxTicks == 0);
        CHECK(can.writeFrame(f, 0)); CHECK(sdk.txTicks == 0);
        CHECK(can.readFrame(f, 17)); CHECK(sdk.rxTicks == pdMS_TO_TICKS(17));
        CHECK(can.writeFrame(f, 23)); CHECK(sdk.txTicks == pdMS_TO_TICKS(23));
        sdk.received.identifier = 0x1ABCDE; sdk.received.extd = 1;
        sdk.received.data_length_code = 3; sdk.received.data[2] = 0xA5;
        CHECK(can.readFrame(f)); CHECK(f.identifier == 0x1ABCDE && f.extd && f.data[2] == 0xA5);
        f.ss = 1; f.self = 1; CHECK(can.writeFrame(f));
        CHECK(sdk.transmitted.identifier == f.identifier && sdk.transmitted.ss && sdk.transmitted.self);
    } else if(!strcmp(test, "status")) {
        twai_status_info_t first = {}, second = {};
        CHECK(!can.getStatus(nullptr)); CHECK(sdk.statusCalls == 0);
        CHECK(!can.getStatus(&first)); CHECK(sdk.statusOutput == &first);
        CHECK(can.begin());
        sdk.status = {TWAI_STATE_RUNNING, 11, 12, 13, 14, 15, 16, 17};
        CHECK(can.getStatus(&first)); CHECK(sdk.statusOutput == &first);
        CHECK(first.state == TWAI_STATE_RUNNING && first.msgs_to_tx == 11 && first.msgs_to_rx == 12);
        CHECK(first.rx_error_counter == 13 && first.tx_error_counter == 14);
        CHECK(first.rx_missed_count == 15 && first.tx_failed_count == 16 && first.bus_error_count == 17);
        sdk.status.state = TWAI_STATE_BUS_OFF; sdk.status.msgs_to_tx = 21;
        CHECK(can.getStatus(&second)); CHECK(sdk.statusOutput == &second);
        CHECK(second.state == TWAI_STATE_BUS_OFF && second.msgs_to_tx == 21);
        CHECK(can.inTxQueue() == 21);
        CHECK(first.state == TWAI_STATE_RUNNING && first.msgs_to_tx == 11);
        sdk.infoResult = ESP_FAIL;
        CHECK(!can.getStatus(&first)); CHECK(sdk.statusOutput == &first);
        unsigned calls = sdk.statusCalls;
        CHECK(!can.getStatus(nullptr)); CHECK(sdk.statusCalls == calls);
        sdk.infoResult = ESP_OK;
        CHECK(can.end()); CHECK(!can.getStatus(&second));
    } else if(!strcmp(test, "config")) {
        twai_filter_config_t filter = {0x2600FFFF, 0x2600FEFF, true};
        CHECK(can.begin(TWAI_SPEED_500KBPS, 18, 17, 20, 100, &filter));
        CHECK(sdk.general.tx_io == 18 && sdk.general.rx_io == 17);
        CHECK(sdk.general.tx_queue_len == 20 && sdk.general.rx_queue_len == 100);
        CHECK(sdk.filter.acceptance_code == filter.acceptance_code && sdk.filter.acceptance_mask == filter.acceptance_mask);
        CHECK(can.begin(TWAI_SPEED_12_5KBPS)); CHECK(sdk.timing.bitrate == 12500);
        CHECK(sdk.general.tx_queue_len == 20 && sdk.general.rx_queue_len == 100);
        CHECK(sdk.filter.acceptance_mask == UINT32_MAX);
    } else return 2;
    CHECK(can.end());
    CHECK(!sdk.installed);
    printf("%s: %d failures\n", test, failures);
    return failures ? 1 : 0;
}
