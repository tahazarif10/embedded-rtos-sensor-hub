/*
 * Copyright (c) 2026 Taha Zarif
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <sensor_hub/sensor_hub.h>

K_MSGQ_DEFINE(test_queue, sizeof(struct sensor_hub_sample), 2, 4);

static struct sensor_hub_state state;

static void sensor_hub_before(void *fixture)
{
	ARG_UNUSED(fixture);
	sensor_hub_init(&state);
	k_msgq_purge(&test_queue);
}

ZTEST(sensor_hub, test_initial_state_is_bounded_and_empty)
{
	struct sensor_hub_metrics snapshot;

	sensor_hub_snapshot(&state, &snapshot);

	zassert_equal(snapshot.accepted_samples, 0U);
	zassert_equal(snapshot.dropped_samples, 0U);
	zassert_equal(snapshot.stale_events, 0U);
	zassert_equal(snapshot.queue_high_watermark, 0U);
}

ZTEST(sensor_hub, test_sample_updates_metrics)
{
	const struct sensor_hub_sample sample = {
		.sequence = 7U,
		.timestamp_ms = 100U,
		.value_milli = 22350,
		.sensor_id = SENSOR_HUB_TEMP,
	};
	struct sensor_hub_metrics snapshot;

	zassert_ok(sensor_hub_consume(&state, &sample));
	sensor_hub_snapshot(&state, &snapshot);

	zassert_equal(snapshot.accepted_samples, 1U);
	zassert_true(snapshot.seen[SENSOR_HUB_TEMP]);
	zassert_false(snapshot.stale[SENSOR_HUB_TEMP]);
	zassert_equal(snapshot.last_sequence[SENSOR_HUB_TEMP], 7U);
	zassert_equal(snapshot.last_seen_ms[SENSOR_HUB_TEMP], 100U);
	zassert_equal(snapshot.last_value_milli[SENSOR_HUB_TEMP], 22350);
}

ZTEST(sensor_hub, test_invalid_sensor_is_rejected)
{
	const struct sensor_hub_sample sample = {
		.sensor_id = SENSOR_HUB_SENSOR_COUNT,
	};

	zassert_equal(sensor_hub_consume(&state, &sample), -EINVAL);
	zassert_equal(sensor_hub_consume(&state, NULL), -EINVAL);
}

ZTEST(sensor_hub, test_software_watchdog_counts_stale_transitions)
{
	struct sensor_hub_sample sample = {
		.sequence = 1U,
		.timestamp_ms = 100U,
		.value_milli = 1200,
		.sensor_id = SENSOR_HUB_VIBRATION,
	};
	struct sensor_hub_metrics snapshot;

	zassert_ok(sensor_hub_consume(&state, &sample));

	sensor_hub_supervise(&state, 351U, 250U);
	sensor_hub_snapshot(&state, &snapshot);
	zassert_true(snapshot.stale[SENSOR_HUB_VIBRATION]);
	zassert_equal(snapshot.stale_events, 1U);

	sensor_hub_supervise(&state, 500U, 250U);
	sensor_hub_snapshot(&state, &snapshot);
	zassert_equal(snapshot.stale_events, 1U);

	sample.sequence = 2U;
	sample.timestamp_ms = 500U;
	zassert_ok(sensor_hub_consume(&state, &sample));
	sensor_hub_snapshot(&state, &snapshot);
	zassert_false(snapshot.stale[SENSOR_HUB_VIBRATION]);

	sensor_hub_supervise(&state, 751U, 250U);
	sensor_hub_snapshot(&state, &snapshot);
	zassert_true(snapshot.stale[SENSOR_HUB_VIBRATION]);
	zassert_equal(snapshot.stale_events, 2U);
}

ZTEST(sensor_hub, test_bounded_queue_pressure_is_observable)
{
	const struct sensor_hub_sample sample = {
		.sensor_id = SENSOR_HUB_TEMP,
	};
	struct sensor_hub_metrics snapshot;

	zassert_ok(k_msgq_put(&test_queue, &sample, K_NO_WAIT));
	sensor_hub_record_enqueue(&state, k_msgq_num_used_get(&test_queue));
	zassert_ok(k_msgq_put(&test_queue, &sample, K_NO_WAIT));
	sensor_hub_record_enqueue(&state, k_msgq_num_used_get(&test_queue));

	const int rc = k_msgq_put(&test_queue, &sample, K_NO_WAIT);
	zassert_not_equal(rc, 0);
	sensor_hub_record_drop(&state);

	sensor_hub_snapshot(&state, &snapshot);
	zassert_equal(snapshot.queue_high_watermark, 2U);
	zassert_equal(snapshot.dropped_samples, 1U);
}

ZTEST_SUITE(sensor_hub, NULL, NULL, sensor_hub_before, NULL, NULL);
