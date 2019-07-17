/*
 * Copyright (c) 2015 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr.h>
#include <misc/printk.h>
#include <device.h>
#include <gpio.h>
#include <spi.h>

#define SPI_DEV  DT_SPI_1_NAME
#define GPIO_DEV DT_GPIO_P0_DEV_NAME

// These need to match SPI1 device tree definition for your board
#define PIN_CLK  SPI_1_SCK_PIN
#define PIN_DIN  SPI_1_MOSI_PIN
// Any GPIO pin that suits your design
#define PIN_CS   18

struct device *spi_dev;
struct device *gpio_dev;

struct spi_cs_control cs;

void spi_init(void)
{
    spi_dev = device_get_binding(SPI_DEV);
    if (!spi_dev)
    {
        printk("SPI: Device driver not found.\n");
    }
    else
    {
        printk("SPI device found\n");
    }

    gpio_pin_configure(gpio_dev, PIN_CLK, GPIO_DIR_OUT);
    gpio_pin_configure(gpio_dev, PIN_DIN, GPIO_DIR_OUT);
    gpio_pin_configure(gpio_dev, PIN_CS, GPIO_DIR_OUT);
}

struct spi_cs_control cs = {
    //.gpio_dev = gpio_dev,
    .gpio_pin = PIN_CS,
    .delay = 0,
};

struct spi_config spi_cfg = {
    .frequency = 1000000U,  // 1 MHz, but MAX7219 supports up to 10 MHz
    .operation = SPI_TRANSFER_MSB | SPI_WORD_SET(8) | SPI_CS_ACTIVE_HIGH,
    .slave = 0,
    .cs = &cs,
};

#define SPI_WORDS 2
uint8_t buffer_tx[SPI_WORDS];

const struct spi_buf tx_buf = {
    .buf = buffer_tx,
    .len = SPI_WORDS,
};

const struct spi_buf_set tx = {
    .buffers = &tx_buf,
    .count = 1
};

void max_send_byte(uint16_t cmd)
{
    uint8_t ret;
    buffer_tx[0] = (cmd >> 8);
    buffer_tx[1] = cmd & 0xff;
    ret = spi_write(spi_dev, &spi_cfg, &tx);
    if (ret != 0)
    {
        printk("SPI write failed %d\n", ret);
    }
}

void sendPatterns(int start)
{
    uint8_t val = 0x00;
    int lineNum;
    for (lineNum = 1; lineNum <= 8; ++lineNum)
    {
        if (lineNum == start)
        {
            switch (start)
            {
                case 1: case 8: val = 0x81; break;
                case 2: case 7: val = 0x42; break;
                case 3: case 6: val = 0x24; break;
                case 4: case 5: val = 0x18; break;
            }
        }
        else {
            val = 0x00;
        }
        max_send_byte((lineNum << 8) | val);
    }
    // Send one No-Op command in order to provide the device with some more clock signals
    // which are required to properly refresh the last display segment
    max_send_byte(0x00FF);
}

void main(void)
{
    int start;
    bool dir;

    gpio_dev = device_get_binding(GPIO_DEV);
    cs.gpio_dev = gpio_dev;
    if (!gpio_dev)
    {
        printk("GPIO: Device driver not found.\n");
    }
    else
    {
        printk("GPIO device found\n");
    }

    k_sleep(100);

    spi_init();
    uint16_t cmds[] = {
        0x0F00, /* disable test display mode */
        0x0900, /* disable decode */
        0x0A01, /* reduce brightness */
        0x0B07, /* set scan limit */
        0x0C01, /* leave shutdown mode */

        /* Blank initial screen state */
        0x0100,
        0x0200,
        0x0300,
        0x0400,
        0x0500,
        0x0600,
        0x0700,
        0x0800,
    };

    int cmdi;
    for (cmdi = 0; cmdi < sizeof(cmds)/sizeof(cmds[0]); ++cmdi)
    {
        max_send_byte(cmds[cmdi]);
    }

    start = 1;
    dir = true;
    printk("Animating...\n");

    while (1)
    {
        sendPatterns(start);
        if ((start == 1 && !dir) || (start == 8 && dir))
        {
            dir = !dir;
        }
        if (dir)
        {
            start++;
        }
        else
        {
            start--;
        }
        k_sleep(50);
    }

    printk("End.\n\n");
};

