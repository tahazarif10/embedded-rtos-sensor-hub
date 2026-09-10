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

## v0.2 — Driver boundary and bus-fault recovery

Status: **complete**

Verified implementation:

- issue: [#2 — v0.2 driver boundary](https://github.com/tahazarif10/embedded-rtos-sensor-hub/issues/2)
- PR: [#3 — feat: add sensor driver boundary and bus fault recovery](https://github.com/tahazarif10/embedded-rtos-sensor-hub/pull/3)
- final PR head: `6339d60f9c82c6f1317fa2e98e51de5013ad85ab`
- PR CI: [#34493289155](https://github.com/tahazarif10/embedded-rtos-sensor-hub/actions/runs/34493289155) — **success**
- squash merge commit: `156903b57eb5525849094a76ca851349a9b0fc34`
- merged-main CI: [#34493673687](https://github.com/tahazarif10/embedded-rtos-sensor-hub/actions/runs/34493673687) — **success**

The merged-main hosted gates verified all of the following:

- normal `native_sim` application build
- deterministic producer-stall variant build
- deterministic transient bus-fault variant build
- ztest/Twister execution on `native_sim/native`
- **12/12 executed test cases passed (100%)**
- no Twister warnings in the selected configuration

### Driver contract verified by tests

- temperature acquisition maps to a simulated I2C-style device/register request
- vibration acquisition maps to a simulated SPI-style device/register request
- fixed 32-bit little-endian values are decoded into sensor samples
- two transient bus failures recover when the configured retry budget is two
- retry exhaustion returns the bus failure and increments failure accounting
- transfer attempts and retry attempts are counted independently
- sequence gaps are rejected and tracking resynchronizes for the next valid sample
- timestamp regressions are rejected without accepting the regressed timestamp as the new baseline
- invalid driver arguments and invalid sensor identifiers are rejected

### Runtime integration

The application producer threads now acquire samples through the driver boundary.
The deterministic bus-fault Kconfig variant injects transient vibration-bus read
failures, and telemetry exposes transfer, retry, and failed-read counters alongside
the existing hub metrics.

## Implementation evidence

The application uses real Zephyr kernel primitives:

- five statically defined threads: two producers, consumer, supervisor, telemetry
- bounded `k_msgq` producer/consumer pipeline
- `k_sem` startup synchronization
- `k_mutex` protected hub metrics
- separate `k_mutex` protected acquisition-driver state
- fixed-size sensor messages
- queue drop and high-watermark accounting
- software heartbeat watchdog logic
- deterministic vibration-producer stall injection
- deterministic simulated bus-fault injection
- bounded driver retry policy and integrity accounting

The application data path does not allocate from the heap.

## Evidence scope

The watchdog is a **software health supervisor**, not a hardware watchdog peripheral.
The I2C/SPI layer verifies simulated transaction contracts and software error
handling; it is not physical-bus validation. The evidence above does not establish
hard real-time guarantees, ISR latency, electrical behavior, physical sensor
accuracy, hardware watchdog behavior, board-level recovery, or safety certification.
