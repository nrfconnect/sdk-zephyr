/*
 * Copyright (c) 2018 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr.h>
#include <sys/printk.h>

#include <drivers/gpio.h>
#include <drivers/led.h>

#include <audio/dmic.h>

/* uncomment if you want PCM output in ascii */
/*#define PCM_OUTPUT_IN_ASCII		1  */

#define AUDIO_FREQ		16000
#define CHAN_SIZE		16
#define PCM_BLK_SIZE_MS		((AUDIO_FREQ/1000) * sizeof(s16_t))

#define NUM_MS		5000

K_MEM_SLAB_DEFINE(rx_mem_slab, PCM_BLK_SIZE_MS, NUM_MS, 1);

struct pcm_stream_cfg mic_streams = {
	.pcm_rate = AUDIO_FREQ,
	.pcm_width = CHAN_SIZE,
	.block_size = PCM_BLK_SIZE_MS,
	.mem_slab = &rx_mem_slab,
};

struct dmic_cfg cfg = {
	.io = {
		/* requesting a pdm freq around 2MHz */
		.min_pdm_clk_freq = 1800000,
		.max_pdm_clk_freq = 2500000,
	},
	.streams = &mic_streams,
	.channel = {
		.req_num_chan = 1,
	},
};

#define NUM_LEDS 12
#define DELAY_TIME K_MSEC(25)

void signal_sampling_started(void)
{
	static struct device *led0, *led1;

	led0 = device_get_binding(DT_ALIAS_LED0_GPIOS_CONTROLLER);
	gpio_pin_configure(led0, DT_ALIAS_LED0_GPIOS_PIN, GPIO_DIR_OUT);
	gpio_pin_write(led0, DT_ALIAS_LED0_GPIOS_PIN, 1);

	led1 = device_get_binding(DT_ALIAS_LED1_GPIOS_CONTROLLER);
	gpio_pin_configure(led1, DT_ALIAS_LED1_GPIOS_PIN, GPIO_DIR_OUT);
	gpio_pin_write(led1, DT_ALIAS_LED1_GPIOS_PIN, 0);
}

void signal_sampling_stopped(void)
{
	static struct device *led0, *led1;

	led0 = device_get_binding(DT_ALIAS_LED0_GPIOS_CONTROLLER);
	gpio_pin_configure(led0, DT_ALIAS_LED0_GPIOS_PIN, GPIO_DIR_OUT);
	gpio_pin_write(led0, DT_ALIAS_LED0_GPIOS_PIN, 1);

	led1 = device_get_binding(DT_ALIAS_LED1_GPIOS_CONTROLLER);
	gpio_pin_configure(led1, DT_ALIAS_LED1_GPIOS_PIN, GPIO_DIR_OUT);
	gpio_pin_write(led1, DT_ALIAS_LED1_GPIOS_PIN, 1);
}

void signal_print_stopped(void)
{
	static struct device *led0, *led1;

	led0 = device_get_binding(DT_ALIAS_LED0_GPIOS_CONTROLLER);
	gpio_pin_configure(led0, DT_ALIAS_LED0_GPIOS_PIN, GPIO_DIR_OUT);
	gpio_pin_write(led0, DT_ALIAS_LED0_GPIOS_PIN, 0);

	led1 = device_get_binding(DT_ALIAS_LED1_GPIOS_CONTROLLER);
	gpio_pin_configure(led1, DT_ALIAS_LED1_GPIOS_PIN, GPIO_DIR_OUT);
	gpio_pin_write(led1, DT_ALIAS_LED1_GPIOS_PIN, 1);
}

void *rx_block[NUM_MS];
size_t rx_size = PCM_BLK_SIZE_MS;

void main(void)
{
	int i;
	u32_t ms;

#ifdef CONFIG_LP3943
	static struct device *ledc;

	ledc = device_get_binding(DT_INST_0_TI_LP3943_LABEL);
	if (!ledc) {
		printk("Could not get pointer to %s sensor\n",
			DT_INST_0_TI_LP3943_LABEL);
		return;
	}

	/* turn all leds on */
	for (i = 0; i < NUM_LEDS; i++) {
		led_on(ledc, i);
		k_sleep(DELAY_TIME);
	}

	/* turn all leds off */
	for (i = 0; i < NUM_LEDS; i++) {
		led_off(ledc, i);
		k_sleep(DELAY_TIME);
	}

#endif

	printk("ArgonKey test!!\n");

	int ret;

	struct device *mic_dev = device_get_binding(DT_INST_0_ST_MPXXDTYY_LABEL);

	if (!mic_dev) {
		printk("Could not get pointer to %s device\n",
			DT_INST_0_ST_MPXXDTYY_LABEL);
		return;
	}

	ret = dmic_configure(mic_dev, &cfg);
	if (ret < 0) {
		printk("microphone configuration error\n");
		return;
	}

	ret = dmic_trigger(mic_dev, DMIC_TRIGGER_START);
	if (ret < 0) {
		printk("microphone start trigger error\n");
		return;
	}

	signal_sampling_started();

	/* Acquire microphone audio */
	for (ms = 0U; ms < NUM_MS; ms++) {
		ret = dmic_read(mic_dev, 0, &rx_block[ms], &rx_size, 2000);
		if (ret < 0) {
			printk("microphone audio read error\n");
			return;
		}
	}

	signal_sampling_stopped();

	ret = dmic_trigger(mic_dev, DMIC_TRIGGER_STOP);
	if (ret < 0) {
		printk("microphone stop trigger error\n");
		return;
	}

	/* print PCM stream */
#ifdef PCM_OUTPUT_IN_ASCII
	printk("-- start\n");
	int j;

	for (i = 0; i < NUM_MS; i++) {
		u16_t *pcm_out = rx_block[i];

		for (j = 0; j < rx_size/2; j++) {
			printk("0x%04x,\n", pcm_out[j]);
		}
	}
	printk("-- end\n");
#else
	unsigned char pcm_l, pcm_h;
	int j;

	for (i = 0; i < NUM_MS; i++) {
		u16_t *pcm_out = rx_block[i];

		for (j = 0; j < rx_size/2; j++) {
			pcm_l = (char)(pcm_out[j] & 0xFF);
			pcm_h = (char)((pcm_out[j] >> 8) & 0xFF);

			z_impl_k_str_out(&pcm_l, 1);
			z_impl_k_str_out(&pcm_h, 1);
		}
	}
#endif

	signal_print_stopped();
}
