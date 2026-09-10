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

- [x] abstract sensor acquisition interface
- [x] simulated I2C/SPI-style transaction layer
- [x] bounded retry/error policy with metrics
- [x] timestamp and sequence integrity checks
- [x] deterministic bus-fault injection
- [x] normal + stall + bus-fault `native_sim` builds
- [x] expanded ztest/Twister regression suite
- [x] hosted PR and merged-main CI evidence

## v0.3 — Telemetry transport

- [ ] bounded versioned telemetry frame format
- [ ] UART-style transport adapter
- [ ] backpressure/drop accounting
- [ ] deterministic queue-pressure injection
- [ ] host-side decoder and replay fixture
- [ ] encode/decode/integrity tests

## v0.4 — Hardware qualification

- [ ] select actual target board
- [ ] hardware watchdog peripheral
- [ ] physical sensor or loopback interface
- [ ] measured timing evidence
- [ ] fault/recovery evidence on hardware
