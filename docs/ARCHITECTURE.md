# Architecture

## Design intent

The project demonstrates an embedded data path with explicit bounded resources and RTOS synchronization.

## Runtime components

| Component | Responsibility | RTOS primitive |
| --- | --- | --- |
| Temperature producer | Generate deterministic temperature samples | thread + semaphore |
| Vibration producer | Generate deterministic vibration samples / optional stall | thread + semaphore |
| Sample pipeline | Bound producer/consumer buffering | `k_msgq` |
| Consumer | Update shared health/telemetry state | thread |
| Shared state | Protect metrics and last-seen data | `k_mutex` |
| Supervisor | Detect stale producer heartbeats | thread + software watchdog logic |
| Telemetry | Emit bounded health metrics | thread |

## Failure model

The v0.1 failure model covers:

- queue saturation: sample is dropped rather than blocking a producer indefinitely
- invalid sensor identifiers: rejected by the core
- producer stall: deterministic vibration producer can stop after N samples
- stale heartbeat: supervisor transitions the sensor to stale and counts the transition
- recovery: a fresh sample clears the stale state

## Memory behavior

The application data path uses statically allocated Zephyr kernel objects and fixed-size messages. No heap allocation is performed by the sensor-hub application code.

## Scope

The watchdog in v0.1 is a **software health supervisor**. A hardware watchdog peripheral, ISR-driven physical sensor acquisition, DMA, and real device buses are later milestones.
