# ESP32-TWAI-CAN

Lightweight Arduino-style TWAI / CAN driver for **[Gauge.S](https://sorek.uk)** —
the open automotive gauge / data logger by **[sorek.uk](https://sorek.uk)**
([shop.sorek.uk](https://shop.sorek.uk)).

A minimal wrapper over the ESP-IDF TWAI (CAN 2.0) controller: declare a
`CanFrame` (a plain `twai_message_t` — no conversion layer), call
`ESP32Can.begin(...)`, then `readFrame()` / `writeFrame()`. Sane defaults
(500 kbps, accept-all filter), re-entrant `begin()` for speed changes, full
status counters, and BUS_OFF recovery helpers. Tested on ESP32 and ESP32-S3;
it drives the Gauge.S CAN bus — OBD-II, KWP-over-CAN (via
[CarDataS](https://github.com/handmade0octopus/CarDataS.git)), and the raw
frame sniffer — at up to 1 Mbps.

## Highlights

- **Thin two-file library** — `CanFrame` = `twai_message_t`, zero conversion
- **One-call `begin()`** with sensible defaults; safe re-begin to change speed
- **Status + recovery** — error/missed/failed counters, queue depths,
  `recover()` / `restart()` for BUS_OFF / STOPPED
- **Zero-cost debug logging** via optional `LOG_TWAI*` macros
- **Proven in Gauge.S** — blocking RX task, watchdog recovery, web status API

## Documentation

- **`AGENTS.md`** — the full technical reference: types, complete API,
  advanced config, logging, Gauge.S integration points. Start there whether
  you are a human or an AI agent.
- **Gauge.S project**: [sorek.uk](https://sorek.uk) · [shop.sorek.uk](https://shop.sorek.uk)
- Repository: [github.com/handmade0octopus/ESP32-TWAI-CAN](https://github.com/handmade0octopus/ESP32-TWAI-CAN.git)
- Example: `examples/OBD2-query/OBD2-query.ino`

## Dependencies

- ESP-IDF TWAI driver (ships with the Arduino-ESP32 core) — no external libraries
- External 3.3V CAN transceiver hardware (e.g. TI SN65HVD230)

## Author

**sorek** — [sorek.uk](https://sorek.uk) — contact@sorek.uk

## License

MIT — see [LICENSE](LICENSE). Copyright (c) sorek.uk.
