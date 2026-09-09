/*
 * Copyright (c) 2026 Taha Zarif
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <sensor_hub/sensor_hub.h>

LOG_MODULE_REGISTER(sensor_hub, LOG_LEVEL_INF);

#define SAMPLE_QUEUE_CAPACITY 8
#define THREAD_STACK_SIZE 1024

K_MSGQ_DEFINE(sample_queue, sizeof(struct sensor_hub_sample),
	      SAMPLE_QUEUE_CAPACITY, 4);
K_SEM_DEFINE(start_sem, 0, 5);

static struct sensor_hub_state hub_state;

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
	uint32_t sequence = 0U;

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

		struct sensor_hub_sample sample = {
			.sequence = sequence,
			.timestamp_ms = k_uptime_get_32(),
			.value_milli = deterministic_value(id, sequence),
			.sensor_id = (uint8_t)id,
		};

		const int rc = k_msgq_put(&sample_queue, &sample, K_NO_WAIT);
		if (rc == 0) {
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

		sensor_hub_snapshot(&hub_state, &metrics);
		LOG_INF("accepted=%u dropped=%u stale_events=%u q_high=%u temp=%d vib=%d",
			metrics.accepted_samples,
			metrics.dropped_samples,
			metrics.stale_events,
			metrics.queue_high_watermark,
			metrics.last_value_milli[SENSOR_HUB_TEMP],
			metrics.last_value_milli[SENSOR_HUB_VIBRATION]);
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

	for (int i = 0; i < 5; ++i) {
		k_sem_give(&start_sem);
	}

	LOG_INF("sensor hub started: queue=%u stale_timeout_ms=%d stall_injection=%d",
		SAMPLE_QUEUE_CAPACITY,
		CONFIG_SENSOR_HUB_STALE_AFTER_MS,
		IS_ENABLED(CONFIG_SENSOR_HUB_INJECT_STALL));

	return 0;
}
