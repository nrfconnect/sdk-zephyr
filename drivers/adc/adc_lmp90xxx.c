/*
 * Copyright (c) 2019 Vestas Wind Systems A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ADC driver for the LMP90xxx AFE.
 */

#include <drivers/adc.h>
#include <drivers/adc/lmp90xxx.h>
#include <drivers/gpio.h>
#include <drivers/spi.h>
#include <kernel.h>
#include <sys/byteorder.h>
#include <zephyr.h>

#define LOG_LEVEL CONFIG_ADC_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(adc_lmp90xxx);

#define ADC_CONTEXT_USES_KERNEL_TIMER
#include "adc_context.h"

/* LMP90xxx register addresses */
#define LMP90XXX_REG_RESETCN         0x00U
#define LMP90XXX_REG_SPI_HANDSHAKECN 0x01U
#define LMP90XXX_REG_SPI_RESET       0x02U
#define LMP90XXX_REG_SPI_STREAMCN    0x03U
#define LMP90XXX_REG_PWRCN           0x08U
#define LMP90XXX_REG_DATA_ONLY_1     0x09U
#define LMP90XXX_REG_DATA_ONLY_2     0x0AU
#define LMP90XXX_REG_ADC_RESTART     0x0BU
#define LMP90XXX_REG_GPIO_DIRCN      0x0EU
#define LMP90XXX_REG_GPIO_DAT        0x0FU
#define LMP90XXX_REG_BGCALCN         0x10U
#define LMP90XXX_REG_SPI_DRDYBCN     0x11U
#define LMP90XXX_REG_ADC_AUXCN       0x12U
#define LMP90XXX_REG_SPI_CRC_CN      0x13U
#define LMP90XXX_REG_SENDIAG_THLDH   0x14U
#define LMP90XXX_REG_SENDIAG_THLDL   0x15U
#define LMP90XXX_REG_SCALCN          0x17U
#define LMP90XXX_REG_ADC_DONE        0x18U
#define LMP90XXX_REG_SENDIAG_FLAGS   0x19U
#define LMP90XXX_REG_ADC_DOUT        0x1AU
#define LMP90XXX_REG_SPI_CRC_DAT     0x1DU
#define LMP90XXX_REG_CH_STS          0x1EU
#define LMP90XXX_REG_CH_SCAN         0x1FU

/* LMP90xxx channel input and configuration registers */
#define LMP90XXX_REG_CH_INPUTCN(ch) (0x20U + (2 * ch))
#define LMP90XXX_REG_CH_CONFIG(ch)  (0x21U + (2 * ch))

/* LMP90xxx upper (URA) and lower (LRA) register addresses */
#define LMP90XXX_URA(addr) ((addr >> 4U) & GENMASK(2, 0))
#define LMP90XXX_LRA(addr) (addr & GENMASK(3, 0))

/* LMP90xxx instruction byte 1 (INST1) */
#define LMP90XXX_INST1_WAB 0x10U
#define LMP90XXX_INST1_RA  0x90U

/* LMP90xxx instruction byte 2 (INST2) */
#define LMP90XXX_INST2_WB        0U
#define LMP90XXX_INST2_R         BIT(7)
#define LMP90XXX_INST2_SZ_1      (0x0U << 5)
#define LMP90XXX_INST2_SZ_2      (0x1U << 5)
#define LMP90XXX_INST2_SZ_3      (0x2U << 5)
#define LMP90XXX_INST2_SZ_STREAM (0x3U << 5)

/* LMP90xxx register values/commands */
#define LMP90XXX_REG_AND_CNV_RST     0xC3U
#define LMP90XXX_SDO_DRDYB_DRIVER(x) ((x & BIT_MASK(3)) << 1)
#define LMP90XXX_PWRCN(x)            (x & BIT_MASK(2))
#define LMP90XXX_RTD_CUR_SEL(x)      (x & BIT_MASK(4))
#define LMP90XXX_SPI_DRDYB_D6(x)     ((x & BIT(0)) << 7)
#define LMP90XXX_EN_CRC(x)           ((x & BIT(0)) << 4)
#define LMP90XXX_DRDYB_AFT_CRC(x)    ((x & BIT(0)) << 2)
#define LMP90XXX_CH_SCAN_SEL(x)      ((x & BIT_MASK(2)) << 6)
#define LMP90XXX_LAST_CH(x)          ((x & BIT_MASK(3)) << 3)
#define LMP90XXX_FIRST_CH(x)         (x & BIT_MASK(3))
#define LMP90XXX_BURNOUT_EN(x)       ((x & BIT(0)) << 7)
#define LMP90XXX_VREF_SEL(x)         ((x & BIT(0)) << 6)
#define LMP90XXX_VINP(x)             ((x & BIT_MASK(3)) << 3)
#define LMP90XXX_VINN(x)             (x & BIT_MASK(3))
#define LMP90XXX_BGCALN(x)           (x & BIT_MASK(3))
#define LMP90XXX_ODR_SEL(x)          ((x & BIT_MASK(3)) << 4)
#define LMP90XXX_GAIN_SEL(x)         ((x & BIT_MASK(3)) << 1)
#define LMP90XXX_BUF_EN(x)           (x & BIT(0))

/* Invalid (never used) Upper Register Address */
#define LMP90XXX_INVALID_URA UINT8_MAX

/* Maximum number of ADC channels */
#define LMP90XXX_MAX_CHANNELS 7

/* Maximum number of ADC inputs */
#define LMP90XXX_MAX_INPUTS 8

/* Default Output Data Rate (ODR) is 214.65 SPS */
#define LMP90XXX_DEFAULT_ODR 7

/* Macro for checking if Data Ready Bar IRQ is in use */
#define LMP90XXX_HAS_DRDYB(config) (config->drdyb_dev_name != NULL)

struct lmp90xxx_config {
	const char *spi_dev_name;
	const char *spi_cs_dev_name;
	u8_t spi_cs_pin;
	struct spi_config spi_cfg;
	const char *drdyb_dev_name;
	u32_t drdyb_pin;
	int drdyb_flags;
	u8_t rtd_current;
	u8_t resolution;
	u8_t channels;
};

struct lmp90xxx_data {
	struct device *dev;
	struct adc_context ctx;
	struct device *spi_dev;
	struct spi_cs_control spi_cs;
	struct gpio_callback drdyb_cb;
	struct k_mutex ura_lock;
	u8_t ura;
	s32_t *buffer;
	s32_t *repeat_buffer;
	u32_t channels;
	u32_t channel_id;
	u8_t channel_odr[LMP90XXX_MAX_CHANNELS];
#ifdef CONFIG_ADC_LMP90XXX_GPIO
	struct k_mutex gpio_lock;
	u8_t gpio_dircn;
	u8_t gpio_dat;
#endif /* CONFIG_ADC_LMP90XXX_GPIO */
	struct k_thread thread;
	struct k_sem sem;

	K_THREAD_STACK_MEMBER(stack,
			CONFIG_ADC_LMP90XXX_ACQUISITION_THREAD_STACK_SIZE);
};

/*
 * Approximated LMP90xxx acquisition times in milliseconds. These are
 * used for the initial delay when polling for data ready.
 */
static const s32_t lmp90xxx_odr_delay_tbl[8] = {
	596, /* 13.42/8 = 1.6775 SPS */
	298, /* 13.42/4 = 3.355 SPS */
	149, /* 13.42/2 = 6.71 SPS */
	75,  /* 13.42 SPS */
	37,  /* 214.65/8 = 26.83125 SPS */
	19,  /* 214.65/4 = 53.6625 SPS */
	9,   /* 214.65/2 = 107.325 SPS */
	5,   /* 214.65 SPS (default) */
};

static inline u8_t lmp90xxx_inst2_sz(size_t len)
{
	if (len == 1) {
		return LMP90XXX_INST2_SZ_1;
	} else if (len == 2) {
		return LMP90XXX_INST2_SZ_2;
	} else if (len == 3) {
		return LMP90XXX_INST2_SZ_3;
	} else {
		return LMP90XXX_INST2_SZ_STREAM;
	}
}

static int lmp90xxx_read_reg(struct device *dev, u8_t addr, u8_t *dptr,
			     size_t len)
{
	const struct lmp90xxx_config *config = dev->config->config_info;
	struct lmp90xxx_data *data = dev->driver_data;
	u8_t ura = LMP90XXX_URA(addr);
	u8_t inst1_uab[2] = { LMP90XXX_INST1_WAB, ura };
	u8_t inst2 = LMP90XXX_INST2_R | LMP90XXX_LRA(addr);
	struct spi_buf tx_buf[2];
	struct spi_buf rx_buf[2];
	struct spi_buf_set tx;
	struct spi_buf_set rx;
	int dummy = 0;
	int i = 0;
	int err;

	if (len == 0) {
		LOG_ERR("attempt to read 0 bytes from register 0x%02x", addr);
		return -EINVAL;
	}

	k_mutex_lock(&data->ura_lock, K_FOREVER);

	if (ura != data->ura) {
		/* Instruction Byte 1 + Upper Address Byte */
		tx_buf[i].buf = inst1_uab;
		tx_buf[i].len = sizeof(inst1_uab);
		dummy += sizeof(inst1_uab);
		i++;
	}

	/* Instruction Byte 2 */
	inst2 |= lmp90xxx_inst2_sz(len);
	tx_buf[i].buf = &inst2;
	tx_buf[i].len = sizeof(inst2);
	dummy += sizeof(inst2);
	i++;

	/* Dummy RX Bytes */
	rx_buf[0].buf = NULL;
	rx_buf[0].len = dummy;

	/* Data Byte(s) */
	rx_buf[1].buf = dptr;
	rx_buf[1].len = len;

	tx.buffers = tx_buf;
	tx.count = i;
	rx.buffers = rx_buf;
	rx.count = 2;

	err = spi_transceive(data->spi_dev, &config->spi_cfg, &tx, &rx);
	if (!err) {
		data->ura = ura;
	} else {
		/* Force INST1 + UAB on next access */
		data->ura = LMP90XXX_INVALID_URA;
	}

	k_mutex_unlock(&data->ura_lock);

	return err;
}

static int lmp90xxx_read_reg8(struct device *dev, u8_t addr, u8_t *val)
{
	return lmp90xxx_read_reg(dev, addr, val, sizeof(val));
}

static int lmp90xxx_write_reg(struct device *dev, u8_t addr, u8_t *dptr,
			      size_t len)
{
	const struct lmp90xxx_config *config = dev->config->config_info;
	struct lmp90xxx_data *data = dev->driver_data;
	u8_t ura = LMP90XXX_URA(addr);
	u8_t inst1_uab[2] = { LMP90XXX_INST1_WAB, ura };
	u8_t inst2 = LMP90XXX_INST2_WB | LMP90XXX_LRA(addr);
	struct spi_buf tx_buf[3];
	struct spi_buf_set tx;
	int i = 0;
	int err;

	if (len == 0) {
		LOG_ERR("attempt write 0 bytes to register 0x%02x", addr);
		return -EINVAL;
	}

	k_mutex_lock(&data->ura_lock, K_FOREVER);

	if (ura != data->ura) {
		/* Instruction Byte 1 + Upper Address Byte */
		tx_buf[i].buf = inst1_uab;
		tx_buf[i].len = sizeof(inst1_uab);
		i++;
	}

	/* Instruction Byte 2 */
	inst2 |= lmp90xxx_inst2_sz(len);
	tx_buf[i].buf = &inst2;
	tx_buf[i].len = sizeof(inst2);
	i++;

	/* Data Byte(s) */
	tx_buf[i].buf = dptr;
	tx_buf[i].len = len;
	i++;

	tx.buffers = tx_buf;
	tx.count = i;

	err = spi_write(data->spi_dev, &config->spi_cfg, &tx);
	if (!err) {
		data->ura = ura;
	} else {
		/* Force INST1 + UAB on next access */
		data->ura = LMP90XXX_INVALID_URA;
	}

	k_mutex_unlock(&data->ura_lock);

	return err;
}

static int lmp90xxx_write_reg8(struct device *dev, u8_t addr, u8_t val)
{
	return lmp90xxx_write_reg(dev, addr, &val, sizeof(val));
}

static int lmp90xxx_soft_reset(struct device *dev)
{
	int err;

	err = lmp90xxx_write_reg8(dev, LMP90XXX_REG_RESETCN,
				  LMP90XXX_REG_AND_CNV_RST);
	if (err) {
		return err;
	}

	/* Write to RESETCN twice in order to reset mode as well as registers */
	return lmp90xxx_write_reg8(dev, LMP90XXX_REG_RESETCN,
				   LMP90XXX_REG_AND_CNV_RST);
}

static inline bool lmp90xxx_has_channel(struct device *dev, u8_t channel)
{
	const struct lmp90xxx_config *config = dev->config->config_info;

	if (channel >= config->channels) {
		return false;
	} else {
		return true;
	}
}

static inline bool lmp90xxx_has_input(struct device *dev, u8_t input)
{
	const struct lmp90xxx_config *config = dev->config->config_info;

	if (input >= LMP90XXX_MAX_INPUTS) {
		return false;
	} else if (config->channels < LMP90XXX_MAX_CHANNELS &&
		   (input >= 3 && input <= 5)) {
		/* This device only has inputs 0, 1, 2, 6, and 7 */
		return false;
	} else {
		return true;
	}
}

static inline int lmp90xxx_acq_time_to_odr(u16_t acq_time)
{
	u16_t acq_value;

	if (acq_time == ADC_ACQ_TIME_DEFAULT) {
		return LMP90XXX_DEFAULT_ODR;
	}

	if (ADC_ACQ_TIME_UNIT(acq_time) != ADC_ACQ_TIME_TICKS) {
		return -EINVAL;
	}

	/*
	 * The LMP90xxx supports odd (and very slow) output data
	 * rates. Allow the caller to specify the ODR directly using
	 * ADC_ACQ_TIME_TICKS
	 */
	acq_value = ADC_ACQ_TIME_VALUE(acq_time);
	if (acq_value <= LMP90XXX_DEFAULT_ODR) {
		return acq_value;
	}

	return -EINVAL;
}

static int lmp90xxx_adc_channel_setup(struct device *dev,
				      const struct adc_channel_cfg *channel_cfg)
{
	struct lmp90xxx_data *data = dev->driver_data;
	u8_t chx_inputcn = LMP90XXX_BURNOUT_EN(0); /* No burnout currents */
	u8_t chx_config = LMP90XXX_BUF_EN(0);      /* No buffer */
	u8_t payload[2];
	u8_t addr;
	int ret;

	switch (channel_cfg->reference) {
	case ADC_REF_EXTERNAL0:
		chx_inputcn |= LMP90XXX_VREF_SEL(0);
		break;
	case ADC_REF_EXTERNAL1:
		chx_inputcn |= LMP90XXX_VREF_SEL(1);
		break;
	default:
		LOG_ERR("unsupported channel reference type '%d'",
			channel_cfg->reference);
		return -ENOTSUP;
	}

	if (!lmp90xxx_has_channel(dev, channel_cfg->channel_id)) {
		LOG_ERR("unsupported channel id '%d'", channel_cfg->channel_id);
		return -ENOTSUP;
	}

	if (!lmp90xxx_has_input(dev, channel_cfg->input_positive)) {
		LOG_ERR("unsupported positive input '%d'",
			channel_cfg->input_positive);
		return -ENOTSUP;
	}
	chx_inputcn |= LMP90XXX_VINP(channel_cfg->input_positive);

	if (!lmp90xxx_has_input(dev, channel_cfg->input_negative)) {
		LOG_ERR("unsupported negative input '%d'",
			channel_cfg->input_negative);
		return -ENOTSUP;
	}
	chx_inputcn |= LMP90XXX_VINN(channel_cfg->input_negative);

	ret = lmp90xxx_acq_time_to_odr(channel_cfg->acquisition_time);
	if (ret < 0) {
		LOG_ERR("unsupported channel acquisition time 0x%02x",
			channel_cfg->acquisition_time);
		return -ENOTSUP;
	}
	chx_config |= LMP90XXX_ODR_SEL(ret);
	data->channel_odr[channel_cfg->channel_id] = ret;

	switch (channel_cfg->gain) {
	case ADC_GAIN_1:
		chx_config |= LMP90XXX_GAIN_SEL(0);
		break;
	case ADC_GAIN_2:
		chx_config |= LMP90XXX_GAIN_SEL(1);
		break;
	case ADC_GAIN_4:
		chx_config |= LMP90XXX_GAIN_SEL(2);
		break;
	case ADC_GAIN_8:
		chx_config |= LMP90XXX_GAIN_SEL(3);
		break;
	case ADC_GAIN_16:
		chx_config |= LMP90XXX_GAIN_SEL(4);
		break;
	case ADC_GAIN_32:
		chx_config |= LMP90XXX_GAIN_SEL(5);
		break;
	case ADC_GAIN_64:
		chx_config |= LMP90XXX_GAIN_SEL(6);
		break;
	case ADC_GAIN_128:
		chx_config |= LMP90XXX_GAIN_SEL(7);
		break;
	default:
		LOG_ERR("unsupported channel gain '%d'", channel_cfg->gain);
		return -ENOTSUP;
	}

	payload[0] = chx_inputcn;
	payload[1] = chx_config;

	addr = LMP90XXX_REG_CH_INPUTCN(channel_cfg->channel_id);
	ret = lmp90xxx_write_reg(dev, addr, payload, sizeof(payload));
	if (ret) {
		LOG_ERR("failed to configure channel (err %d)", ret);
	}

	return ret;
}

static int lmp90xxx_validate_buffer_size(const struct adc_sequence *sequence)
{
	u8_t channels = 0;
	size_t needed;
	u32_t mask;

	for (mask = BIT(LMP90XXX_MAX_CHANNELS - 1); mask != 0; mask >>= 1) {
		if (mask & sequence->channels) {
			channels++;
		}
	}

	needed = channels * sizeof(s32_t);
	if (sequence->options) {
		needed *= (1 + sequence->options->extra_samplings);
	}

	if (sequence->buffer_size < needed) {
		return -ENOMEM;
	}

	return 0;
}

static int lmp90xxx_adc_start_read(struct device *dev,
				   const struct adc_sequence *sequence)
{
	const struct lmp90xxx_config *config = dev->config->config_info;
	struct lmp90xxx_data *data = dev->driver_data;
	u8_t bgcalcn = LMP90XXX_BGCALN(0x3); /* Default to BgCalMode3 */
	int err;

	if (sequence->resolution != config->resolution) {
		LOG_ERR("unsupported resolution %d", sequence->resolution);
		return -ENOTSUP;
	}

	err = lmp90xxx_validate_buffer_size(sequence);
	if (err) {
		LOG_ERR("buffer size too small");
		return err;
	}

	if (sequence->calibrate) {
		/* Use BgCalMode2 */
		bgcalcn = LMP90XXX_BGCALN(0x2);
	}

	err = lmp90xxx_write_reg8(dev, LMP90XXX_REG_BGCALCN, bgcalcn);
	if (err) {
		LOG_ERR("failed to setup background calibration (err %d)", err);
		return err;
	}

	data->buffer = sequence->buffer;
	adc_context_start_read(&data->ctx, sequence);

	return adc_context_wait_for_completion(&data->ctx);
}

static int lmp90xxx_adc_read_async(struct device *dev,
				   const struct adc_sequence *sequence,
				   struct k_poll_signal *async)
{
	struct lmp90xxx_data *data = dev->driver_data;
	int err;

	adc_context_lock(&data->ctx, async ? true : false, async);
	err = lmp90xxx_adc_start_read(dev, sequence);
	adc_context_release(&data->ctx, err);

	return err;
}

static int lmp90xxx_adc_read(struct device *dev,
			     const struct adc_sequence *sequence)
{
	return lmp90xxx_adc_read_async(dev, sequence, NULL);
}

static void lmp90xxx_adc_start_channel(struct device *dev)
{
	const struct lmp90xxx_config *config = dev->config->config_info;
	struct lmp90xxx_data *data = dev->driver_data;
	u8_t ch_scan;
	int err;

	data->channel_id = find_lsb_set(data->channels) - 1;

	LOG_DBG("starting channel %d", data->channel_id);

	/* Single channel, single scan mode */
	ch_scan = LMP90XXX_CH_SCAN_SEL(0x1) |
		  LMP90XXX_FIRST_CH(data->channel_id) |
		  LMP90XXX_LAST_CH(data->channel_id);

	err = lmp90xxx_write_reg8(dev, LMP90XXX_REG_CH_SCAN, ch_scan);
	if (err) {
		LOG_ERR("failed to setup scan channels (err %d)", err);
		adc_context_complete(&data->ctx, err);
		return;
	}

	/* Start scan */
	err = lmp90xxx_write_reg8(dev, LMP90XXX_REG_PWRCN, LMP90XXX_PWRCN(0));
	if (err) {
		LOG_ERR("failed to set active mode (err %d)", err);
		adc_context_complete(&data->ctx, err);
		return;
	}

	if (!LMP90XXX_HAS_DRDYB(config)) {
		/* Signal thread to start polling for data ready */
		k_sem_give(&data->sem);
	}
}

static void adc_context_start_sampling(struct adc_context *ctx)
{
	struct lmp90xxx_data *data =
		CONTAINER_OF(ctx, struct lmp90xxx_data, ctx);

	data->channels = ctx->sequence.channels;
	data->repeat_buffer = data->buffer;

	lmp90xxx_adc_start_channel(data->dev);
}

static void adc_context_update_buffer_pointer(struct adc_context *ctx,
					      bool repeat_sampling)
{
	struct lmp90xxx_data *data =
		CONTAINER_OF(ctx, struct lmp90xxx_data, ctx);

	if (repeat_sampling) {
		data->buffer = data->repeat_buffer;
	}
}

static u8_t lmp90xxx_crc8(u8_t val, const void *buf, size_t cnt)
{
	const u8_t *p = buf;
	int i, j;

	for (i = 0; i < cnt; i++) {
		val ^= p[i];

		for (j = 0; j < 8; j++) {
			if (val & 0x80) {
				val = (val << 1) ^ 0x31;
			} else {
				val <<= 1;
			}
		}
	}

	return val ^ 0xFFU;
}

static void lmp90xxx_acquisition_thread(struct device *dev)
{
	const struct lmp90xxx_config *config = dev->config->config_info;
	struct lmp90xxx_data *data = dev->driver_data;
	s32_t result = 0;
	u8_t adc_done;
	u8_t buf[4]; /* ADC_DOUT + CRC */
	s32_t delay;
	u8_t odr;
	int err;

	while (true) {
		k_sem_take(&data->sem, K_FOREVER);

		if (!LMP90XXX_HAS_DRDYB(config)) {
			odr = data->channel_odr[data->channel_id];
			delay = lmp90xxx_odr_delay_tbl[odr];
			LOG_DBG("sleeping for %d ms", delay);
			k_sleep(delay);

			/* Poll for data ready */
			do {
				err = lmp90xxx_read_reg8(dev,
							 LMP90XXX_REG_ADC_DONE,
							 &adc_done);
				if (adc_done == 0xFFU) {
					LOG_DBG("sleeping for 1 ms");
					k_sleep(1);
				} else {
					break;
				}
			} while (true);
		}

		if (IS_ENABLED(CONFIG_ADC_LMP90XXX_CRC)) {
			err = lmp90xxx_read_reg(dev, LMP90XXX_REG_ADC_DOUT,
						buf, sizeof(buf));
		} else {
			err = lmp90xxx_read_reg(dev, LMP90XXX_REG_ADC_DOUT,
						buf, config->resolution / 8);
		}

		if (err) {
			LOG_ERR("failed to read ADC DOUT (err %d)", err);
			adc_context_complete(&data->ctx, err);
			return;
		}

		if (IS_ENABLED(CONFIG_ADC_LMP90XXX_CRC)) {
			u8_t crc = lmp90xxx_crc8(0, buf, 3);

			if (buf[3] != crc) {
				LOG_ERR("CRC mismatch (0x%02x vs. 0x%02x)",
					buf[3], crc);
				adc_context_complete(&data->ctx, -EIO);
				return;
			}
		}

		/* Read result, get rid of CRC, and sign extend result */
		result = (s32_t)sys_get_be32(buf);
		result >>= (32 - config->resolution);

		LOG_DBG("finished channel %d, result = %d", data->channel_id,
			result);

		/*
		 * ADC samples are stored as s32_t regardless of the
		 * resolution in order to provide a uniform interface
		 * for the driver.
		 */
		*data->buffer++ = result;
		data->channels &= ~BIT(data->channel_id);

		if (data->channels) {
			lmp90xxx_adc_start_channel(dev);
		} else {
			adc_context_on_sampling_done(&data->ctx, dev);
		}
	}
}

static void lmp90xxx_drdyb_callback(struct device *port,
				    struct gpio_callback *cb, u32_t pins)
{
	struct lmp90xxx_data *data =
		CONTAINER_OF(cb, struct lmp90xxx_data, drdyb_cb);

	/* Signal thread that data is now ready */
	k_sem_give(&data->sem);
}

#ifdef CONFIG_ADC_LMP90XXX_GPIO
int lmp90xxx_gpio_set_output(struct device *dev, u8_t pin)
{
	struct lmp90xxx_data *data = dev->driver_data;
	int err = 0;
	u8_t tmp;

	if (pin > LMP90XXX_GPIO_MAX) {
		return -EINVAL;
	}

	k_mutex_lock(&data->gpio_lock, K_FOREVER);

	tmp = data->gpio_dircn | BIT(pin);
	if (tmp != data->gpio_dircn) {
		err = lmp90xxx_write_reg8(dev, LMP90XXX_REG_GPIO_DIRCN, tmp);
		if (!err) {
			data->gpio_dircn = tmp;
		}
	}

	k_mutex_unlock(&data->gpio_lock);

	return err;
}

int lmp90xxx_gpio_set_input(struct device *dev, u8_t pin)
{
	struct lmp90xxx_data *data = dev->driver_data;
	int err = 0;
	u8_t tmp;

	if (pin > LMP90XXX_GPIO_MAX) {
		return -EINVAL;
	}

	k_mutex_lock(&data->gpio_lock, K_FOREVER);

	tmp = data->gpio_dircn & ~BIT(pin);
	if (tmp != data->gpio_dircn) {
		err = lmp90xxx_write_reg8(dev, LMP90XXX_REG_GPIO_DIRCN, tmp);
		if (!err) {
			data->gpio_dircn = tmp;
		}
	}

	k_mutex_unlock(&data->gpio_lock);

	return err;
}

int lmp90xxx_gpio_set_pin_value(struct device *dev, u8_t pin, bool value)
{
	struct lmp90xxx_data *data = dev->driver_data;
	int err = 0;
	u8_t tmp;

	if (pin > LMP90XXX_GPIO_MAX) {
		return -EINVAL;
	}

	k_mutex_lock(&data->gpio_lock, K_FOREVER);

	tmp = data->gpio_dat;
	WRITE_BIT(tmp, pin, value);

	if (tmp != data->gpio_dat) {
		err = lmp90xxx_write_reg8(dev, LMP90XXX_REG_GPIO_DAT, tmp);
		if (!err) {
			data->gpio_dat = tmp;
		}
	}

	k_mutex_unlock(&data->gpio_lock);

	return err;
}

int lmp90xxx_gpio_get_pin_value(struct device *dev, u8_t pin, bool *value)
{
	struct lmp90xxx_data *data = dev->driver_data;
	int err = 0;
	u8_t tmp;

	if (pin > LMP90XXX_GPIO_MAX) {
		return -EINVAL;
	}

	k_mutex_lock(&data->gpio_lock, K_FOREVER);

	err = lmp90xxx_read_reg8(dev, LMP90XXX_REG_GPIO_DAT, &tmp);
	if (!err) {
		*value = tmp & BIT(pin);
	}

	k_mutex_unlock(&data->gpio_lock);

	return err;
}
#endif /* CONFIG_ADC_LMP90XXX_GPIO */

static int lmp90xxx_init(struct device *dev)
{
	const struct lmp90xxx_config *config = dev->config->config_info;
	struct lmp90xxx_data *data = dev->driver_data;
	struct device *drdyb_dev;
	int err;

	data->dev = dev;
	k_mutex_init(&data->ura_lock);
	k_sem_init(&data->sem, 0, 1);
#ifdef CONFIG_ADC_LMP90XXX_GPIO
	k_mutex_init(&data->gpio_lock);
#endif /* CONFIG_ADC_LMP90XXX_GPIO */

	/* Force INST1 + UAB on first access */
	data->ura = LMP90XXX_INVALID_URA;

	data->spi_dev = device_get_binding(config->spi_dev_name);
	if (!data->spi_dev) {
		LOG_ERR("SPI master device '%s' not found",
			config->spi_dev_name);
		return -EINVAL;
	}

	if (config->spi_cs_dev_name) {
		data->spi_cs.gpio_dev =
			device_get_binding(config->spi_cs_dev_name);
		if (!data->spi_cs.gpio_dev) {
			LOG_ERR("SPI CS GPIO device '%s' not found",
				config->spi_cs_dev_name);
			return -EINVAL;
		}

		data->spi_cs.gpio_pin = config->spi_cs_pin;
	}

	err = lmp90xxx_soft_reset(dev);
	if (err) {
		LOG_ERR("failed to request soft reset (err %d)", err);
		return err;
	}

	err = lmp90xxx_write_reg8(dev, LMP90XXX_REG_SPI_HANDSHAKECN,
				  LMP90XXX_SDO_DRDYB_DRIVER(0x4));
	if (err) {
		LOG_ERR("failed to set SPI handshake control (err %d)",
			err);
		return err;
	}

	if (config->rtd_current) {
		err = lmp90xxx_write_reg8(dev, LMP90XXX_REG_ADC_AUXCN,
				LMP90XXX_RTD_CUR_SEL(config->rtd_current));
		if (err) {
			LOG_ERR("failed to set RTD current (err %d)", err);
			return err;
		}
	}

	if (IS_ENABLED(CONFIG_ADC_LMP90XXX_CRC)) {
		err = lmp90xxx_write_reg8(dev, LMP90XXX_REG_SPI_CRC_CN,
					LMP90XXX_EN_CRC(1) |
					LMP90XXX_DRDYB_AFT_CRC(1));
		if (err) {
			LOG_ERR("failed to enable CRC (err %d)", err);
			return err;
		}
	}

	if (LMP90XXX_HAS_DRDYB(config)) {
		drdyb_dev = device_get_binding(config->drdyb_dev_name);
		if (!drdyb_dev) {
			LOG_ERR("DRDYB GPIO device '%s' not found",
				config->drdyb_dev_name);
			return -EINVAL;
		}

		err = gpio_pin_configure(drdyb_dev, config->drdyb_pin,
					 (GPIO_DIR_IN | GPIO_INT |
					 GPIO_INT_EDGE | config->drdyb_flags));
		if (err) {
			LOG_ERR("failed to configure DRDYB GPIO pin (err %d)",
				err);
			return -EINVAL;
		}

		gpio_init_callback(&data->drdyb_cb, lmp90xxx_drdyb_callback,
				   BIT(config->drdyb_pin));

		err = gpio_add_callback(drdyb_dev, &data->drdyb_cb);
		if (err) {
			LOG_ERR("failed to add DRDYB callback (err %d)", err);
			return -EINVAL;
		}

		err = lmp90xxx_write_reg8(dev, LMP90XXX_REG_SPI_DRDYBCN,
					  LMP90XXX_SPI_DRDYB_D6(1));
		if (err) {
			LOG_ERR("failed to configure D6 as DRDYB (err %d)",
				err);
			return err;
		}

		err = gpio_pin_enable_callback(drdyb_dev,
					       config->drdyb_pin);
		if (err) {
			LOG_ERR("failed to enable DRDBY callback (err %d)",
				err);
			return -EINVAL;
		}
	}

	k_thread_create(&data->thread, data->stack,
			CONFIG_ADC_LMP90XXX_ACQUISITION_THREAD_STACK_SIZE,
			(k_thread_entry_t)lmp90xxx_acquisition_thread,
			dev, NULL, NULL,
			CONFIG_ADC_LMP90XXX_ACQUISITION_THREAD_PRIO,
			0, K_NO_WAIT);

	/* Put device in stand-by to prepare it for single-shot conversion */
	err = lmp90xxx_write_reg8(dev, LMP90XXX_REG_PWRCN, LMP90XXX_PWRCN(0x3));
	if (err) {
		LOG_ERR("failed to request stand-by mode (err %d)", err);
		return err;
	}

	adc_context_unlock_unconditionally(&data->ctx);

	return 0;
}

static const struct adc_driver_api lmp90xxx_adc_api = {
	.channel_setup = lmp90xxx_adc_channel_setup,
	.read = lmp90xxx_adc_read,
#ifdef CONFIG_ADC_ASYNC
	.read_async = lmp90xxx_adc_read_async,
#endif
};

#define ASSERT_LMP90XXX_CURRENT_VALID(v) \
	BUILD_ASSERT_MSG(v == 0 || v == 100 || v == 200 || v == 300 || \
			 v == 400 || v == 500 || v == 600 || v == 700 || \
			 v == 800 || v == 900 || v == 1000, \
			 "unsupported RTD current (" #v ")")

#define LMP90XXX_UAMPS_TO_RTD_CUR_SEL(x) (x / 100)

#define LMP90XXX_DEVICE(t, n, res, ch) \
	ASSERT_LMP90XXX_CURRENT_VALID(DT_INST_##n##_TI_LMP##t##_RTD_CURRENT); \
	static struct lmp90xxx_data lmp##t##_data_##n = { \
		ADC_CONTEXT_INIT_TIMER(lmp##t##_data_##n, ctx), \
		ADC_CONTEXT_INIT_LOCK(lmp##t##_data_##n, ctx), \
		ADC_CONTEXT_INIT_SYNC(lmp##t##_data_##n, ctx), \
	}; \
	static const struct lmp90xxx_config lmp##t##_config_##n = { \
		.spi_dev_name = DT_INST_##n##_TI_LMP##t##_BUS_NAME, \
		.spi_cs_dev_name = \
			DT_INST_##n##_TI_LMP##t##_CS_GPIOS_CONTROLLER, \
		.spi_cs_pin = DT_INST_##n##_TI_LMP##t##_CS_GPIOS_PIN, \
		.spi_cfg = { \
			.operation = (SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | \
				     SPI_WORD_SET(8)), \
			.frequency = \
				DT_INST_##n##_TI_LMP##t##_SPI_MAX_FREQUENCY, \
			.slave = DT_INST_##n##_TI_LMP##t##_BASE_ADDRESS, \
			.cs = &lmp##t##_data_##n.spi_cs, \
		}, \
		.drdyb_dev_name = \
			DT_INST_##n##_TI_LMP##t##_DRDYB_GPIOS_CONTROLLER, \
		.drdyb_pin = DT_INST_##n##_TI_LMP##t##_DRDYB_GPIOS_PIN, \
		.drdyb_flags = DT_INST_##n##_TI_LMP##t##_DRDYB_GPIOS_FLAGS, \
		.rtd_current = LMP90XXX_UAMPS_TO_RTD_CUR_SEL( \
			DT_INST_##n##_TI_LMP##t##_RTD_CURRENT), \
		.resolution = res, \
		.channels = ch, \
	}; \
	DEVICE_AND_API_INIT(lmp##t##_##n, \
			    DT_INST_##n##_TI_LMP##t##_LABEL, \
			    &lmp90xxx_init, &lmp##t##_data_##n, \
			    &lmp##t##_config_##n, POST_KERNEL, \
			    CONFIG_ADC_LMP90XXX_INIT_PRIORITY, \
			    &lmp90xxx_adc_api)

/*
 * LMP90077: 16 bit, 2 diff/4 se (4 channels), 0 currents
 */
#if DT_INST_0_TI_LMP90077
#ifndef DT_INST_0_TI_LMP90077_CS_GPIOS_CONTROLLER
#define DT_INST_0_TI_LMP90077_CS_GPIOS_CONTROLLER NULL
#define DT_INST_0_TI_LMP90077_CS_GPIOS_PIN 0
#endif /* DT_INST_0_TI_LMP90077_CS_GPIOS_CONTROLLER */
#ifndef DT_INST_0_TI_LMP90077_DRDYB_GPIOS_CONTROLLER
#define DT_INST_0_TI_LMP90077_DRDYB_GPIOS_CONTROLLER NULL
#define DT_INST_0_TI_LMP90077_DRDYB_GPIOS_PIN 0
#define DT_INST_0_TI_LMP90077_DRDYB_GPIOS_FLAGS 0
#endif /* !DT_INST_0_TI_LMP90077_DRDYB_GPIOS_CONTROLLER */
#define DT_INST_0_TI_LMP90077_RTD_CURRENT 0
LMP90XXX_DEVICE(90077, 0, 16, 4);
#endif /* DT_INST_0_TI_LMP90077 */

/*
 * LMP90078: 16 bit, 2 diff/4 se (4 channels), 2 currents
 */
#if DT_INST_0_TI_LMP90078
#ifndef DT_INST_0_TI_LMP90078_CS_GPIOS_CONTROLLER
#define DT_INST_0_TI_LMP90078_CS_GPIOS_CONTROLLER NULL
#define DT_INST_0_TI_LMP90078_CS_GPIOS_PIN 0
#endif /* DT_INST_0_TI_LMP90078_CS_GPIOS_CONTROLLER */
#ifndef DT_INST_0_TI_LMP90078_DRDYB_GPIOS_CONTROLLER
#define DT_INST_0_TI_LMP90078_DRDYB_GPIOS_CONTROLLER NULL
#define DT_INST_0_TI_LMP90078_DRDYB_GPIOS_PIN 0
#define DT_INST_0_TI_LMP90078_DRDYB_GPIOS_FLAGS 0
#endif /* !DT_INST_0_TI_LMP90078_DRDYB_GPIOS_CONTROLLER */
#ifndef DT_INST_0_TI_LMP90078_RTD_CURRENT
#define DT_INST_0_TI_LMP90078_RTD_CURRENT 0
#endif /* DT_INST_0_TI_LMP90078_RTD_CURRENT */
LMP90XXX_DEVICE(90078, 0, 16, 4);
#endif /* DT_INST_0_TI_LMP90078 */

/*
 * LMP90079: 16 bit, 4 diff/7 se (7 channels), 0 currents, has VIN3-5
 */
#if DT_INST_0_TI_LMP90079
#ifndef DT_INST_0_TI_LMP90079_CS_GPIOS_CONTROLLER
#define DT_INST_0_TI_LMP90079_CS_GPIOS_CONTROLLER NULL
#define DT_INST_0_TI_LMP90079_CS_GPIOS_PIN 0
#endif /* DT_INST_0_TI_LMP90079_CS_GPIOS_CONTROLLER */
#ifndef DT_INST_0_TI_LMP90079_DRDYB_GPIOS_CONTROLLER
#define DT_INST_0_TI_LMP90079_DRDYB_GPIOS_CONTROLLER NULL
#define DT_INST_0_TI_LMP90079_DRDYB_GPIOS_PIN 0
#define DT_INST_0_TI_LMP90079_DRDYB_GPIOS_FLAGS 0
#endif /* !DT_INST_0_TI_LMP90079_DRDYB_GPIOS_CONTROLLER */
#define DT_INST_0_TI_LMP90079_RTD_CURRENT 0
LMP90XXX_DEVICE(90079, 0, 16, 7);
#endif /* DT_INST_0_TI_LMP90079 */

/*
 * LMP90080: 16 bit, 4 diff/7 se (7 channels), 2 currents, has VIN3-5
 */
#if DT_INST_0_TI_LMP90080
#ifndef DT_INST_0_TI_LMP90080_CS_GPIOS_CONTROLLER
#define DT_INST_0_TI_LMP90080_CS_GPIOS_CONTROLLER NULL
#define DT_INST_0_TI_LMP90080_CS_GPIOS_PIN 0
#endif /* DT_INST_0_TI_LMP90080_CS_GPIOS_CONTROLLER */
#ifndef DT_INST_0_TI_LMP90080_DRDYB_GPIOS_CONTROLLER
#define DT_INST_0_TI_LMP90080_DRDYB_GPIOS_CONTROLLER NULL
#define DT_INST_0_TI_LMP90080_DRDYB_GPIOS_PIN 0
#define DT_INST_0_TI_LMP90080_DRDYB_GPIOS_FLAGS 0
#endif /* !DT_INST_0_TI_LMP90080_DRDYB_GPIOS_CONTROLLER */
#ifndef DT_INST_0_TI_LMP90080_RTD_CURRENT
#define DT_INST_0_TI_LMP90080_RTD_CURRENT 0
#endif /* DT_INST_0_TI_LMP90080_RTD_CURRENT */
LMP90XXX_DEVICE(90080, 0, 16, 7);
#endif /* DT_INST_0_TI_LMP90080 */

/*
 * LMP90097: 24 bit, 2 diff/4 se (4 channels), 0 currents
 */
#if DT_INST_0_TI_LMP90097
#ifndef DT_INST_0_TI_LMP90097_CS_GPIOS_CONTROLLER
#define DT_INST_0_TI_LMP90097_CS_GPIOS_CONTROLLER NULL
#define DT_INST_0_TI_LMP90097_CS_GPIOS_PIN 0
#endif /* DT_INST_0_TI_LMP90097_CS_GPIOS_CONTROLLER */
#ifndef DT_INST_0_TI_LMP90097_DRDYB_GPIOS_CONTROLLER
#define DT_INST_0_TI_LMP90097_DRDYB_GPIOS_CONTROLLER NULL
#define DT_INST_0_TI_LMP90097_DRDYB_GPIOS_PIN 0
#define DT_INST_0_TI_LMP90097_DRDYB_GPIOS_FLAGS 0
#endif /* !DT_INST_0_TI_LMP90097_DRDYB_GPIOS_CONTROLLER */
#define DT_INST_0_TI_LMP90097_RTD_CURRENT 0
LMP90XXX_DEVICE(90097, 0, 24, 4);
#endif /* DT_INST_0_TI_LMP90097 */

/*
 * LMP90098: 24 bit, 2 diff/4 se (4 channels), 2 currents
 */
#if DT_INST_0_TI_LMP90098
#ifndef DT_INST_0_TI_LMP90098_CS_GPIOS_CONTROLLER
#define DT_INST_0_TI_LMP90098_CS_GPIOS_CONTROLLER NULL
#define DT_INST_0_TI_LMP90098_CS_GPIOS_PIN 0
#endif /* DT_INST_0_TI_LMP90098_CS_GPIOS_CONTROLLER */
#ifndef DT_INST_0_TI_LMP90098_DRDYB_GPIOS_CONTROLLER
#define DT_INST_0_TI_LMP90098_DRDYB_GPIOS_CONTROLLER NULL
#define DT_INST_0_TI_LMP90098_DRDYB_GPIOS_PIN 0
#define DT_INST_0_TI_LMP90098_DRDYB_GPIOS_FLAGS 0
#endif /* !DT_INST_0_TI_LMP90098_DRDYB_GPIOS_CONTROLLER */
#ifndef DT_INST_0_TI_LMP90098_RTD_CURRENT
#define DT_INST_0_TI_LMP90098_RTD_CURRENT 0
#endif /* DT_INST_0_TI_LMP90098_RTD_CURRENT */
LMP90XXX_DEVICE(90098, 0, 24, 4);
#endif /* DT_INST_0_TI_LMP90098 */

/*
 * LMP90099: 24 bit, 4 diff/7 se (7 channels), 0 currents, has VIN3-5
 */
#if DT_INST_0_TI_LMP90099
#ifndef DT_INST_0_TI_LMP90099_CS_GPIOS_CONTROLLER
#define DT_INST_0_TI_LMP90099_CS_GPIOS_CONTROLLER NULL
#define DT_INST_0_TI_LMP90099_CS_GPIOS_PIN 0
#endif /* DT_INST_0_TI_LMP90099_CS_GPIOS_CONTROLLER */
#ifndef DT_INST_0_TI_LMP90099_DRDYB_GPIOS_CONTROLLER
#define DT_INST_0_TI_LMP90099_DRDYB_GPIOS_CONTROLLER NULL
#define DT_INST_0_TI_LMP90099_DRDYB_GPIOS_PIN 0
#define DT_INST_0_TI_LMP90099_DRDYB_GPIOS_FLAGS 0
#endif /* !DT_INST_0_TI_LMP90099_DRDYB_GPIOS_CONTROLLER */
#define DT_INST_0_TI_LMP90099_RTD_CURRENT 0
LMP90XXX_DEVICE(90099, 0, 24, 7);
#endif /* DT_INST_0_TI_LMP90099 */

/*
 * LMP90100: 24 bit, 4 diff/7 se (7 channels), 2 currents, has VIN3-5
 */
#if DT_INST_0_TI_LMP90100
#ifndef DT_INST_0_TI_LMP90100_CS_GPIOS_CONTROLLER
#define DT_INST_0_TI_LMP90100_CS_GPIOS_CONTROLLER NULL
#define DT_INST_0_TI_LMP90100_CS_GPIOS_PIN 0
#endif /* DT_INST_0_TI_LMP90100_CS_GPIOS_CONTROLLER */
#ifndef DT_INST_0_TI_LMP90100_DRDYB_GPIOS_CONTROLLER
#define DT_INST_0_TI_LMP90100_DRDYB_GPIOS_CONTROLLER NULL
#define DT_INST_0_TI_LMP90100_DRDYB_GPIOS_PIN 0
#define DT_INST_0_TI_LMP90100_DRDYB_GPIOS_FLAGS 0
#endif /* !DT_INST_0_TI_LMP90100_DRDYB_GPIOS_CONTROLLER */
#ifndef DT_INST_0_TI_LMP90100_RTD_CURRENT
#define DT_INST_0_TI_LMP90100_RTD_CURRENT 0
#endif /* DT_INST_0_TI_LMP90100_RTD_CURRENT */
LMP90XXX_DEVICE(90100, 0, 24, 7);
#endif /* DT_INST_0_TI_LMP90100 */
