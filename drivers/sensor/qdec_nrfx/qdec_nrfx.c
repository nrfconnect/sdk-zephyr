/*
 * Copyright (c) 2018, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sensor.h>
#include <spinlock.h>

#include <nrfx_qdec.h>
#include <hal/nrf_gpio.h>

#define SYS_LOG_DOMAIN "qdec_nrfx"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_QDEC_NRFX_LEVEL
#include <logging/sys_log.h>


#define FULL_ANGLE 360

/* limit range to avoid overflow when converting steps to degrees */
#define ACC_MAX (INT_MAX / FULL_ANGLE)
#define ACC_MIN (INT_MIN / FULL_ANGLE)


struct qdec_nrfx_data {
	struct k_spinlock        lock;
	s32_t                    acc;
	sensor_trigger_handler_t data_ready_handler;
#ifdef CONFIG_DEVICE_POWER_MANAGEMENT
	u32_t                    pm_state;
#endif
};


static struct qdec_nrfx_data qdec_nrfx_data;

DEVICE_DECLARE(qdec_nrfx);


static void accumulate(struct qdec_nrfx_data *data, int16_t acc)
{
	k_spinlock_key_t key = k_spin_lock(&data->lock);

	bool overflow = ((acc > 0) && (ACC_MAX - acc < data->acc)) ||
			((acc < 0) && (ACC_MIN - acc > data->acc));

	if (!overflow) {
		data->acc += acc;
	}

	k_spin_unlock(&data->lock, key);
}

static int qdec_nrfx_sample_fetch(struct device *dev, enum sensor_channel chan)
{
	struct qdec_nrfx_data *data = &qdec_nrfx_data;

	int16_t acc;
	int16_t accdbl;

	ARG_UNUSED(dev);

	SYS_LOG_DBG("");

	if ((chan != SENSOR_CHAN_ALL) && (chan != SENSOR_CHAN_ROTATION)) {
		return -ENOTSUP;
	}

	nrfx_qdec_accumulators_read(&acc, &accdbl);

	accumulate(data, acc);

	return 0;
}

static int qdec_nrfx_channel_get(struct device       *dev,
				 enum sensor_channel  chan,
				 struct sensor_value *val)
{
	struct qdec_nrfx_data *data = &qdec_nrfx_data;
	k_spinlock_key_t key;
	s32_t acc;

	ARG_UNUSED(dev);

	SYS_LOG_DBG("");

	if (chan != SENSOR_CHAN_ROTATION) {
		return -ENOTSUP;
	}

	key = k_spin_lock(&data->lock);
	acc = data->acc;
	data->acc = 0;
	k_spin_unlock(&data->lock, key);

	static_assert(CONFIG_QDEC_STEPS > 0, "only positive number valid");
	static_assert(CONFIG_QDEC_STEPS <= 2148, "overflow possible");

	val->val1 = (acc * FULL_ANGLE) / CONFIG_QDEC_STEPS;
	val->val2 = (acc * FULL_ANGLE) - (val->val1 * CONFIG_QDEC_STEPS);
	if (val->val2 != 0) {
		val->val2 *= 1000000;
		val->val2 /= CONFIG_QDEC_STEPS;
	}

	return 0;
}

static int qdec_nrfx_trigger_set(struct device               *dev,
				 const struct sensor_trigger *trig,
				 sensor_trigger_handler_t     handler)
{
	struct qdec_nrfx_data *data = &qdec_nrfx_data;
	k_spinlock_key_t key;

	ARG_UNUSED(dev);

	SYS_LOG_DBG("");

	if (trig->type != SENSOR_TRIG_DATA_READY) {
		return -ENOTSUP;
	}

	if ((trig->chan != SENSOR_CHAN_ALL) &&
	    (trig->chan != SENSOR_CHAN_ROTATION)) {
		return -ENOTSUP;
	}

	key = k_spin_lock(&data->lock);
	data->data_ready_handler = handler;
	k_spin_unlock(&data->lock, key);

	return 0;
}

static void qdec_nrfx_event_handler(nrfx_qdec_event_t event)
{
	sensor_trigger_handler_t handler;
	k_spinlock_key_t key;

	switch (event.type) {
	case NRF_QDEC_EVENT_REPORTRDY:
		accumulate(&qdec_nrfx_data, event.data.report.acc);

		key = k_spin_lock(&qdec_nrfx_data.lock);
		handler = qdec_nrfx_data.data_ready_handler;
		k_spin_unlock(&qdec_nrfx_data.lock, key);

		if (handler) {
			struct sensor_trigger trig = {
				.type = SENSOR_TRIG_DATA_READY,
				.chan = SENSOR_CHAN_ROTATION,
			};

			handler(DEVICE_GET(qdec_nrfx), &trig);
		}
		break;

	default:
		SYS_LOG_ERR("unhandled event (0x%x)", event.type);
		break;
	};
}

static void qdec_nrfx_gpio_ctrl(bool enable)
{
	if (CONFIG_QDEC_ENABLE_PIN != 0xFFFFFFFF) {
		uint32_t val = (enable)?(0):(1);

		nrf_gpio_pin_write(CONFIG_QDEC_ENABLE_PIN, val);
		nrf_gpio_cfg_output(CONFIG_QDEC_ENABLE_PIN);
	}
}

static int qdec_nrfx_init(struct device *dev)
{
	static const nrfx_qdec_config_t config = {
		.reportper          = NRF_QDEC_REPORTPER_40,
		.sampleper          = NRF_QDEC_SAMPLEPER_2048us,
		.psela              = CONFIG_QDEC_A_PIN,
		.pselb              = CONFIG_QDEC_B_PIN,
		.pselled            = CONFIG_QDEC_LED_PIN,
		.ledpre             = CONFIG_QDEC_LED_PRE,
		.ledpol             = NRF_QDEC_LEPOL_ACTIVE_HIGH,
		.interrupt_priority = NRFX_QDEC_CONFIG_IRQ_PRIORITY,
		.dbfen              = 0, /* disabled */
		.sample_inten       = 0, /* disabled */
	};

	nrfx_err_t nerr;

	SYS_LOG_DBG("");

	IRQ_CONNECT(CONFIG_QDEC_IRQ, CONFIG_QDEC_IRQ_PRI,
		    nrfx_isr, nrfx_qdec_irq_handler, 0);

	nerr = nrfx_qdec_init(&config, qdec_nrfx_event_handler);
	if (nerr == NRFX_ERROR_INVALID_STATE) {
		SYS_LOG_ERR("qdec already in use");
		return -EBUSY;
	} else if (nerr != NRFX_SUCCESS) {
		SYS_LOG_ERR("failed to initialize qdec");
		return -EFAULT;
	}

	qdec_nrfx_gpio_ctrl(true);
	nrfx_qdec_enable();

#ifdef CONFIG_DEVICE_POWER_MANAGEMENT
	struct qdec_nrfx_data *data = &qdec_nrfx_data;

	data->pm_state = DEVICE_PM_ACTIVE_STATE;
#endif

	return 0;
}

#ifdef CONFIG_DEVICE_POWER_MANAGEMENT

static int qdec_nrfx_pm_get_state(struct qdec_nrfx_data *data,
				  u32_t                 *state)
{
	k_spinlock_key_t key = k_spin_lock(&data->lock);
	*state = data->pm_state;
	k_spin_unlock(&data->lock, key);

	return 0;
}

static int qdec_nrfx_pm_set_state(struct qdec_nrfx_data *data,
				  u32_t                  new_state)
{
	u32_t old_state;

	k_spinlock_key_t key = k_spin_lock(&data->lock);
	old_state = data->pm_state;
	k_spin_unlock(&data->lock, key);

	if (old_state == new_state) {
		/* leave unchanged */
		return 0;
	}

	if (old_state == DEVICE_PM_ACTIVE_STATE) {
		/* device must be suspended */
		nrfx_qdec_disable();
		qdec_nrfx_gpio_ctrl(false);
	}

	if (new_state == DEVICE_PM_OFF_STATE) {
		/* device must be uninitialized */
		nrfx_qdec_uninit();
	}

	if (new_state == DEVICE_PM_ACTIVE_STATE) {
		qdec_nrfx_gpio_ctrl(true);
		nrfx_qdec_enable();
	}

	/* record the new state */
	key = k_spin_lock(&data->lock);
	data->pm_state = new_state;
	k_spin_unlock(&data->lock, key);

	return 0;
}

static int qdec_nrfx_pm_control(struct device *dev,
				u32_t          ctrl_command,
				void          *context)
{
	struct qdec_nrfx_data *data = &qdec_nrfx_data;
	int err;

	SYS_LOG_DBG("");

	switch (ctrl_command) {
	case DEVICE_PM_GET_POWER_STATE:
		err = qdec_nrfx_pm_get_state(data, context);
		break;

	case DEVICE_PM_SET_POWER_STATE:
		err = qdec_nrfx_pm_set_state(data, *((u32_t *)context));
		break;

	default:
		err = -ENOTSUP;
		break;
	}

	return err;
}

#endif /* CONFIG_DEVICE_POWER_MANAGEMENT */


static const struct sensor_driver_api qdec_nrfx_driver_api = {
	.sample_fetch = qdec_nrfx_sample_fetch,
	.channel_get  = qdec_nrfx_channel_get,
	.trigger_set  = qdec_nrfx_trigger_set,
};

DEVICE_DEFINE(qdec_nrfx, CONFIG_QDEC_NAME, qdec_nrfx_init,
		qdec_nrfx_pm_control, NULL, NULL, POST_KERNEL,
		CONFIG_SENSOR_INIT_PRIORITY, &qdec_nrfx_driver_api);
