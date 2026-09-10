/*
 * Copyright (c) 2026 Taha Zarif
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <sensor_hub/acquisition.h>
#include <sensor_hub/sensor_hub.h>
#include <sensor_hub/sim_bus.h>

LOG_MODULE_REGISTER(sensor_hub, LOG_LEVEL_INF);

#define SAMPLE_QUEUE_CAPACITY 8
#define THREAD_STACK_SIZE 1024

K_MSGQ_DEFINE(sample_queue, sizeof(struct sensor_hub_sample),
	      SAMPLE_QUEUE_CAPACITY, 4);
K_SEM_DEFINE(start_sem, 0, 5);

static struct sensor_hub_state hub_state;
static struct sensor_hub_sim_bus temp_bus;
static struct sensor_hub_sim_bus vibration_bus;
static struct sensor_hub_driver temp_driver;
static struct sensor_hub_driver vibration_driver;

static const struct sensor_hub_bus_ops sim_bus_ops = {
	.read = sensor_hub_sim_bus_read,
};

static int32_t deterministic_value(enum sensor_hub_sensor_id id, uint32_t sequence)
{
	if (id == SENSOR_HUB_TEMP) {
		return 22000 + (int32_t)((sequence % 10U) * 25U);
	}

	return 1000 + (int32_t)((sequence % 5U) * 100U);
}

static void producer_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const enum sensor_hub_sensor_id id =
		(enum sensor_hub_sensor_id)(uintptr_t)p1;
	const k_timeout_t period =
		id == SENSOR_HUB_TEMP ? K_MSEC(50) : K_MSEC(80);
	struct sensor_hub_sim_bus *bus =
		id == SENSOR_HUB_TEMP ? &temp_bus : &vibration_bus;
	struct sensor_hub_driver *driver =
		id == SENSOR_HUB_TEMP ? &temp_driver : &vibration_driver;
	uint32_t sequence = 0U;
	bool bus_fault_injected = false;

	k_sem_take(&start_sem, K_FOREVER);

	while (true) {
		if (id == SENSOR_HUB_VIBRATION &&
		    IS_ENABLED(CONFIG_SENSOR_HUB_INJECT_STALL) &&
		    sequence >= CONFIG_SENSOR_HUB_STALL_AFTER_SAMPLES) {
			LOG_WRN("fault injection: vibration producer stalled at seq=%u",
				sequence);
			while (true) {
				k_sleep(K_SECONDS(1));
			}
		}

		if (id == SENSOR_HUB_VIBRATION &&
		    IS_ENABLED(CONFIG_SENSOR_HUB_INJECT_BUS_FAULT) &&
		    !bus_fault_injected &&
		    sequence >= CONFIG_SENSOR_HUB_BUS_FAULT_AFTER_SAMPLES) {
			sensor_hub_sim_bus_inject_failures(
				bus, CONFIG_SENSOR_HUB_BUS_FAULT_ATTEMPTS);
			bus_fault_injected = true;
			LOG_WRN("fault injection: vibration bus failures=%d at seq=%u",
				CONFIG_SENSOR_HUB_BUS_FAULT_ATTEMPTS, sequence);
		}

		sensor_hub_sim_bus_set_value(bus, id,
					 deterministic_value(id, sequence));

		struct sensor_hub_sample sample;
		const int rc = sensor_hub_driver_read(
			driver, id, sequence, k_uptime_get_32(), &sample);

		if (rc != 0) {
			LOG_WRN("acquisition failed: sensor=%u seq=%u rc=%d",
				(unsigned int)id, sequence, rc);
			k_sleep(period);
			continue;
		}

		if (k_msgq_put(&sample_queue, &sample, K_NO_WAIT) == 0) {
			sensor_hub_record_enqueue(
				&hub_state, k_msgq_num_used_get(&sample_queue));
		} else {
			sensor_hub_record_drop(&hub_state);
		}

		sequence++;
		k_sleep(period);
	}
}

static void consumer_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct sensor_hub_sample sample;

	k_sem_take(&start_sem, K_FOREVER);

	while (true) {
		k_msgq_get(&sample_queue, &sample, K_FOREVER);
		(void)sensor_hub_consume(&hub_state, &sample);
	}
}

static void supervisor_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	bool previous_stale[SENSOR_HUB_SENSOR_COUNT] = {false};

	k_sem_take(&start_sem, K_FOREVER);

	while (true) {
		struct sensor_hub_metrics metrics;

		sensor_hub_supervise(&hub_state, k_uptime_get_32(),
				     CONFIG_SENSOR_HUB_STALE_AFTER_MS);
		sensor_hub_snapshot(&hub_state, &metrics);

		for (size_t i = 0; i < SENSOR_HUB_SENSOR_COUNT; ++i) {
			if (metrics.stale[i] && !previous_stale[i]) {
				LOG_WRN("software watchdog: sensor=%u stale",
					(unsigned int)i);
			}
			previous_stale[i] = metrics.stale[i];
		}

		k_sleep(K_MSEC(100));
	}
}

static void telemetry_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	k_sem_take(&start_sem, K_FOREVER);

	while (true) {
		struct sensor_hub_metrics metrics;
		struct sensor_hub_driver_stats temp_stats;
		struct sensor_hub_driver_stats vibration_stats;

		sensor_hub_snapshot(&hub_state, &metrics);
		sensor_hub_driver_snapshot(&temp_driver, &temp_stats);
		sensor_hub_driver_snapshot(&vibration_driver, &vibration_stats);

		LOG_INF("accepted=%u dropped=%u stale_events=%u q_high=%u temp=%d vib=%d",
			metrics.accepted_samples,
			metrics.dropped_samples,
			metrics.stale_events,
			metrics.queue_high_watermark,
			metrics.last_value_milli[SENSOR_HUB_TEMP],
			metrics.last_value_milli[SENSOR_HUB_VIBRATION]);
		LOG_INF("bus temp attempts=%u retries=%u fail=%u vib attempts=%u retries=%u fail=%u",
			temp_stats.transfer_attempts,
			temp_stats.retry_attempts,
			temp_stats.failed_reads,
			vibration_stats.transfer_attempts,
			vibration_stats.retry_attempts,
			vibration_stats.failed_reads);
		k_sleep(K_MSEC(500));
	}
}

K_THREAD_DEFINE(temp_producer, THREAD_STACK_SIZE, producer_thread,
		(void *)(uintptr_t)SENSOR_HUB_TEMP, NULL, NULL,
		4, 0, 0);
K_THREAD_DEFINE(vibration_producer, THREAD_STACK_SIZE, producer_thread,
		(void *)(uintptr_t)SENSOR_HUB_VIBRATION, NULL, NULL,
		4, 0, 0);
K_THREAD_DEFINE(consumer, THREAD_STACK_SIZE, consumer_thread,
		NULL, NULL, NULL, 3, 0, 0);
K_THREAD_DEFINE(supervisor, THREAD_STACK_SIZE, supervisor_thread,
		NULL, NULL, NULL, 2, 0, 0);
K_THREAD_DEFINE(telemetry, THREAD_STACK_SIZE, telemetry_thread,
		NULL, NULL, NULL, 5, 0, 0);

int main(void)
{
	sensor_hub_init(&hub_state);
	sensor_hub_sim_bus_init(&temp_bus);
	sensor_hub_sim_bus_init(&vibration_bus);

	int rc = sensor_hub_driver_init(
		&temp_driver, &sim_bus_ops, &temp_bus,
		CONFIG_SENSOR_HUB_DRIVER_MAX_RETRIES);
	if (rc != 0) {
		LOG_ERR("temperature driver init failed: rc=%d", rc);
		return rc;
	}

	rc = sensor_hub_driver_init(
		&vibration_driver, &sim_bus_ops, &vibration_bus,
		CONFIG_SENSOR_HUB_DRIVER_MAX_RETRIES);
	if (rc != 0) {
		LOG_ERR("vibration driver init failed: rc=%d", rc);
		return rc;
	}

	for (int i = 0; i < 5; ++i) {
		k_sem_give(&start_sem);
	}

	LOG_INF("sensor hub started: queue=%u stale_timeout_ms=%d stall_injection=%d bus_fault=%d",
		SAMPLE_QUEUE_CAPACITY,
		CONFIG_SENSOR_HUB_STALE_AFTER_MS,
		IS_ENABLED(CONFIG_SENSOR_HUB_INJECT_STALL),
		IS_ENABLED(CONFIG_SENSOR_HUB_INJECT_BUS_FAULT));

	return 0;
}
