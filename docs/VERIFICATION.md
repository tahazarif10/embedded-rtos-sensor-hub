# Verification

This file records observed verification only.

## v0.1 — Zephyr RTOS core

Status: **complete**

Baseline:

- Zephyr **v4.4.2**, pinned by `west.yml`
- Ubuntu 24.04 GitHub-hosted runner
- `native_sim/native`
- host GNU toolchain
- ztest/Twister

Verified implementation:

- PR: [#1 — feat: add Zephyr RTOS sensor hub core](https://github.com/tahazarif10/embedded-rtos-sensor-hub/pull/1)
- final PR head: `e074a7e940736922cb79ba163b69a3391ba2ea65`
- PR CI: [#34406892562](https://github.com/tahazarif10/embedded-rtos-sensor-hub/actions/runs/34406892562) — **success**
- squash merge commit: `efcb16c30a110c5bb8644c36e193bb856493e60d`
- merged-main CI: [#34407100161](https://github.com/tahazarif10/embedded-rtos-sensor-hub/actions/runs/34407100161) — **success**

The hosted gates verified:

- normal Zephyr application build on `native_sim`
- deterministic stall-injection variant build
- ztest/Twister execution on `native_sim/native`
- **5/5 sensor-hub test cases passed**
- bounded queue-pressure behavior
- invalid sensor rejection
- sample/telemetry state updates
- stale transition detection
- stale-event de-duplication
- recovery after a fresh sample and a subsequent second stale transition

## v0.1 implementation evidence

The application uses real Zephyr kernel primitives:

- five statically defined threads: two producers, consumer, supervisor, telemetry
- bounded `k_msgq` producer/consumer pipeline
- `k_sem` startup synchronization
- `k_mutex` protected shared metrics
- fixed-size sensor messages
- queue drop and high-watermark accounting
- software heartbeat watchdog logic
- deterministic vibration-producer stall injection

The application data path does not allocate from the heap.

## Evidence scope

The watchdog is a **software health supervisor**, not a hardware watchdog peripheral.
The evidence above is software/native-simulation verification. It does not establish
hard real-time guarantees, ISR latency, physical sensor accuracy, hardware watchdog
behavior, physical bus behavior, or safety certification.
