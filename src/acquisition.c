/*
 * Copyright (c) 2026 Taha Zarif
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <sensor_hub/acquisition.h>

#define SENSOR_HUB_VALUE_BYTES 4U

static int request_for_sensor(enum sensor_hub_sensor_id sensor,
			      struct sensor_hub_bus_request *request)
{
	switch (sensor) {
	case SENSOR_HUB_TEMP:
		*request = (struct sensor_hub_bus_request) {
			.bus = SENSOR_HUB_BUS_I2C,
			.device = 0x48U,
			.reg = 0x00U,
		};
		return 0;
	case SENSOR_HUB_VIBRATION:
		*request = (struct sensor_hub_bus_request) {
			.bus = SENSOR_HUB_BUS_SPI,
			.device = 0x01U,
			.reg = 0x10U,
		};
		return 0;
	default:
		return -EINVAL;
	}
}

static int32_t decode_le32(const uint8_t data[SENSOR_HUB_VALUE_BYTES])
{
	const uint32_t value =
		((uint32_t)data[0]) |
		((uint32_t)data[1] << 8) |
		((uint32_t)data[2] << 16) |
		((uint32_t)data[3] << 24);

	return (int32_t)value;
}

int sensor_hub_driver_init(struct sensor_hub_driver *driver,
			   const struct sensor_hub_bus_ops *ops,
			   void *context,
			   uint8_t max_retries)
{
	if (driver == NULL || ops == NULL || ops->read == NULL) {
		return -EINVAL;
	}

	memset(driver, 0, sizeof(*driver));
	k_mutex_init(&driver->lock);
	driver->ops = *ops;
	driver->context = context;
	driver->max_retries = max_retries;

	return 0;
}

int sensor_hub_driver_read(struct sensor_hub_driver *driver,
			   enum sensor_hub_sensor_id sensor,
			   uint32_t sequence,
			   uint32_t timestamp_ms,
			   struct sensor_hub_sample *sample)
{
	struct sensor_hub_bus_request request;
	uint8_t data[SENSOR_HUB_VALUE_BYTES];
	int rc;

	if (driver == NULL || sample == NULL ||
	    sensor >= SENSOR_HUB_SENSOR_COUNT) {
		return -EINVAL;
	}

	rc = request_for_sensor(sensor, &request);
	if (rc != 0) {
		return rc;
	}

	k_mutex_lock(&driver->lock, K_FOREVER);

	for (uint32_t attempt = 0U; attempt <= driver->max_retries; ++attempt) {
		driver->stats.transfer_attempts++;
		if (attempt > 0U) {
			driver->stats.retry_attempts++;
		}

		rc = driver->ops.read(driver->context, &request,
				      data, sizeof(data));
		if (rc == 0) {
			break;
		}
	}

	if (rc != 0) {
		driver->stats.failed_reads++;
		k_mutex_unlock(&driver->lock);
		return rc;
	}

	bool sequence_error = false;
	bool timestamp_error = false;

	if (driver->seen[sensor]) {
		const uint32_t expected = driver->last_sequence[sensor] + 1U;

		if (sequence != expected) {
			driver->stats.sequence_errors++;
			sequence_error = true;
		}

		if (timestamp_ms < driver->last_timestamp_ms[sensor]) {
			driver->stats.timestamp_regressions++;
			timestamp_error = true;
		}
	}

	/* Resynchronize sequence tracking after an anomaly so one bad sample does
	 * not create an unbounded cascade of sequence errors.
	 */
	driver->last_sequence[sensor] = sequence;
	if (!driver->seen[sensor] || !timestamp_error) {
		driver->last_timestamp_ms[sensor] = timestamp_ms;
	}
	driver->seen[sensor] = true;

	if (sequence_error) {
		k_mutex_unlock(&driver->lock);
		return -EILSEQ;
	}

	if (timestamp_error) {
		k_mutex_unlock(&driver->lock);
		return -ERANGE;
	}

	*sample = (struct sensor_hub_sample) {
		.sequence = sequence,
		.timestamp_ms = timestamp_ms,
		.value_milli = decode_le32(data),
		.sensor_id = (uint8_t)sensor,
	};

	k_mutex_unlock(&driver->lock);
	return 0;
}

void sensor_hub_driver_snapshot(struct sensor_hub_driver *driver,
				struct sensor_hub_driver_stats *snapshot)
{
	if (driver == NULL || snapshot == NULL) {
		return;
	}

	k_mutex_lock(&driver->lock, K_FOREVER);
	*snapshot = driver->stats;
	k_mutex_unlock(&driver->lock);
}
