#ifndef ESP32_TWAI_CAN_HPP
#define ESP32_TWAI_CAN_HPP

/**
 * @file ESP32-TWAI-CAN.hpp
 * @author sorek (contact@sorek.uk)
 * @brief ESP32 driver for TWAI / CAN for Arduino using ESP-IDF drivers.
 * @version 1.1
 * @date 2023-12-15
 *
 * @copyright Copyright (c) 2023-2026
 *
 * I tried to create as simple and as lightweight Arduino ESP32 TWAI / CAN library
 * as possible. Currently testing it has very small footprint both on ESP32 and ESP32-S3.
 *
 * Simply declare your rx and tx frames using 'CanFrame' structures and you are good to go!
 *
 * 2026-09-06: dual driver path. SoCs with more than one TWAI controller
 * (ESP32-C6, ...) use the new ESP-IDF handle-based driver (esp_twai.h,
 * available in IDF 5.5+) so TWO TwaiCAN instances can run at the same time
 * (globals CAN1 and CAN2, see CanBridge.S). Everything else keeps the legacy
 * single-instance driver (driver/twai.h) unchanged.
 *
 */

#ifdef ARDUINO
# include <Arduino.h>
#else
# include "inttypes.h"
# include "string.h"
# include "freertos/FreeRTOS.h"
# include "freertos/queue.h"
# include "esp_attr.h"
#endif

#include "soc/soc_caps.h"

// Uncomment or declare before importing header
// #define LOG_TWAI log_e
// #define LOG_TWAI_TX log_e
// #define LOG_TWAI_RX log_e

#ifndef LOG_TWAI
# define LOG_TWAI
#endif

#ifndef LOG_TWAI_TX
# define LOG_TWAI_TX
#endif

#ifndef LOG_TWAI_RX
# define LOG_TWAI_RX
#endif

#if CONFIG_TWAI_ISR_IN_IRAM || CONFIG_TWAI_ISR_CACHE_SAFE
#define IRAM_ATTR_TWAI IRAM_ATTR
#else
#define IRAM_ATTR_TWAI
#endif

/* ── Driver selection ───────────────────────────────────────────────────────
 * New (multi-controller) driver when the SoC has >1 TWAI controller AND the
 * new headers exist (IDF 5.5+). Otherwise the legacy single-instance driver.
 */
#if defined(SOC_TWAI_CONTROLLER_NUM) && (SOC_TWAI_CONTROLLER_NUM > 1) && __has_include(<esp_twai_onchip.h>)
#define TWAI_CAN_NEW_DRIVER 1
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#else
#undef TWAI_CAN_NEW_DRIVER
#include "driver/twai.h"
#endif

#ifdef TWAI_CAN_NEW_DRIVER

/* Frame layout identical to twai_message_t (legacy driver) so application
 * code written against the legacy API compiles unchanged. */
struct CanFrame {
    uint32_t identifier;
    union {
        struct {
            uint32_t extd : 1;
            uint32_t rtr : 1;
            uint32_t ss : 1;
            uint32_t self : 1;
            uint32_t dlc_non_comp : 1;
            uint32_t reserved : 27;
        };
        uint32_t flags;
    };
    uint8_t data_length_code;
    uint8_t data[8];
};

typedef bool (*TwaiFrameObserver)(const CanFrame& frame, bool transmitted, uint32_t stampMs, void* context);

struct TwaiDiagnostics {
    uint32_t accepted, completed, failed, rejected;
    uint32_t rxMissed, busErrors;
    uint16_t txErrors, rxErrors;
    int errorState;
};

/* Legacy driver states, kept numerically identical (driver/twai.h) so
 * applications comparing against TWAI_STATE_* keep working. */
typedef enum {
    TWAI_STATE_STOPPED = 0,
    TWAI_STATE_RUNNING = 1,
    TWAI_STATE_BUS_OFF = 2,
    TWAI_STATE_RECOVERING = 3,
} twai_state_compat_t;

/* Legacy config pointers are not meaningful for the new driver; the params
 * stay in the signature (as void*) so existing call sites compile. */
typedef void* twai_filter_compat_t;
typedef void* twai_general_compat_t;
typedef void* twai_timing_compat_t;

#else /* legacy driver */

typedef twai_message_t CanFrame;
typedef twai_filter_config_t* twai_filter_compat_t;
typedef twai_general_config_t* twai_general_compat_t;
typedef twai_timing_config_t* twai_timing_compat_t;

#endif /* TWAI_CAN_NEW_DRIVER */

// clang-format off
enum TwaiSpeed : uint8_t {
    #if (SOC_TWAI_BRP_MAX > 256)
    TWAI_SPEED_1KBPS,
    TWAI_SPEED_5KBPS,
    TWAI_SPEED_10KBPS,
    #endif
    #if (SOC_TWAI_BRP_MAX > 128) || (CONFIG_ESP32_REV_MIN_FULL >= 200)
    TWAI_SPEED_12_5KBPS,
    TWAI_SPEED_16KBPS,
    TWAI_SPEED_20KBPS,
    #endif
    TWAI_SPEED_50KBPS,
    TWAI_SPEED_100KBPS,
    TWAI_SPEED_125KBPS,
    TWAI_SPEED_250KBPS,
    TWAI_SPEED_500KBPS,
    TWAI_SPEED_800KBPS,
    TWAI_SPEED_1000KBPS,
    TWAI_SPEED_SIZE
};
// clang-format on

class TwaiCAN {
  public:
    TwaiCAN() {}

    // Call before begin!
    void      setSpeed(TwaiSpeed);
    TwaiSpeed getSpeed() { return speed; };
    uint32_t  getSpeedNumeric();

    // Converts from numeric CAN speed to enum values: setSpeed(convertSpeed(500));
    TwaiSpeed convertSpeed(uint16_t canSpeed = 0);

    // Size of queues for TWAI-CAN driver - remember about memory constrains!
    void setTxQueueSize(uint16_t);
    void setRxQueueSize(uint16_t);

    // Returns number of messages still in queue
    uint32_t inTxQueue();
    uint32_t inRxQueue();

    uint32_t rxErrorCounter();
    uint32_t txErrorCounter();
    uint32_t rxMissedCounter();
    uint32_t txFailedCounter();
    uint32_t busErrCounter();
    uint32_t canState();

#ifdef TWAI_CAN_NEW_DRIVER
    // Optional ISR observer of received and successfully transmitted frames.
    // Set before begin(); observer must be IRAM-safe when cache-safe ISR is enabled.
    void setFrameObserver(TwaiFrameObserver observer, void* context) { frameObserver = observer; observerContext = context; }
    bool getDiagnostics(TwaiDiagnostics* out);
#endif

    bool setPins(int8_t txPin, int8_t rxPin);

    // Everything is defaulted so you can just call .begin() or .begin(TwaiSpeed)
    // Calling begin() to change speed works, it will disable current driver first
    // NOTE (new driver): fConfig/gConfig/tConfig are ignored - the new driver
    // configures accept-all filtering and bitrate-derived timing itself.
    bool begin(TwaiSpeed          twaiSpeed = TWAI_SPEED_SIZE,
               int8_t             txPin     = -1,
               int8_t             rxPin     = -1,
               uint16_t           txQueue   = 0xFFFF,
               uint16_t           rxQueue   = 0xFFFF,
               twai_filter_compat_t  fConfig = nullptr,
               twai_general_compat_t gConfig = nullptr,
               twai_timing_compat_t  tConfig = nullptr);

    bool recover(void);

    bool restart(void);

    // Pass frame either by reference or pointer; timeout in ms, you can pass 0 for non blocking
    inline bool IRAM_ATTR_TWAI readFrame(CanFrame* frame, uint32_t timeout = 1000) { return (frame) && readFrame(*frame, timeout); }
#ifdef TWAI_CAN_NEW_DRIVER
    inline bool IRAM_ATTR_TWAI readFrame(CanFrame& frame, uint32_t timeout = 1000) {
        if(!rxQueue) return false;
        TickType_t ticks = (timeout == (uint32_t)portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout);
        bool       ret   = xQueueReceive(rxQueue, &frame, ticks) == pdTRUE;
        if(ret) LOG_TWAI_RX("Frame received %03X", frame.identifier);
        return ret;
    }
#else
    inline bool IRAM_ATTR_TWAI readFrame(CanFrame& frame, uint32_t timeout = 1000) {
        bool ret = false;
        if(twai_receive(&frame, pdMS_TO_TICKS(timeout)) == ESP_OK) {
            LOG_TWAI_RX("Frame received %03X", frame.identifier);
            ret = true;
        }
        return ret;
    }
#endif

    // Pass frame either by reference or pointer; timeout in ms, you can pass 0 for non blocking
    inline bool IRAM_ATTR_TWAI writeFrame(const CanFrame* frame, uint32_t timeout = 1) { return (frame) && writeFrame(*frame, timeout); }
#ifdef TWAI_CAN_NEW_DRIVER
    /* The new driver DMA-reads the payload after the call returns, so frames
     * are copied into per-instance shadow slots (freed from the on_tx_done
     * ISR callback). Slot claims are serialized; copying and driver waits are
     * outside the lock. Serialize begin()/end() against all frame I/O. */
    inline bool IRAM_ATTR_TWAI writeFrame(const CanFrame& frame, uint32_t timeout = 1) {
        bool ret = false;
        if(node && txShadow && frame.data_length_code <= 8 &&
           frame.identifier <= (frame.extd ? 0x1FFFFFFFu : 0x7FFu)) {
            TxShadow* slot = nullptr;
            portENTER_CRITICAL(&txMux);
            for(uint16_t i = 0; i < txShadowCount; i++) {
                if(!txShadow[i].busy) {
                    slot = &txShadow[i];
                    slot->busy = true;
                    break;
                }
            }
            portEXIT_CRITICAL(&txMux);
            if(slot) {
                memcpy(slot->data, frame.data, frame.data_length_code);
                slot->frame.header.id  = frame.identifier;
                slot->frame.header.ide = frame.extd;
                slot->frame.header.rtr = frame.rtr;
                slot->frame.header.dlc = frame.data_length_code;
                slot->frame.buffer     = slot->data;
                slot->frame.buffer_len = frame.data_length_code;
                int tmo                = (timeout == (uint32_t)portMAX_DELAY) ? -1 : (int)timeout;
                if(twai_node_transmit(node, &slot->frame, tmo) == ESP_OK) {
                    portENTER_CRITICAL(&txMux);
                    txAccepted = txAccepted + 1;
                    portEXIT_CRITICAL(&txMux);
                    LOG_TWAI_TX("Frame sent     %03X", frame.identifier);
                    ret = true;
                } else {
                    portENTER_CRITICAL(&txMux);
                    slot->busy = false;
                    portEXIT_CRITICAL(&txMux);
                }
            }
        }
        if(!ret) {
            portENTER_CRITICAL(&txMux);
            txFailed = txFailed + 1;
            portEXIT_CRITICAL(&txMux);
        }
        return ret;
    }
#else
    inline bool IRAM_ATTR_TWAI writeFrame(const CanFrame& frame, uint32_t timeout = 1) {
        bool ret = false;
        if(twai_transmit(&frame, pdMS_TO_TICKS(timeout)) == ESP_OK) {
            LOG_TWAI_TX("Frame sent     %03X", frame.identifier);
            ret = true;
        }
        return ret;
    }
#endif

    bool end();

  protected:
#ifdef TWAI_CAN_NEW_DRIVER
    bool getNewNodeStatus(twai_node_status_t* st) { return node && (twai_node_get_info(node, st, nullptr) == ESP_OK); }
#else
    twai_status_info_t status;
    bool               getStatusInfo();
#endif

  private:
#ifdef TWAI_CAN_NEW_DRIVER
    struct TxShadow {
        twai_frame_t   frame;
        uint8_t        data[8];
        volatile bool busy;
    };
    static bool IRAM_ATTR_TWAI rxDoneCb(twai_node_handle_t handle, const twai_rx_done_event_data_t* edata, void* ctx);
    static bool IRAM_ATTR_TWAI txDoneCb(twai_node_handle_t handle, const twai_tx_done_event_data_t* edata, void* ctx);

    twai_node_handle_t node          = nullptr;
    QueueHandle_t      rxQueue       = nullptr;
    TwaiFrameObserver  frameObserver = nullptr;
    void*              observerContext = nullptr;
    TxShadow*          txShadow      = nullptr;
    uint16_t           txShadowCount = 0;
    portMUX_TYPE       txMux = portMUX_INITIALIZER_UNLOCKED;
    volatile uint32_t  rxMissed      = 0;
    volatile uint32_t  txFailed      = 0;
    volatile uint32_t  txAccepted    = 0;
    volatile uint32_t  txCompleted   = 0;
    volatile uint32_t  txBusFailed   = 0;
    volatile bool      recovering    = false;
#endif
    bool      init        = false;
    int8_t    tx          = 5;
    int8_t    rx          = 4;
    uint16_t  txQueueSize = 5;
    uint16_t  rxQueueSize = 5;
    TwaiSpeed speed       = TWAI_SPEED_500KBPS;
};

/* Single-controller projects (Gauge.S) keep using ESP32Can; multi-controller
 * projects (CanBridge.S) use CAN1/CAN2 - ESP32Can stays an alias of CAN1. */
extern TwaiCAN CAN1;
extern TwaiCAN CAN2;
extern TwaiCAN& ESP32Can;

#endif // ESP32_TWAI_CAN_HPP
