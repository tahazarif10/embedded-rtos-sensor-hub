/*
 * Copyright (c) 2026 Taha Zarif
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SENSOR_HUB_SIM_BUS_H_
#define SENSOR_HUB_SIM_BUS_H_

#include <stdint.h>

#include <sensor_hub/acquisition.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sensor_hub_sim_bus {
	int32_t value_milli[SENSOR_HUB_SENSOR_COUNT];
	uint32_t transfer_calls;
	uint32_t failures_remaining;
	struct sensor_hub_bus_request last_request;
};

void sensor_hub_sim_bus_init(struct sensor_hub_sim_bus *bus);
void sensor_hub_sim_bus_set_value(struct sensor_hub_sim_bus *bus,
				  enum sensor_hub_sensor_id sensor,
				  int32_t value_milli);
void sensor_hub_sim_bus_inject_failures(struct sensor_hub_sim_bus *bus,
					uint32_t failures);
int sensor_hub_sim_bus_read(void *context,
			    const struct sensor_hub_bus_request *request,
			    uint8_t *data,
			    size_t len);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_HUB_SIM_BUS_H_ */
