# Roadmap

## v0.1 — RTOS core

- [x] bounded `k_msgq` pipeline
- [x] producer/consumer threads
- [x] semaphore startup synchronization
- [x] mutex-protected telemetry
- [x] software heartbeat watchdog
- [x] deterministic producer-stall injection
- [x] queue-pressure metrics
- [x] ztest/Twister regression suite
- [x] normal + stall-injection `native_sim` builds
- [x] hosted PR and merged-main CI evidence

## v0.2 — Driver boundary

- [ ] abstract sensor acquisition interface
- [ ] simulated I2C/SPI-style transaction layer
- [ ] retry/error policy
- [ ] timestamp and sequence integrity checks
- [ ] deterministic bus-fault injection

## v0.3 — Telemetry transport

- [ ] bounded telemetry frame format
- [ ] UART-style transport adapter
- [ ] backpressure/drop accounting
- [ ] host-side decoder and replay fixture

## v0.4 — Hardware qualification

- [ ] select actual target board
- [ ] hardware watchdog peripheral
- [ ] physical sensor or loopback interface
- [ ] measured timing evidence
- [ ] fault/recovery evidence on hardware
