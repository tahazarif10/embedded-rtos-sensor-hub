# Embedded RTOS Sensor Hub

[![CI](https://github.com/tahazarif10/embedded-rtos-sensor-hub/actions/workflows/ci.yml/badge.svg)](https://github.com/tahazarif10/embedded-rtos-sensor-hub/actions/workflows/ci.yml)
[![Zephyr](https://img.shields.io/badge/Zephyr-4.4.2-6b4eff)](https://github.com/zephyrproject-rtos/zephyr/releases/tag/v4.4.2)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

A small, evidence-driven **Zephyr RTOS** project that models a deterministic embedded sensor hub using native simulation before any hardware claim is made.

## v0.1 scope

The first milestone exercises real RTOS primitives and failure handling:

- two deterministic sensor-producer threads
- a bounded `k_msgq` sample pipeline
- startup synchronization with `k_sem`
- shared telemetry protected by `k_mutex`
- a software heartbeat watchdog for stale-sensor detection
- explicit queue-drop and queue high-watermark metrics
- deterministic vibration-sensor stall injection
- ztest/Twister regression tests on `native_sim`
- GitHub Actions build + test verification

The project intentionally uses fixed-size messages and bounded queues. It does not use heap allocation in the application data path.

## Architecture

```text
temperature producer ─┐
                      ├─> bounded k_msgq ─> consumer ─> protected telemetry
vibration producer ───┘                         │
                                               ├─> software watchdog supervisor
                                               └─> periodic telemetry reporter
```

The software watchdog is a health supervisor based on producer heartbeats. It is **not** a claim that a hardware watchdog peripheral is configured.

## Build

The repository is a west manifest repository pinned to Zephyr **v4.4.2**.

```bash
python -m pip install west
west init -l .
west update
west zephyr-export

west build -b native_sim -s app -d build/app
```

Run the unit/regression suite:

```bash
west twister -T tests -p native_sim --inline-logs
```

## Fault injection

Enable the deterministic vibration-producer stall:

```bash
west build -b native_sim -s app -d build/stall -- \
  -DCONFIG_SENSOR_HUB_INJECT_STALL=y
```

After the configured number of vibration samples, that producer stops publishing. The supervisor marks it stale after the configured timeout and increments a stale-event counter.

## Evidence policy

All timing and behavior claims in this repository are scoped to the checked-in Zephyr/native-simulation configuration. Hardware latency, interrupt response, physical sensor accuracy, and safety certification are out of scope until measured on actual hardware.

See [Architecture](docs/ARCHITECTURE.md), [Verification](docs/VERIFICATION.md), and [Roadmap](docs/ROADMAP.md).
