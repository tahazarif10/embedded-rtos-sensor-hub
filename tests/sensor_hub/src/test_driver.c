/*
 * Copyright (c) 2026 Taha Zarif
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <zephyr/ztest.h>

#include <sensor_hub/acquisition.h>
#include <sensor_hub/sim_bus.h>

static struct sensor_hub_sim_bus bus;
static struct sensor_hub_driver driver;
static const struct sensor_hub_bus_ops ops = {
	.read = sensor_hub_sim_bus_read,
};

static void driver_before(void *fixture)
{
	ARG_UNUSED(fixture);
	sensor_hub_sim_bus_init(&bus);
	zassert_ok(sensor_hub_driver_init(&driver, &ops, &bus, 2U));
}

ZTEST(sensor_driver, test_temperature_uses_i2c_contract)
{
	struct sensor_hub_sample sample;

	sensor_hub_sim_bus_set_value(&bus, SENSOR_HUB_TEMP, 22350);
	zassert_ok(sensor_hub_driver_read(
		&driver, SENSOR_HUB_TEMP, 0U, 100U, &sample));

	zassert_equal(bus.last_request.bus, SENSOR_HUB_BUS_I2C);
	zassert_equal(bus.last_request.device, 0x48U);
	zassert_equal(bus.last_request.reg, 0x00U);
	zassert_equal(sample.value_milli, 22350);
	zassert_equal(sample.sensor_id, SENSOR_HUB_TEMP);
}

ZTEST(sensor_driver, test_vibration_uses_spi_contract)
{
	struct sensor_hub_sample sample;

	sensor_hub_sim_bus_set_value(&bus, SENSOR_HUB_VIBRATION, 1400);
	zassert_ok(sensor_hub_driver_read(
		&driver, SENSOR_HUB_VIBRATION, 0U, 200U, &sample));

	zassert_equal(bus.last_request.bus, SENSOR_HUB_BUS_SPI);
	zassert_equal(bus.last_request.device, 0x01U);
	zassert_equal(bus.last_request.reg, 0x10U);
	zassert_equal(sample.value_milli, 1400);
	zassert_equal(sample.sensor_id, SENSOR_HUB_VIBRATION);
}

ZTEST(sensor_driver, test_retry_recovers_after_transient_bus_failures)
{
	struct sensor_hub_sample sample;
	struct sensor_hub_driver_stats stats;

	sensor_hub_sim_bus_set_value(&bus, SENSOR_HUB_TEMP, 22100);
	sensor_hub_sim_bus_inject_failures(&bus, 2U);

	zassert_ok(sensor_hub_driver_read(
		&driver, SENSOR_HUB_TEMP, 0U, 100U, &sample));
	sensor_hub_driver_snapshot(&driver, &stats);

	zassert_equal(stats.transfer_attempts, 3U);
	zassert_equal(stats.retry_attempts, 2U);
	zassert_equal(stats.failed_reads, 0U);
	zassert_equal(bus.failures_remaining, 0U);
	zassert_equal(sample.value_milli, 22100);
}

ZTEST(sensor_driver, test_retry_exhaustion_is_counted)
{
	struct sensor_hub_sample sample;
	struct sensor_hub_driver_stats stats;

	zassert_ok(sensor_hub_driver_init(&driver, &ops, &bus, 1U));
	sensor_hub_sim_bus_inject_failures(&bus, 3U);

	zassert_equal(sensor_hub_driver_read(
		&driver, SENSOR_HUB_TEMP, 0U, 100U, &sample), -EIO);
	sensor_hub_driver_snapshot(&driver, &stats);

	zassert_equal(stats.transfer_attempts, 2U);
	zassert_equal(stats.retry_attempts, 1U);
	zassert_equal(stats.failed_reads, 1U);
}

ZTEST(sensor_driver, test_sequence_gap_is_detected_and_resynchronized)
{
	struct sensor_hub_sample sample;
	struct sensor_hub_driver_stats stats;

	sensor_hub_sim_bus_set_value(&bus, SENSOR_HUB_TEMP, 22000);
	zassert_ok(sensor_hub_driver_read(
		&driver, SENSOR_HUB_TEMP, 10U, 100U, &sample));
	zassert_equal(sensor_hub_driver_read(
		&driver, SENSOR_HUB_TEMP, 12U, 110U, &sample), -EILSEQ);

	/* Sequence tracking resynchronizes to 12, so 13 is accepted. */
	zassert_ok(sensor_hub_driver_read(
		&driver, SENSOR_HUB_TEMP, 13U, 120U, &sample));
	sensor_hub_driver_snapshot(&driver, &stats);

	zassert_equal(stats.sequence_errors, 1U);
	zassert_equal(sample.sequence, 13U);
}

ZTEST(sensor_driver, test_timestamp_regression_is_detected_without_cascade)
{
	struct sensor_hub_sample sample;
	struct sensor_hub_driver_stats stats;

	sensor_hub_sim_bus_set_value(&bus, SENSOR_HUB_VIBRATION, 1200);
	zassert_ok(sensor_hub_driver_read(
		&driver, SENSOR_HUB_VIBRATION, 1U, 100U, &sample));
	zassert_equal(sensor_hub_driver_read(
		&driver, SENSOR_HUB_VIBRATION, 2U, 90U, &sample), -ERANGE);

	/* The regressed timestamp is not accepted as the new time baseline. */
	zassert_ok(sensor_hub_driver_read(
		&driver, SENSOR_HUB_VIBRATION, 3U, 110U, &sample));
	sensor_hub_driver_snapshot(&driver, &stats);

	zassert_equal(stats.timestamp_regressions, 1U);
	zassert_equal(sample.timestamp_ms, 110U);
}

ZTEST(sensor_driver, test_invalid_driver_arguments_are_rejected)
{
	struct sensor_hub_sample sample;

	zassert_equal(sensor_hub_driver_init(NULL, &ops, &bus, 1U), -EINVAL);
	zassert_equal(sensor_hub_driver_init(&driver, NULL, &bus, 1U), -EINVAL);
	zassert_equal(sensor_hub_driver_read(
		NULL, SENSOR_HUB_TEMP, 0U, 0U, &sample), -EINVAL);
	zassert_equal(sensor_hub_driver_read(
		&driver, SENSOR_HUB_SENSOR_COUNT, 0U, 0U, &sample), -EINVAL);
	zassert_equal(sensor_hub_driver_read(
		&driver, SENSOR_HUB_TEMP, 0U, 0U, NULL), -EINVAL);
}

ZTEST_SUITE(sensor_driver, NULL, NULL, driver_before, NULL, NULL);
