#ifndef ESP32_TWAI_CAN_HPP
#define ESP32_TWAI_CAN_HPP

/**
 * @file ESP32-TWAI-CAN.hpp
 * @author sorek (contact@sorek.uk)
 * @brief ESP32 driver for TWAI / CAN for Adruino using ESP-IDF drivers.
 * @version 1.0
 * @date 2023-12-15
 * 
 * @copyright Copyright (c) 2023
 * 
 * I tried to create as simple and as lightweight Arduino ESP32 TWAI / CAN library
 * as possible. Currently testing it has very small footprint both on ESP32 and ESP32-S3.
 * 
 * Simply declare your rx and tx frames using 'CanFrame' structures and you are good to go!
 * 
 */
#ifdef ARDUINO
#include <Arduino.h>
#else
#include "inttypes.h"
#endif
#include "driver/twai.h"


// Uncomment or declare before importing header
//#define LOG_TWAI log_e
//#define LOG_TWAI_TX log_e
//#define LOG_TWAI_RX log_e

#ifndef LOG_TWAI
#define LOG_TWAI
#endif

#ifndef LOG_TWAI_TX
#define LOG_TWAI_TX
#endif

#ifndef LOG_TWAI_RX
#define LOG_TWAI_RX
#endif

typedef twai_message_t CanFrame;

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
    TWAI_SPEED_100KBPS,
    TWAI_SPEED_125KBPS,
    TWAI_SPEED_250KBPS,
    TWAI_SPEED_500KBPS,
    TWAI_SPEED_800KBPS,
    TWAI_SPEED_1000KBPS,
    TWAI_SPEED_SIZE
};

class TwaiCAN {
 public:
    TwaiCAN() {}

    // Call before begin!
    void setSpeed(TwaiSpeed);
    TwaiSpeed getSpeed() { return speed; };

    // Converts from numeric CAN speed to enum values: setSpeed(convertSpeed(500));
    TwaiSpeed convertSpeed(uint16_t canSpeed = 0);
    
    // Size of queues for TWAI-CAN driver - remember about memory constrains!
    void setTxQueueSize(uint16_t);
    void setRxQueueSize(uint16_t);

    // Returns number of messages still in queue
    uint32_t inTxQueue();
    uint32_t inRxQueue();
    
    
    bool setPins(int8_t txPin, int8_t rxPin);

    // Everything is defaulted so you can just call .begin() or .begin(TwaiSpeed)
    // Calling begin() to change speed works, it will disable current driver first
    bool begin(TwaiSpeed twaiSpeed = TWAI_SPEED_SIZE, 
                    int8_t txPin = -1, int8_t rxPin = -1,
                    uint16_t txQueue = 0xFFFF, uint16_t rxQueue = 0xFFFF,
                    twai_filter_config_t*  fConfig = nullptr,
                    twai_general_config_t* gConfig = nullptr,
                    twai_timing_config_t*  tConfig = nullptr);
    
    // Pass frame either by reference or pointer; timeout in ms, you can pass 0 for non blocking
    inline bool IRAM_ATTR readFrame(CanFrame& frame, uint32_t timeout = 1000) { return readFrame(&frame, timeout); }
    inline bool IRAM_ATTR readFrame(CanFrame* frame, uint32_t timeout = 1000) {
        bool ret = false;
        if((frame) && twai_receive(frame, pdMS_TO_TICKS(timeout)) == ESP_OK) {
            LOG_TWAI_RX("Frame received %03X", frame->identifier);
            ret = true;
        }
        return ret;
    }

    // Pass frame either by reference or pointer; timeout in ms, you can pass 0 for non blocking
    inline bool IRAM_ATTR writeFrame(CanFrame& frame, uint32_t timeout = 1) { return writeFrame(&frame, timeout); }
    inline bool IRAM_ATTR writeFrame(CanFrame* frame, uint32_t timeout = 1) {
        bool ret = false;
        if((frame) && twai_transmit(frame, pdMS_TO_TICKS(timeout)) == ESP_OK) {
            LOG_TWAI_TX("Frame sent     %03X", frame->identifier);
            ret = true;
        }
        return ret;
    }

    bool end();

 protected:
    twai_status_info_t status;
    bool getStatusInfo();

 private:
    bool init = false;
    int8_t tx = 5;
    int8_t rx = 4;
    uint16_t txQueueSize = 5;
    uint16_t rxQueueSize = 5;
    TwaiSpeed speed = TWAI_SPEED_500KBPS;
};

extern TwaiCAN ESP32Can;

#endif//ESP32_TWAI_CAN_HPP