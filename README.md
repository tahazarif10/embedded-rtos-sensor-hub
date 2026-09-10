# Embedded RTOS Sensor Hub

[![CI](https://github.com/tahazarif10/embedded-rtos-sensor-hub/actions/workflows/ci.yml/badge.svg)](https://github.com/tahazarif10/embedded-rtos-sensor-hub/actions/workflows/ci.yml)
[![Zephyr](https://img.shields.io/badge/Zephyr-4.4.2-6b4eff)](https://github.com/zephyrproject-rtos/zephyr/releases/tag/v4.4.2)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

An evidence-driven **Zephyr RTOS** project that models a deterministic embedded
sensor hub using native simulation before any hardware claim is made.

## Current status

**v0.2 driver boundary and bus-fault recovery — complete**

The project now combines the v0.1 bounded RTOS pipeline with a testable sensor
acquisition boundary. Hosted CI verifies normal, producer-stall, and transient
bus-fault builds plus the complete ztest/Twister suite on `native_sim/native`.

Verified engineering scope:

- five statically defined Zephyr threads
- bounded `k_msgq` sample pipeline
- startup synchronization with `k_sem`
- shared metrics protected by `k_mutex`
- software heartbeat watchdog for stale-sensor detection
- explicit queue-drop and queue high-watermark accounting
- abstract bus read interface between producer threads and sensor data
- simulated **I2C-style** temperature transaction contract
- simulated **SPI-style** vibration transaction contract
- bounded retry policy with transfer/retry/failure metrics
- per-sensor sequence continuity validation with resynchronization
- timestamp-regression rejection without poisoning the later valid baseline
- deterministic producer-stall and transient bus-fault injection
- **12/12 checked-in ztest cases passing**
- GitHub Actions verification on the PR and merged `main`

The application data path uses fixed-size messages and bounded kernel objects; it
does not allocate from the heap.

## Architecture

```text
                           simulated bus boundary
                         ┌──────────────────────────┐
temperature producer ───>│ I2C-style read + retry  │─┐
vibration producer ─────>│ SPI-style read + retry  │─┼─> bounded k_msgq
                         └──────────────────────────┘ │
                                                     v
                                                  consumer
                                                     │
                              ┌──────────────────────┴───────────────────┐
                              v                                          v
                    protected telemetry                       heartbeat supervisor
                              │
                              v
                      telemetry reporter
```

The I2C/SPI layer is deliberately simulated and testable. It establishes driver
contracts and error handling without claiming physical electrical, timing, or
peripheral behavior.

The watchdog remains a **software health supervisor**; a hardware watchdog
peripheral is not claimed.

## Build

The repository is a west manifest repository pinned to Zephyr **v4.4.2**.

```bash
python -m pip install west
west init -l .
west update
west zephyr-export
west build -b native_sim -s . -d build/app
```

Run the regression suite:

```bash
west twister -T tests -p native_sim --inline-logs
```

## Fault injection

Producer-stall build:

```bash
west build -b native_sim -s . -d build/stall -- \
  -DCONFIG_SENSOR_HUB_INJECT_STALL=y
```

Transient bus-fault build:

```bash
west build -b native_sim -s . -d build/bus-fault -- \
  -DCONFIG_SENSOR_HUB_INJECT_BUS_FAULT=y \
  -DCONFIG_SENSOR_HUB_BUS_FAULT_ATTEMPTS=2
```

The bus-fault fixture injects bounded read failures into the simulated vibration
bus so retry and recovery behavior is exercised deterministically.

## Hosted evidence

### v0.1

- [PR #1](https://github.com/tahazarif10/embedded-rtos-sensor-hub/pull/1)
- [PR CI #34406892562](https://github.com/tahazarif10/embedded-rtos-sensor-hub/actions/runs/34406892562) — success
- merge commit `efcb16c30a110c5bb8644c36e193bb856493e60d`
- [merged-main CI #34407100161](https://github.com/tahazarif10/embedded-rtos-sensor-hub/actions/runs/34407100161) — success

### v0.2

- [PR #3](https://github.com/tahazarif10/embedded-rtos-sensor-hub/pull/3)
- [PR CI #34493289155](https://github.com/tahazarif10/embedded-rtos-sensor-hub/actions/runs/34493289155) — success
- merge commit `156903b57eb5525849094a76ca851349a9b0fc34`
- [merged-main CI #34493673687](https://github.com/tahazarif10/embedded-rtos-sensor-hub/actions/runs/34493673687) — success
- Twister: **12/12 executed test cases passed** on `native_sim/native`

## Evidence policy

All timing and behavior claims in this repository are scoped to the checked-in
Zephyr/native-simulation configuration. Hardware latency, interrupt response,
physical sensor accuracy, hardware watchdog behavior, physical I2C/SPI behavior,
and safety certification remain out of scope until measured on actual hardware.

See [Architecture](docs/ARCHITECTURE.md), [Verification](docs/VERIFICATION.md), and [Roadmap](docs/ROADMAP.md).
