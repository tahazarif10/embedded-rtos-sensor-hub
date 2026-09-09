/*
 * Copyright (c) 2026 Taha Zarif
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SENSOR_HUB_SENSOR_HUB_H_
#define SENSOR_HUB_SENSOR_HUB_H_

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

enum sensor_hub_sensor_id {
	SENSOR_HUB_TEMP = 0,
	SENSOR_HUB_VIBRATION,
	SENSOR_HUB_SENSOR_COUNT,
};

struct sensor_hub_sample {
	uint32_t sequence;
	uint32_t timestamp_ms;
	int32_t value_milli;
	uint8_t sensor_id;
};

struct sensor_hub_metrics {
	uint32_t accepted_samples;
	uint32_t dropped_samples;
	uint32_t stale_events;
	uint32_t queue_high_watermark;
	uint32_t last_sequence[SENSOR_HUB_SENSOR_COUNT];
	uint32_t last_seen_ms[SENSOR_HUB_SENSOR_COUNT];
	int32_t last_value_milli[SENSOR_HUB_SENSOR_COUNT];
	bool seen[SENSOR_HUB_SENSOR_COUNT];
	bool stale[SENSOR_HUB_SENSOR_COUNT];
};

struct sensor_hub_state {
	struct k_mutex lock;
	struct sensor_hub_metrics metrics;
};

void sensor_hub_init(struct sensor_hub_state *state);
void sensor_hub_record_enqueue(struct sensor_hub_state *state, uint32_t queue_depth);
void sensor_hub_record_drop(struct sensor_hub_state *state);
int sensor_hub_consume(struct sensor_hub_state *state,
		       const struct sensor_hub_sample *sample);
void sensor_hub_supervise(struct sensor_hub_state *state,
			  uint32_t now_ms,
			  uint32_t stale_after_ms);
void sensor_hub_snapshot(struct sensor_hub_state *state,
			 struct sensor_hub_metrics *snapshot);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_HUB_SENSOR_HUB_H_ */
