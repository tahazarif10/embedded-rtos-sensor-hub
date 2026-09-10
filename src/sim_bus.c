/*
 * Copyright (c) 2026 Taha Zarif
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <sensor_hub/sim_bus.h>

void sensor_hub_sim_bus_init(struct sensor_hub_sim_bus *bus)
{
	memset(bus, 0, sizeof(*bus));
}

void sensor_hub_sim_bus_set_value(struct sensor_hub_sim_bus *bus,
				  enum sensor_hub_sensor_id sensor,
				  int32_t value_milli)
{
	if (bus == NULL || sensor >= SENSOR_HUB_SENSOR_COUNT) {
		return;
	}

	bus->value_milli[sensor] = value_milli;
}

void sensor_hub_sim_bus_inject_failures(struct sensor_hub_sim_bus *bus,
					uint32_t failures)
{
	if (bus != NULL) {
		bus->failures_remaining = failures;
	}
}

static int sensor_from_request(const struct sensor_hub_bus_request *request)
{
	if (request->bus == SENSOR_HUB_BUS_I2C &&
	    request->device == 0x48U && request->reg == 0x00U) {
		return SENSOR_HUB_TEMP;
	}

	if (request->bus == SENSOR_HUB_BUS_SPI &&
	    request->device == 0x01U && request->reg == 0x10U) {
		return SENSOR_HUB_VIBRATION;
	}

	return -ENODEV;
}

int sensor_hub_sim_bus_read(void *context,
			    const struct sensor_hub_bus_request *request,
			    uint8_t *data,
			    size_t len)
{
	struct sensor_hub_sim_bus *bus = context;

	if (bus == NULL || request == NULL || data == NULL || len != 4U) {
		return -EINVAL;
	}

	bus->transfer_calls++;
	bus->last_request = *request;

	if (bus->failures_remaining > 0U) {
		bus->failures_remaining--;
		return -EIO;
	}

	const int sensor = sensor_from_request(request);
	if (sensor < 0) {
		return sensor;
	}

	const uint32_t value = (uint32_t)bus->value_milli[sensor];
	data[0] = (uint8_t)(value & 0xffU);
	data[1] = (uint8_t)((value >> 8) & 0xffU);
	data[2] = (uint8_t)((value >> 16) & 0xffU);
	data[3] = (uint8_t)((value >> 24) & 0xffU);

	return 0;
}
