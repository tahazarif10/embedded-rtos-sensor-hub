# Architecture

## Design intent

The project demonstrates an embedded data path with explicit bounded resources,
RTOS synchronization, testable acquisition boundaries, and deterministic fault
handling.

## Runtime components

| Component | Responsibility | RTOS / software contract |
| --- | --- | --- |
| Temperature producer | Acquire deterministic temperature samples | thread + semaphore + driver boundary |
| Vibration producer | Acquire vibration samples / inject stall or bus fault | thread + semaphore + driver boundary |
| Acquisition driver | Serialize access, retry transient reads, validate sample integrity | `k_mutex` + bounded retry policy |
| Simulated temperature bus | Model an I2C-style device/register transaction | bus abstraction, device `0x48`, register `0x00` |
| Simulated vibration bus | Model an SPI-style device/register transaction | bus abstraction, device `0x01`, register `0x10` |
| Sample pipeline | Bound producer/consumer buffering | `k_msgq` |
| Consumer | Update shared health/telemetry state | thread |
| Shared state | Protect metrics and last-seen data | `k_mutex` |
| Supervisor | Detect stale producer heartbeats | thread + software watchdog logic |
| Telemetry | Emit health and acquisition metrics | thread |

## Acquisition boundary

Producer threads no longer construct samples directly from generated values.
They call `sensor_hub_driver_read()`, which owns the acquisition contract:

1. map a sensor ID to an explicit simulated bus request
2. serialize the driver transaction with a mutex
3. perform one transfer plus a configured bounded number of retries
4. count transfer attempts, retries, and exhausted reads
5. validate per-sensor sequence continuity
6. reject timestamp regressions
7. decode the fixed 32-bit little-endian sample value
8. emit a validated fixed-size `sensor_hub_sample`

The sequence checker deliberately resynchronizes after a detected gap so one bad
sample does not generate an unbounded cascade of later sequence errors. A
regressed timestamp is rejected without replacing the last accepted timestamp.

## Failure model

The verified v0.2 failure model covers:

- queue saturation: sample is dropped rather than blocking a producer indefinitely
- invalid sensor identifiers: rejected by the core/driver boundary
- producer stall: deterministic vibration producer can stop after N samples
- stale heartbeat: supervisor transitions the sensor to stale and counts the transition
- recovery: a fresh sample clears the stale state
- transient bus failures: bounded retries recover when the simulated bus becomes healthy
- exhausted bus retries: acquisition fails and increments `failed_reads`
- sequence gaps: rejected with integrity accounting, then tracking resynchronizes
- timestamp regression: rejected without accepting the regressed time baseline

## Memory behavior

The application data path uses statically allocated Zephyr kernel objects,
fixed-size messages, and fixed driver state. No heap allocation is performed by
the sensor-hub application code.

## Concurrency

Each sensor driver owns a `k_mutex`, so bus transactions and driver statistics are
serialized even if future producers share a driver instance. Hub metrics use a
separate mutex, keeping acquisition synchronization independent from consumer and
telemetry state synchronization.

## Scope

The I2C/SPI transaction layer is a **simulated driver contract**, not a physical
peripheral implementation. The watchdog is a **software health supervisor**, not
a configured hardware watchdog peripheral. ISR-driven acquisition, DMA, physical
buses, hardware timing, and board-level fault recovery remain later qualification
work.
