# ESP32-TWAI-CAN — Technical Reference (Agents & Developers)

Single consolidated technical doc for the ESP32-TWAI-CAN library: API, usage
patterns, and conventions. For the project/owner front page see `README.md`.
**Keep it in sync with the code** — the library is one header + one .cpp, so
there is no second doc to drift against.

## What This Library Does

Minimal Arduino-style wrapper over the ESP-IDF TWAI (CAN 2.0) controller
driver. `CanFrame` **is** `twai_message_t` — there is no conversion layer.
`TwaiCAN` handles driver install/start/stop/uninstall, speed/pin/queue
configuration, status counters, and bus recovery. A global `ESP32Can`
singleton instance is provided and is what Gauge.S uses everywhere.

**It does NOT**: decode protocols (OBD-II/KWP live in CarDataS), do acceptance
filtering beyond the config you pass in, or support CAN FD.

## Features

- Thin: `readFrame()`/`writeFrame()` map 1:1 to `twai_receive`/`twai_transmit`
- Sane defaults: 500 kbps, TX pin 5 / RX pin 4, TX/RX queues of 5,
  accept-all filter, normal mode
- `TwaiSpeed` enum with chip-conditional entries; `convertSpeed()` maps
  numeric kbps to the enum
- Re-entrant `begin()` — safe to call again to change speed/pins (calls
  `end()` internally first)
- Status: TX/RX queue depth, RX/TX error counters, missed/failed frame
  counters, bus error counter, controller state
- `recover()` / `restart()` helpers for BUS_OFF and STOPPED states
- Optional debug logging via `LOG_TWAI*` macros (zero cost by default)
- IRAM-safe read/write when `CONFIG_TWAI_ISR_IN_IRAM` is set

## Hardware

Requires an external 3.3V CAN transceiver (TI SN65HVD230 or similar
recommended). Tested on ESP32 and ESP32-S3; builds for any ESP32 variant with
a TWAI peripheral (S2, C3, ...). The speed-enum low-bitrate entries are
compile-time gated on `SOC_TWAI_BRP_MAX` / `CONFIG_ESP32_REV_MIN_FULL`.

## Types

```cpp
typedef twai_message_t CanFrame;   // the IDF struct, used directly
// fields: identifier, extd, rtr, ss, self, data_length_code, data[8]
```

`enum TwaiSpeed : uint8_t` (from the header, order matters — the timing table
is indexed by it):

| Members | Availability |
|---------|--------------|
| `TWAI_SPEED_1KBPS`, `_5KBPS`, `_10KBPS` | only when `SOC_TWAI_BRP_MAX > 256` |
| `TWAI_SPEED_12_5KBPS`, `_16KBPS`, `_20KBPS` | `SOC_TWAI_BRP_MAX > 128` or ESP32 rev >= 2 |
| `TWAI_SPEED_50KBPS`, `_100KBPS`, `_125KBPS`, `_250KBPS`, `_500KBPS`, `_800KBPS`, `_1000KBPS` | always |
| `TWAI_SPEED_SIZE` | sentinel — "keep current" for `begin()` |

## API Reference (class TwaiCAN)

Configuration (call before `begin()`):

```cpp
void      setSpeed(TwaiSpeed);            // invalid values ignored
TwaiSpeed getSpeed();
uint32_t  getSpeedNumeric();              // kbps (12.5k returns 12)
TwaiSpeed convertSpeed(uint16_t kbps);    // 12 or 13 -> 12.5; unknown keeps current
void      setTxQueueSize(uint16_t);       // 0xFFFF = keep current
void      setRxQueueSize(uint16_t);       // 0xFFFF = keep current
bool      setPins(int8_t txPin, int8_t rxPin);  // false if already running
                                                // or a pin is negative
```

(Quirk: `setPins()` calls `LOG_TWAI("Wrong pins...")` unconditionally — the
message appears even on success when `LOG_TWAI` is enabled. Harmless.)

Lifecycle:

```cpp
bool begin(TwaiSpeed             speed   = TWAI_SPEED_SIZE,  // keep current
           int8_t                txPin   = -1,               // keep current
           int8_t                rxPin   = -1,               // keep current
           uint16_t              txQueue = 0xFFFF,           // keep current
           uint16_t              rxQueue = 0xFFFF,           // keep current
           twai_filter_config_t*  fConfig = nullptr,         // accept-all
           twai_general_config_t* gConfig = nullptr,         // normal mode
           twai_timing_config_t*  tConfig = nullptr);        // timing table
bool end();        // stop + uninstall; returns true when not initialized
bool recover();    // BUS_OFF -> twai_initiate_recovery(); RECOVERING/STOPPED -> true
bool restart();    // STOPPED -> twai_start()
```

`begin()` details (from the .cpp): calls `end()` first, applies speed/pins/
queues, `gpio_reset_pin()` on both pins, installs with `TWAI_MODE_NORMAL`,
`TWAI_ALERT_NONE`, accept-all filter, timing from an internal table indexed by
`TwaiSpeed`, `ESP_INTR_FLAG_IRAM` when `CONFIG_TWAI_ISR_IN_IRAM` else
`ESP_INTR_FLAG_LEVEL1`. On install/start failure it calls `end()` and returns
false.

Frame I/O (reference and pointer overloads; timeout in ms, 0 = non-blocking):

```cpp
bool readFrame(CanFrame& frame, uint32_t timeout = 1000);
bool readFrame(CanFrame* frame, uint32_t timeout = 1000);   // nullptr-safe
bool writeFrame(const CanFrame& frame, uint32_t timeout = 1);
bool writeFrame(const CanFrame* frame, uint32_t timeout = 1); // nullptr-safe
```

Status (all return 0 when `twai_get_status_info()` fails):

```cpp
uint32_t inTxQueue();        // messages waiting to be transmitted
uint32_t inRxQueue();        // messages waiting to be read
uint32_t rxErrorCounter();
uint32_t txErrorCounter();
uint32_t rxMissedCounter();  // RX FIFO overruns
uint32_t txFailedCounter();
uint32_t busErrCounter();
uint32_t canState();         // raw twai_state_t as uint32
```

Gauge.S maps `canState()` for the web UI as: 1 = RUNNING, 2 = BUS_OFF,
3 = RECOVERING, anything else = STOPPED (see `src/wifi/NetworkHandler.hpp`).

Global instance:

```cpp
extern TwaiCAN ESP32Can;   // defined in ESP32-TWAI-CAN.cpp
```

## Quick Start

```cpp
#include <ESP32-TWAI-CAN.hpp>

#define CAN_TX 5
#define CAN_RX 4

CanFrame rxFrame;

void setup() {
    Serial.begin(115200);

    ESP32Can.setPins(CAN_TX, CAN_RX);          // defaults are 5/4 anyway
    ESP32Can.setRxQueueSize(5);                // optional, these are defaults
    ESP32Can.setTxQueueSize(5);
    ESP32Can.setSpeed(ESP32Can.convertSpeed(500));

    if (ESP32Can.begin()) Serial.println("CAN bus started!");

    // ...or everything in one call (safe to re-call; ends the driver first):
    // ESP32Can.begin(ESP32Can.convertSpeed(500), CAN_TX, CAN_RX, 10, 10);
}

void sendObdFrame(uint8_t pid) {
    CanFrame f         = {0};
    f.identifier       = 0x7DF;   // OBD-II broadcast request
    f.extd             = 0;
    f.data_length_code = 8;
    f.data[0] = 2;  f.data[1] = 1;  f.data[2] = pid;
    f.data[3] = f.data[4] = f.data[5] = f.data[6] = f.data[7] = 0xAA; // padding
    ESP32Can.writeFrame(f);       // default 1 ms TX timeout
}

void loop() {
    static uint32_t lastStamp = 0;
    if (millis() - lastStamp > 1000) { lastStamp = millis(); sendObdFrame(5); }

    if (ESP32Can.readFrame(rxFrame, 1000)) {
        if (rxFrame.identifier == 0x7E8) {     // OBD-II response
            Serial.printf("Coolant: %d C\n", rxFrame.data[3] - 40);
        }
    }
}
```

Full runnable version: `examples/OBD2-query/OBD2-query.ino`.

## Advanced Configuration

Pass your own filter/general/timing configs to `begin()` (any nullptr falls
back to the defaults above). The struct layouts come from the IDF — see the
soc/twai_types.h and driver/twai.h headers of your Arduino-ESP32 core.
Gauge.S uses this for a filtered bus-off self-test in
`src/hardware/Hardware.hpp`:

```cpp
twai_filter_config_t canFiltr = { /* acceptance_code / acceptance_mask */ };
ESP32Can.begin(ESP32Can.convertSpeed(500), CAN_TX, CAN_RX, -1, -1, &canFiltr);
```

## Debug Logging

All logging macros default to empty (zero cost). Define before the include:

```cpp
#define LOG_TWAI    log_e   // driver-level events (install/start/stop/errors)
#define LOG_TWAI_TX log_e   // every transmitted frame
#define LOG_TWAI_RX log_e   // every received frame
#include <ESP32-TWAI-CAN.hpp>
```

## Used in Gauge.S

- `src/hardware/Hardware.hpp` — `initCan()` brings the bus up:
  `ESP32Can.begin(ESP32Can.convertSpeed(DEFAULT_CAN_SPEED), CAN_TX, CAN_RX,
  CAN_TX_QUEUE_SIZE, CAN_RX_QUEUE_SIZE)`; also `ESP32Can.end()` on the
  restart and chassis deep-sleep shutdown paths.
- `src/GaugeApp.hpp` — `canTask` blocks on
  `ESP32Can.readFrame(rxFrame, portMAX_DELAY)`; a watchdog branch checks
  `canState()`/error counters and calls `recover()`/`restart()`; all TX goes
  through `ESP32Can.writeFrame(txFrame, 10)`.
- `src/wifi/NetworkHandler.hpp` — the web status endpoint exposes
  `canState()`, all error/missed/failed counters, queue depths, and
  `getSpeedNumeric()`.
- `lib/CarDataS/` sits on top: its OBD-II and KWP logic is hardware-decoupled
  and this library is the concrete driver behind the app's CAN callbacks.

## Conventions

- Two files under `src/` (`ESP32-TWAI-CAN.hpp` + `ESP32-TWAI-CAN.cpp`);
  `library.json` `srcFilter` builds only the .cpp.
- No dynamic allocation, no STL — C-style wrapper over the IDF driver.
- Version lives in `library.json` (1.0.3) — keep `library.properties` in sync.
- ESP32/Arduino-only by design (wraps the IDF TWAI driver); the WASM simulator
  does not build it — protocol logic above it (CarDataS KWP/OBD) is
  driver-agnostic instead.

## File Structure

```
lib/ESP32-TWAI-CAN/
  src/ESP32-TWAI-CAN.hpp            -- TwaiCAN class, CanFrame typedef, TwaiSpeed enum
  src/ESP32-TWAI-CAN.cpp            -- implementation + ESP32Can singleton definition
  examples/OBD2-query/OBD2-query.ino  -- request/response demo
  library.json                      -- PlatformIO manifest (v1.0.3)
  library.properties                -- Arduino IDE manifest
  keywords.txt                      -- Arduino IDE syntax highlighting
  LICENSE                           -- MIT
```

## License

MIT — see [LICENSE](LICENSE).
