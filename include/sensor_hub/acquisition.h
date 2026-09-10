/*
 * Copyright (c) 2026 Taha Zarif
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SENSOR_HUB_ACQUISITION_H_
#define SENSOR_HUB_ACQUISITION_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>

#include <sensor_hub/sensor_hub.h>

#ifdef __cplusplus
extern "C" {
#endif

enum sensor_hub_bus_kind {
	SENSOR_HUB_BUS_I2C = 0,
	SENSOR_HUB_BUS_SPI,
};

struct sensor_hub_bus_request {
	enum sensor_hub_bus_kind bus;
	uint8_t device;
	uint8_t reg;
};

struct sensor_hub_bus_ops {
	int (*read)(void *context,
		    const struct sensor_hub_bus_request *request,
		    uint8_t *data,
		    size_t len);
};

struct sensor_hub_driver_stats {
	uint32_t transfer_attempts;
	uint32_t retry_attempts;
	uint32_t failed_reads;
	uint32_t sequence_errors;
	uint32_t timestamp_regressions;
};

struct sensor_hub_driver {
	struct k_mutex lock;
	struct sensor_hub_bus_ops ops;
	void *context;
	uint8_t max_retries;
	struct sensor_hub_driver_stats stats;
	uint32_t last_sequence[SENSOR_HUB_SENSOR_COUNT];
	uint32_t last_timestamp_ms[SENSOR_HUB_SENSOR_COUNT];
	bool seen[SENSOR_HUB_SENSOR_COUNT];
};

int sensor_hub_driver_init(struct sensor_hub_driver *driver,
			   const struct sensor_hub_bus_ops *ops,
			   void *context,
			   uint8_t max_retries);

int sensor_hub_driver_read(struct sensor_hub_driver *driver,
			   enum sensor_hub_sensor_id sensor,
			   uint32_t sequence,
			   uint32_t timestamp_ms,
			   struct sensor_hub_sample *sample);

void sensor_hub_driver_snapshot(struct sensor_hub_driver *driver,
				struct sensor_hub_driver_stats *snapshot);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_HUB_ACQUISITION_H_ */
