/* Bluetooth: Mesh Generic OnOff, Generic Level, Lighting & Vendor Models
 *
 * Copyright (c) 2018 Vikrant More
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ble_mesh.h"
#include "device_composition.h"
#include "storage.h"

static u8_t storage_id;
u8_t reset_counter;

static void save_reset_counter(void)
{
	settings_save_one("ps/rc", &reset_counter, sizeof(reset_counter));
}

static void save_gen_def_trans_time_state(void)
{
	settings_save_one("ps/gdtt", &gen_def_trans_time_srv_user_data.tt,
			  sizeof(gen_def_trans_time_srv_user_data.tt));
}

static void save_gen_onpowerup_state(void)
{
	settings_save_one("ps/gpo", &gen_power_onoff_srv_user_data.onpowerup,
			  sizeof(gen_power_onoff_srv_user_data.onpowerup));

	if (gen_power_onoff_srv_user_data.onpowerup == 0x02) {
		save_on_flash(LIGHTNESS_TEMP_LAST_STATE);
	}
}

static void save_lightness_temp_def_state(void)
{
	light_ctl_srv_user_data.lightness_temp_def =
		(u32_t) ((light_ctl_srv_user_data.lightness_def << 16) |
			 light_ctl_srv_user_data.temp_def);

	settings_save_one("ps/ltd", &light_ctl_srv_user_data.lightness_temp_def,
			  sizeof(light_ctl_srv_user_data.lightness_temp_def));
}

static void save_lightness_temp_last_state(void)
{
	light_ctl_srv_user_data.lightness_temp_last =
		(u32_t) ((light_ctl_srv_user_data.lightness << 16) |
			 light_ctl_srv_user_data.temp);

	settings_save_one("ps/ltl",
			  &light_ctl_srv_user_data.lightness_temp_last,
			  sizeof(light_ctl_srv_user_data.lightness_temp_last));

	printk("Light CTL Last values have beed saved !!\n");
}

static void save_lightness_range(void)
{
	light_lightness_srv_user_data.lightness_range =
		(u32_t) ((light_lightness_srv_user_data.light_range_max << 16) |
			 light_lightness_srv_user_data.light_range_min);

	settings_save_one("ps/lr",
			  &light_lightness_srv_user_data.lightness_range,
			  sizeof(light_lightness_srv_user_data.lightness_range)
			 );
}

static void save_temperature_range(void)
{
	light_ctl_srv_user_data.temperature_range =
		(u32_t) ((light_ctl_srv_user_data.temp_range_max << 16) |
			 light_ctl_srv_user_data.temp_range_min);

	settings_save_one("ps/tr",
			  &light_ctl_srv_user_data.temperature_range,
			  sizeof(light_ctl_srv_user_data.temperature_range));
}

static void storage_work_handler(struct k_work *work)
{
	switch (storage_id) {
	case RESET_COUNTER:
		save_reset_counter();
		break;
	case GEN_DEF_TRANS_TIME_STATE:
		save_gen_def_trans_time_state();
		break;
	case GEN_ONPOWERUP_STATE:
		save_gen_onpowerup_state();
		break;
	case LIGHTNESS_TEMP_DEF_STATE:
		save_lightness_temp_def_state();
		break;
	case LIGHTNESS_TEMP_LAST_STATE:
		save_lightness_temp_last_state();
		break;
	case LIGHTNESS_RANGE:
		save_lightness_range();
		break;
	case TEMPERATURE_RANGE:
		save_temperature_range();
		break;
	}
}

K_WORK_DEFINE(storage_work, storage_work_handler);

void save_on_flash(u8_t id)
{
	storage_id = id;
	k_work_submit(&storage_work);
}

static int ps_set(int argc, char **argv, size_t len_rd,
		  settings_read_cb read_cb, void *cb_arg)
{
	ssize_t len = 0;

	if (argc == 1) {
		if (!strcmp(argv[0], "rc")) {
			len = read_cb(cb_arg, &reset_counter,
				      sizeof(reset_counter));
		}

		if (!strcmp(argv[0], "gdtt")) {
			len = read_cb(cb_arg,
			 &gen_def_trans_time_srv_user_data.tt,
			 sizeof(gen_def_trans_time_srv_user_data.tt));
		}

		if (!strcmp(argv[0], "gpo")) {
			len = read_cb(cb_arg,
			 &gen_power_onoff_srv_user_data.onpowerup,
			 sizeof(gen_power_onoff_srv_user_data.onpowerup));
		}

		if (!strcmp(argv[0], "ltd")) {
			len = read_cb(cb_arg,
			 &light_ctl_srv_user_data.lightness_temp_def,
			 sizeof(light_ctl_srv_user_data.lightness_temp_def));
		}

		if (!strcmp(argv[0], "ltl")) {
			len = read_cb(cb_arg,
			 &light_ctl_srv_user_data.lightness_temp_last,
			 sizeof(light_ctl_srv_user_data.lightness_temp_last));
		}

		if (!strcmp(argv[0], "lr")) {
			len = read_cb(cb_arg,
			 &light_lightness_srv_user_data.lightness_range,
			 sizeof(light_lightness_srv_user_data.lightness_range));
		}

		if (!strcmp(argv[0], "tr")) {
			len = read_cb(cb_arg,
			 &light_ctl_srv_user_data.temperature_range,
			 sizeof(light_ctl_srv_user_data.temperature_range));
		}

		return (len < 0) ? len : 0;
	}

	return -ENOENT;
}

static struct settings_handler ps_settings = {
	.name = "ps",
	.h_set = ps_set,
};

int ps_settings_init(void)
{
	int err;

	err = settings_subsys_init();
	if (err) {
		printk("settings_subsys_init failed (err %d)", err);
		return err;
	}

	err = settings_register(&ps_settings);
	if (err) {
		printk("ps_settings_register failed (err %d)", err);
		return err;
	}

	return 0;
}
