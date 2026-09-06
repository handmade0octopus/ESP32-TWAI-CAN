#include "ESP32-TWAI-CAN.hpp"
#include "driver/gpio.h"

void TwaiCAN::setSpeed(TwaiSpeed twaiSpeed) {
    if(twaiSpeed < TWAI_SPEED_SIZE) speed = twaiSpeed;
}

// clang-format off
uint32_t TwaiCAN::getSpeedNumeric() { 
    uint32_t actualSpeed = 500;
    switch(getSpeed()) {
        default: break;
        #if (SOC_TWAI_BRP_MAX > 256)
        case TWAI_SPEED_1KBPS   :   actualSpeed = 1   ; break;
        case TWAI_SPEED_5KBPS   :   actualSpeed = 5   ; break;
        case TWAI_SPEED_10KBPS  :   actualSpeed = 10  ; break;
        #endif
        #if (SOC_TWAI_BRP_MAX > 128) || (CONFIG_ESP32_REV_MIN_FULL >= 200)
        case TWAI_SPEED_12_5KBPS:   actualSpeed = 12  ; break;
        case TWAI_SPEED_16KBPS  :   actualSpeed = 16  ; break;
        case TWAI_SPEED_20KBPS  :   actualSpeed = 20  ; break;
        #endif
        case TWAI_SPEED_50KBPS  :   actualSpeed = 50  ; break;
        case TWAI_SPEED_100KBPS :   actualSpeed = 100 ; break;
        case TWAI_SPEED_125KBPS :   actualSpeed = 125 ; break;
        case TWAI_SPEED_250KBPS :   actualSpeed = 250 ; break;
        case TWAI_SPEED_500KBPS :   actualSpeed = 500 ; break;
        case TWAI_SPEED_800KBPS :   actualSpeed = 800 ; break;
        case TWAI_SPEED_1000KBPS:   actualSpeed = 1000; break;
    }
    return actualSpeed;
}
// clang-format on

#ifdef TWAI_CAN_NEW_DRIVER

/* Exact bit/s for the new driver (getSpeedNumeric() rounds 12.5k down to 12). */
static uint32_t speedToBps(TwaiSpeed s) {
    switch(s) {
        #if (SOC_TWAI_BRP_MAX > 256)
        case TWAI_SPEED_1KBPS: return 1000;
        case TWAI_SPEED_5KBPS: return 5000;
        case TWAI_SPEED_10KBPS: return 10000;
        #endif
        #if (SOC_TWAI_BRP_MAX > 128) || (CONFIG_ESP32_REV_MIN_FULL >= 200)
        case TWAI_SPEED_12_5KBPS: return 12500;
        case TWAI_SPEED_16KBPS: return 16000;
        case TWAI_SPEED_20KBPS: return 20000;
        #endif
        case TWAI_SPEED_50KBPS: return 50000;
        case TWAI_SPEED_100KBPS: return 100000;
        case TWAI_SPEED_125KBPS: return 125000;
        case TWAI_SPEED_250KBPS: return 250000;
        case TWAI_SPEED_500KBPS: return 500000;
        case TWAI_SPEED_800KBPS: return 800000;
        case TWAI_SPEED_1000KBPS: return 1000000;
        default: return 500000;
    }
}

#endif /* TWAI_CAN_NEW_DRIVER */

// clang-format off
TwaiSpeed TwaiCAN::convertSpeed(uint16_t canSpeed) { 
    TwaiSpeed actualSpeed = getSpeed();
    switch(canSpeed) {
        default: break;
        #if (SOC_TWAI_BRP_MAX > 256)
        case 1:     actualSpeed = TWAI_SPEED_1KBPS;     break;
        case 5:     actualSpeed = TWAI_SPEED_5KBPS;     break;
        case 10:    actualSpeed = TWAI_SPEED_10KBPS;    break;
        #endif
        #if (SOC_TWAI_BRP_MAX > 128) || (CONFIG_ESP32_REV_MIN_FULL >= 200)
        case 13:    // Just to handle those who would round upwards..
        case 12:    actualSpeed = TWAI_SPEED_12_5KBPS;  break;
        case 16:    actualSpeed = TWAI_SPEED_16KBPS;    break;
        case 20:    actualSpeed = TWAI_SPEED_20KBPS;    break;
        #endif
        case 50:    actualSpeed = TWAI_SPEED_50KBPS;    break;
        case 100:   actualSpeed = TWAI_SPEED_100KBPS;   break;
        case 125:   actualSpeed = TWAI_SPEED_125KBPS;   break;
        case 250:   actualSpeed = TWAI_SPEED_250KBPS;   break;
        case 500:   actualSpeed = TWAI_SPEED_500KBPS;   break;
        case 800:   actualSpeed = TWAI_SPEED_800KBPS;   break;
        case 1000:  actualSpeed = TWAI_SPEED_1000KBPS;  break;
    }
    return actualSpeed;
}
// clang-format on

void TwaiCAN::setTxQueueSize(uint16_t txQueue) {
    if(txQueue != 0xFFFF) txQueueSize = txQueue;
}

void TwaiCAN::setRxQueueSize(uint16_t rxQueue) {
    if(rxQueue != 0xFFFF) rxQueueSize = rxQueue;
}

#ifdef TWAI_CAN_NEW_DRIVER

/* ────────────────── New driver (multi-controller) path ────────────────── */

bool IRAM_ATTR_TWAI TwaiCAN::rxDoneCb(twai_node_handle_t handle, const twai_rx_done_event_data_t* edata, void* ctx) {
    (void)edata;
    TwaiCAN*   self  = (TwaiCAN*)ctx;
    BaseType_t woken = pdFALSE;
    twai_frame_t rf  = {};
    uint8_t     buf[8];
    rf.buffer     = buf;
    rf.buffer_len = sizeof(buf);
    if(twai_node_receive_from_isr(handle, &rf) == ESP_OK) {
        CanFrame cf = {};
        cf.identifier       = rf.header.id;
        cf.extd             = rf.header.ide;
        cf.rtr              = rf.header.rtr;
        cf.data_length_code = (uint8_t)(rf.header.dlc > 8 ? 8 : rf.header.dlc);
        memcpy(cf.data, buf, cf.data_length_code);
        if(!xQueueSendFromISR(self->rxQueue, &cf, &woken)) self->rxMissed = self->rxMissed + 1;
    }
    return woken == pdTRUE;
}

bool IRAM_ATTR_TWAI TwaiCAN::txDoneCb(twai_node_handle_t handle, const twai_tx_done_event_data_t* edata, void* ctx) {
    (void)handle;
    TwaiCAN* self = (TwaiCAN*)ctx;
    if(self->txShadow && edata->done_tx_frame) {
        const uint8_t* done = edata->done_tx_frame->buffer;
        for(uint16_t i = 0; i < self->txShadowCount; i++) {
            if(self->txShadow[i].frame.buffer == done) {
                self->txShadow[i].busy = false;
                break;
            }
        }
    }
    return false;
}

uint32_t TwaiCAN::inTxQueue() {
    uint32_t ret = 0;
    for(uint16_t i = 0; i < txShadowCount; i++) {
        if(txShadow[i].busy) ret++;
    }
    return ret;
};

uint32_t TwaiCAN::inRxQueue() {
    return rxQueue ? uxQueueMessagesWaiting(rxQueue) : 0;
};

uint32_t TwaiCAN::rxErrorCounter() {
    twai_node_status_t st;
    return getNewNodeStatus(&st) ? st.rx_error_count : 0;
};

uint32_t TwaiCAN::txErrorCounter() {
    twai_node_status_t st;
    return getNewNodeStatus(&st) ? st.tx_error_count : 0;
};

uint32_t TwaiCAN::rxMissedCounter() { return rxMissed; };

uint32_t TwaiCAN::txFailedCounter() { return txFailed; };

uint32_t TwaiCAN::busErrCounter() {
    twai_node_record_t rec;
    return (node && twai_node_get_info(node, nullptr, &rec) == ESP_OK) ? rec.bus_err_num : 0;
};

uint32_t TwaiCAN::canState() {
    uint32_t          ret = 0;
    twai_node_status_t st;
    if(getNewNodeStatus(&st)) {
        if(st.state == TWAI_ERROR_BUS_OFF) {
            ret = recovering ? TWAI_STATE_RECOVERING : TWAI_STATE_BUS_OFF;
        } else {
            recovering = false;
            ret        = TWAI_STATE_RUNNING;
        }
    }
    return ret;
};

bool TwaiCAN::recover(void) {
    twai_node_status_t st;
    if(!getNewNodeStatus(&st)) {
        LOG_TWAI("CAN bus status read failed!");
        return false;
    }
    if(st.state == TWAI_ERROR_BUS_OFF) {
        LOG_TWAI("Bus was off, starting recovery");
        recovering = true;
        return twai_node_recover(node) == ESP_OK;
    }
    // Already ok (or recovering), nothing to do
    return true;
}

bool TwaiCAN::restart(void) {
    if(!node) return false;
    esp_err_t e = twai_node_enable(node);
    return (e == ESP_OK) || (e == ESP_ERR_INVALID_STATE); // already enabled counts as started
}

bool TwaiCAN::begin(TwaiSpeed            twaiSpeed,
                    int8_t               txPin,
                    int8_t               rxPin,
                    uint16_t             txQueue,
                    uint16_t             rxQueue,
                    twai_filter_compat_t fConfig,
                    twai_general_compat_t gConfig,
                    twai_timing_compat_t tConfig) {
    (void)fConfig;
    (void)gConfig;
    (void)tConfig;
    bool ret = false;
    if(end()) {
        init = true;
        setSpeed(twaiSpeed);
        setPins(txPin, rxPin);
        setTxQueueSize(txQueue);
        setRxQueueSize(rxQueue);

        gpio_reset_pin((gpio_num_t)rx);
        gpio_reset_pin((gpio_num_t)tx);

        twai_onchip_node_config_t cfg = {};
        cfg.io_cfg.tx                = (gpio_num_t)tx;
        cfg.io_cfg.rx                = (gpio_num_t)rx;
        cfg.io_cfg.quanta_clk_out    = (gpio_num_t)-1;
        cfg.io_cfg.bus_off_indicator = (gpio_num_t)-1;
        cfg.bit_timing.bitrate       = speedToBps(speed);
        cfg.tx_queue_depth           = txQueueSize;
        cfg.fail_retry_cnt           = -1; // retransmit until success or bus-off (CAN default)

        // Install TWAI node (the driver picks the next free controller)
        if(twai_new_node_onchip(&cfg, &node) == ESP_OK) {
            LOG_TWAI("Driver installed");
        } else {
            LOG_TWAI("Failed to install driver");
            node = nullptr;
        }

        if(node) {
            txShadowCount = (txQueueSize < 4) ? 4 : txQueueSize;
            txShadow      = (TxShadow*)calloc(txShadowCount, sizeof(TxShadow));
            this->rxQueue = xQueueCreate(rxQueueSize, sizeof(CanFrame));
            if(!txShadow || !rxQueue) {
                LOG_TWAI("Queue allocation failed");
            } else {
                twai_event_callbacks_t cbs = {};
                cbs.on_rx_done = rxDoneCb;
                cbs.on_tx_done = txDoneCb;
                twai_node_register_event_callbacks(node, &cbs, this);

                // Start TWAI node
                if(twai_node_enable(node) == ESP_OK) {
                    LOG_TWAI("Driver started");
                    ret = true;
                } else {
                    LOG_TWAI("Failed to start driver");
                }
            }
        }
        if(!ret) end();
    }
    return ret;
}

bool TwaiCAN::end() {
    bool ret = false;
    if(init) {
        if(node) {
            twai_node_disable(node);
            twai_node_delete(node);
            node       = nullptr;
            recovering = false;
            LOG_TWAI("Driver stopped\n");
        }
        if(rxQueue) {
            vQueueDelete(rxQueue);
            rxQueue = nullptr;
        }
        if(txShadow) {
            free(txShadow);
            txShadow      = nullptr;
            txShadowCount = 0;
        }
        init = false;
        ret  = true;
    } else
        ret = true;
    return ret;
}

#else /* legacy driver path */

/* ──────────────────── Legacy driver (single instance) ─────────────────── */

bool TwaiCAN::getStatusInfo() {
    return ESP_OK == twai_get_status_info(&status);
}

uint32_t TwaiCAN::inTxQueue() {
    uint32_t ret = 0;
    if(getStatusInfo()) {
        ret = status.msgs_to_tx;
    }
    return ret;
};

uint32_t TwaiCAN::inRxQueue() {
    uint32_t ret = 0;
    if(getStatusInfo()) {
        ret = status.msgs_to_rx;
    }
    return ret;
};

uint32_t TwaiCAN::rxErrorCounter() {
    uint32_t ret = 0;
    if(getStatusInfo()) {
        ret = status.rx_error_counter;
    }
    return ret;
};

uint32_t TwaiCAN::txErrorCounter() {
    uint32_t ret = 0;
    if(getStatusInfo()) {
        ret = status.tx_error_counter;
    }
    return ret;
};

uint32_t TwaiCAN::rxMissedCounter() {
    uint32_t ret = 0;
    if(getStatusInfo()) {
        ret = status.rx_missed_count;
    }
    return ret;
};

uint32_t TwaiCAN::txFailedCounter() {
    uint32_t ret = 0;
    if(getStatusInfo()) {
        ret = status.tx_failed_count;
    }
    return ret;
};

uint32_t TwaiCAN::busErrCounter() {
    uint32_t ret = 0;
    if(getStatusInfo()) {
        ret = status.bus_error_count;
    }
    return ret;
};

uint32_t TwaiCAN::canState() {
    uint32_t ret = 0;
    if(getStatusInfo()) {
        ret = (uint32_t)status.state;
    }
    return ret;
};

bool TwaiCAN::recover(void) {
    uint32_t ret = 0;
    if(!getStatusInfo()) {
        LOG_TWAI("CAN bus status read failed!");
        return false;
    }
    switch(status.state) {
        case TWAI_STATE_BUS_OFF: {
            LOG_TWAI("Bus was off, starting recovery");
            return twai_initiate_recovery();
        }
        case TWAI_STATE_RECOVERING: {
            // Already recovering, nothing to do
            return true;
        }
        case TWAI_STATE_STOPPED: {
            // Stopped, nothing to do
            return true;
        }
        default: {
            LOG_TWAI("Wrong state for recovery!");
        }
    }

    return false;
}

bool TwaiCAN::restart(void) {
    uint32_t ret = 0;
    if(!getStatusInfo()) {
        LOG_TWAI("CAN bus status read failed!");
        return false;
    }
    switch(status.state) {
        case TWAI_STATE_STOPPED: {
            // Stopped, restart
            return twai_start();
        }
        default: {
            LOG_TWAI("Wrong state for restart!");
        }
    }

    return false;
}

bool TwaiCAN::begin(TwaiSpeed              twaiSpeed,
                    int8_t                 txPin,
                    int8_t                 rxPin,
                    uint16_t               txQueue,
                    uint16_t               rxQueue,
                    twai_filter_compat_t   fConfig,
                    twai_general_compat_t  gConfig,
                    twai_timing_compat_t   tConfig) {
    bool ret = false;
    if(end()) {
        init = true;
        setSpeed(twaiSpeed);
        setPins(txPin, rxPin);

        gpio_reset_pin((gpio_num_t)rx);
        gpio_reset_pin((gpio_num_t)tx);

        setTxQueueSize(txQueue);
        setRxQueueSize(rxQueue);

        twai_general_config_t g_config = {.mode           = TWAI_MODE_NORMAL,
                                          .tx_io          = (gpio_num_t)tx,
                                          .rx_io          = (gpio_num_t)rx,
                                          .clkout_io      = TWAI_IO_UNUSED,
                                          .bus_off_io     = TWAI_IO_UNUSED,
                                          .tx_queue_len   = txQueueSize,
                                          .rx_queue_len   = rxQueueSize,
                                          .alerts_enabled = TWAI_ALERT_NONE,
                                          .clkout_divider = 0,
                                    #if CONFIG_TWAI_ISR_IN_IRAM
                                          .intr_flags     = ESP_INTR_FLAG_IRAM  };
                                    #else
                                          .intr_flags     = ESP_INTR_FLAG_LEVEL1};
                                    #endif
        // clang-format off
        twai_timing_config_t t_config[TWAI_SPEED_SIZE] = {
            #if (SOC_TWAI_BRP_MAX > 256)
            TWAI_TIMING_CONFIG_1KBITS(),
            TWAI_TIMING_CONFIG_5KBITS(),
            TWAI_TIMING_CONFIG_10KBITS(),
            #endif
            #if (SOC_TWAI_BRP_MAX > 128) || (CONFIG_ESP32_REV_MIN_FULL >= 200)
            TWAI_TIMING_CONFIG_12_5KBITS(),
            TWAI_TIMING_CONFIG_16KBITS(),
            TWAI_TIMING_CONFIG_20KBITS(),
            #endif
            TWAI_TIMING_CONFIG_50KBITS(),
            TWAI_TIMING_CONFIG_100KBITS(),
            TWAI_TIMING_CONFIG_125KBITS(),
            TWAI_TIMING_CONFIG_250KBITS(),
            TWAI_TIMING_CONFIG_500KBITS(),
            TWAI_TIMING_CONFIG_800KBITS(),
            TWAI_TIMING_CONFIG_1MBITS()
        };
        // clang-format on

        twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

        if(!gConfig) gConfig = &g_config;
        if(!tConfig) tConfig = &t_config[speed];
        if(!fConfig) fConfig = &f_config;

        // Install TWAI driver
        if(twai_driver_install(gConfig, tConfig, fConfig) == ESP_OK) {
            LOG_TWAI("Driver installed");
        } else {
            LOG_TWAI("Failed to install driver");
        }

        // Start TWAI driver
        if(twai_start() == ESP_OK) {
            LOG_TWAI("Driver started");
            ret = true;
        } else {
            LOG_TWAI("Failed to start driver");
        }
        if(!ret) end();
    }
    return ret;
}

bool TwaiCAN::end() {
    bool ret = false;
    if(init) {
        // Stop the TWAI driver
        if(twai_stop() == ESP_OK) {
            LOG_TWAI("Driver stopped\n");
            ret = true;
        } else {
            LOG_TWAI("Failed to stop driver\n");
        }

        // Uninstall the TWAI driver
        if(twai_driver_uninstall() == ESP_OK) {
            LOG_TWAI("Driver uninstalled\n");
            ret &= true;
        } else {
            LOG_TWAI("Failed to uninstall driver\n");
            ret &= false;
        }
        init = !ret;
    } else
        ret = true;
    return ret;
}

#endif /* TWAI_CAN_NEW_DRIVER */

bool TwaiCAN::setPins(int8_t txPin, int8_t rxPin) {
    bool ret = !init;

    if(txPin >= 0)
        tx = txPin;
    else
        ret = false;
    if(rxPin >= 0)
        rx = rxPin;
    else
        ret = false;

    LOG_TWAI("Wrong pins or CAN bus running already!");
    return ret;
}

TwaiCAN CAN1;
TwaiCAN CAN2;
TwaiCAN& ESP32Can = CAN1;
