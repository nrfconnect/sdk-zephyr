/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file Sample app to demonstrate PWM.
 *
 * This app uses PWM pins 0, 1, and 2.
 */

#include <zephyr.h>
#include <misc/printk.h>
#include <device.h>
#include <pwm.h>

#if defined(RED_PWM_LED_PWM_CONTROLLER) && \
      defined(RED_PWM_LED_PWM_CHANNEL) && \
      defined(GREEN_PWM_LED_PWM_CONTROLLER) && \
      defined(GREEN_PWM_LED_PWM_CHANNEL) && \
      defined(BLUE_PWM_LED_PWM_CONTROLLER) && \
      defined(BLUE_PWM_LED_PWM_CHANNEL)
/* Get the defines from dt (based on aliases 'red-pwm-led', 'green-pwm-led' &
 * 'blue-pwm-led')
 */
#define PWM_DEV0	RED_PWM_LED_PWM_CONTROLLER
#define PWM_CH0		RED_PWM_LED_PWM_CHANNEL
#define PWM_DEV1	GREEN_PWM_LED_PWM_CONTROLLER
#define PWM_CH1		GREEN_PWM_LED_PWM_CHANNEL
#define PWM_DEV2	BLUE_PWM_LED_PWM_CONTROLLER
#define PWM_CH2		BLUE_PWM_LED_PWM_CHANNEL
#else
#error "Choose supported board or add new board for the application"
#endif

/*
 * 50 is flicker fusion threshold. Modulated light will be perceived
 * as steady by our eyes when blinking rate is at least 50.
 */
#define PERIOD (USEC_PER_SEC / 50U)

/* in micro second */
#define STEPSIZE	2000

static int write_pin(struct device *pwm_dev, u32_t pwm_pin,
		     u32_t pulse_width)
{
	return pwm_pin_set_usec(pwm_dev, pwm_pin, PERIOD, pulse_width);
}

void main(void)
{
	struct device *pwm_dev[3];
	u32_t pulse_width0, pulse_width1, pulse_width2;

	printk("PWM demo app-RGB LED\n");

	pwm_dev[0] = device_get_binding(PWM_DEV0);
	pwm_dev[1] = device_get_binding(PWM_DEV1);
	pwm_dev[2] = device_get_binding(PWM_DEV2);
	if (!pwm_dev[0] || !pwm_dev[1] || !pwm_dev[2]) {
		printk("Cannot find PWM device!\n");
		return;
	}

	while (1) {
		for (pulse_width0 = 0U; pulse_width0 <= PERIOD;
		     pulse_width0 += STEPSIZE) {
			if (write_pin(pwm_dev[0], PWM_CH0,
				      pulse_width0) != 0) {
				printk("pin 0 write fails!\n");
				return;
			}

			for (pulse_width1 = 0U; pulse_width1 <= PERIOD;
			     pulse_width1 += STEPSIZE) {
				if (write_pin(pwm_dev[1], PWM_CH1,
					      pulse_width1) != 0) {
					printk("pin 1 write fails!\n");
					return;
				}

				for (pulse_width2 = 0U; pulse_width2 <= PERIOD;
				     pulse_width2 += STEPSIZE) {
					if (write_pin(pwm_dev[2], PWM_CH2,
						      pulse_width2) != 0) {
						printk("pin 2 write fails!\n");
						return;
					}
					k_sleep(MSEC_PER_SEC);
				}
			}
		}
	}
}
