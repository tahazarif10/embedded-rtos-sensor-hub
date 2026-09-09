/*
 * Copyright (c) 2026 Taha Zarif
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <sensor_hub/sensor_hub.h>

void sensor_hub_init(struct sensor_hub_state *state)
{
	memset(state, 0, sizeof(*state));
	k_mutex_init(&state->lock);
}

void sensor_hub_record_enqueue(struct sensor_hub_state *state, uint32_t queue_depth)
{
	k_mutex_lock(&state->lock, K_FOREVER);
	if (queue_depth > state->metrics.queue_high_watermark) {
		state->metrics.queue_high_watermark = queue_depth;
	}
	k_mutex_unlock(&state->lock);
}

void sensor_hub_record_drop(struct sensor_hub_state *state)
{
	k_mutex_lock(&state->lock, K_FOREVER);
	state->metrics.dropped_samples++;
	k_mutex_unlock(&state->lock);
}

int sensor_hub_consume(struct sensor_hub_state *state,
		       const struct sensor_hub_sample *sample)
{
	if (sample == NULL || sample->sensor_id >= SENSOR_HUB_SENSOR_COUNT) {
		return -EINVAL;
	}

	k_mutex_lock(&state->lock, K_FOREVER);
	state->metrics.accepted_samples++;
	state->metrics.last_sequence[sample->sensor_id] = sample->sequence;
	state->metrics.last_seen_ms[sample->sensor_id] = sample->timestamp_ms;
	state->metrics.last_value_milli[sample->sensor_id] = sample->value_milli;
	state->metrics.seen[sample->sensor_id] = true;
	state->metrics.stale[sample->sensor_id] = false;
	k_mutex_unlock(&state->lock);

	return 0;
}

void sensor_hub_supervise(struct sensor_hub_state *state,
			  uint32_t now_ms,
			  uint32_t stale_after_ms)
{
	k_mutex_lock(&state->lock, K_FOREVER);

	for (size_t i = 0; i < SENSOR_HUB_SENSOR_COUNT; ++i) {
		if (!state->metrics.seen[i]) {
			continue;
		}

		const uint32_t age_ms = now_ms - state->metrics.last_seen_ms[i];
		const bool is_stale = age_ms > stale_after_ms;

		if (is_stale && !state->metrics.stale[i]) {
			state->metrics.stale_events++;
		}
		state->metrics.stale[i] = is_stale;
	}

	k_mutex_unlock(&state->lock);
}

void sensor_hub_snapshot(struct sensor_hub_state *state,
			 struct sensor_hub_metrics *snapshot)
{
	k_mutex_lock(&state->lock, K_FOREVER);
	*snapshot = state->metrics;
	k_mutex_unlock(&state->lock);
}
