/*
 * Copyright (c) 2018 Linaro Limited.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <uart.h>
#include <misc/printk.h>
#include <tty.h>
#include <drivers/console/console.h>
#include <drivers/console/uart_console.h>

static int tty_irq_input_hook(struct tty_serial *tty, u8_t c);
static int tty_putchar(struct tty_serial *tty, u8_t c);

static void tty_uart_isr(void *user_data)
{
	struct tty_serial *tty = user_data;
	struct device *dev = tty->uart_dev;

	uart_irq_update(dev);

	if (uart_irq_rx_ready(dev)) {
		u8_t c;

		while (1) {
			if (uart_fifo_read(dev, &c, 1) == 0) {
				break;
			}
			tty_irq_input_hook(tty, c);
		}
	}

	if (uart_irq_tx_ready(dev)) {
		if (tty->tx_get == tty->tx_put) {
			/* Output buffer empty, don't bother
			 * us with tx interrupts
			 */
			uart_irq_tx_disable(dev);
		} else {
			uart_fifo_fill(dev, &tty->tx_ringbuf[tty->tx_get++], 1);
			if (tty->tx_get >= tty->tx_ringbuf_sz) {
				tty->tx_get = 0;
			}
			k_sem_give(&tty->tx_sem);
		}
	}
}

static int tty_irq_input_hook(struct tty_serial *tty, u8_t c)
{
	int rx_next = tty->rx_put + 1;

	if (rx_next >= tty->rx_ringbuf_sz) {
		rx_next = 0;
	}

	if (rx_next == tty->rx_get) {
		/* Try to give a clue to user that some input was lost */
		tty_putchar(tty, '~');
		return 1;
	}

	tty->rx_ringbuf[tty->rx_put] = c;
	tty->rx_put = rx_next;
	k_sem_give(&tty->rx_sem);

	return 1;
}

static int tty_putchar(struct tty_serial *tty, u8_t c)
{
	unsigned int key;
	int tx_next;
	int res;

	res = k_sem_take(&tty->tx_sem, tty->tx_timeout);
	if (res < 0) {
		return res;
	}

	key = irq_lock();
	tx_next = tty->tx_put + 1;
	if (tx_next >= tty->tx_ringbuf_sz) {
		tx_next = 0;
	}
	if (tx_next == tty->tx_get) {
		irq_unlock(key);
		return -ENOSPC;
	}

	tty->tx_ringbuf[tty->tx_put] = c;
	tty->tx_put = tx_next;

	irq_unlock(key);
	uart_irq_tx_enable(tty->uart_dev);
	return 0;
}

ssize_t tty_write(struct tty_serial *tty, const void *buf, size_t size)
{
	const u8_t *p = buf;
	size_t out_size = 0;
	int res = 0;

	if (tty->tx_ringbuf_sz == 0) {
		/* Unbuffered operation, implicitly blocking. */
		out_size = size;

		while (size--) {
			uart_poll_out(tty->uart_dev, *p++);
		}

		return out_size;
	}

	while (size--) {
		res = tty_putchar(tty, *p++);
		if (res < 0) {
			/* If we didn't transmit anything, return the error. */
			if (out_size == 0) {
				errno = -res;
				return res;
			}

			/*
			 * Otherwise, return how much we transmitted. If error
			 * was transient (like EAGAIN), on next call user might
			 * not even get it. And if it's non-transient, they'll
			 * get it on the next call.
			 */
			return out_size;
		}

		out_size++;
	}

	return out_size;
}

static int tty_getchar(struct tty_serial *tty)
{
	unsigned int key;
	u8_t c;
	int res;

	res = k_sem_take(&tty->rx_sem, tty->rx_timeout);
	if (res < 0) {
		return res;
	}

	key = irq_lock();
	c = tty->rx_ringbuf[tty->rx_get++];
	if (tty->rx_get >= tty->rx_ringbuf_sz) {
		tty->rx_get = 0;
	}
	irq_unlock(key);

	return c;
}

static ssize_t tty_read_unbuf(struct tty_serial *tty, void *buf, size_t size)
{
	u8_t *p = buf;
	size_t out_size = 0;
	int res = 0;
	u32_t timeout = tty->rx_timeout;

	while (size) {
		u8_t c;
		res = uart_poll_in(tty->uart_dev, &c);
		if (res <= -2) {
			/* Error occured, best we can do is to return
			 * accumulated data w/o error, or return error
			 * directly if none.
			 */
			if (out_size == 0) {
				errno = res;
				return -1;
			}
			break;
		}

		if (res == 0) {
			*p++ = c;
			out_size++;
			size--;
		}

		if (size == 0 || (timeout != K_FOREVER && timeout-- == 0)) {
			break;
		}

		/* Avoid 100% busy-polling, and yet try to process bursts
		 * of data without extra delays.
		 */
		if (res == -1) {
			k_sleep(1);
		}
	}

	return out_size;
}

ssize_t tty_read(struct tty_serial *tty, void *buf, size_t size)
{
	u8_t *p = buf;
	size_t out_size = 0;
	int res = 0;

	if (tty->rx_ringbuf_sz == 0) {
		return tty_read_unbuf(tty, buf, size);
	}

	while (size--) {
		res = tty_getchar(tty);
		if (res < 0) {
			/* If we didn't transmit anything, return the error. */
			if (out_size == 0) {
				errno = -res;
				return res;
			}

			/*
			 * Otherwise, return how much we transmitted. If error
			 * was transient (like EAGAIN), on next call user might
			 * not even get it. And if it's non-transient, they'll
			 * get it on the next call.
			 */
			return out_size;
		}

		*p++ = (u8_t)res;
		out_size++;
	}

	return out_size;
}

int tty_init(struct tty_serial *tty, struct device *uart_dev)
{
	tty->uart_dev = uart_dev;

	/* We start in unbuffer mode. */
	tty->rx_ringbuf = NULL;
	tty->rx_ringbuf_sz = 0;
	tty->tx_ringbuf = NULL;
	tty->tx_ringbuf_sz = 0;

	tty->rx_get = tty->rx_put = tty->tx_get = tty->tx_put = 0;

	tty->rx_timeout = K_FOREVER;
	tty->tx_timeout = K_FOREVER;

	uart_irq_callback_user_data_set(uart_dev, tty_uart_isr, tty);

	return 0;
}

int tty_set_rx_buf(struct tty_serial *tty, void *buf, size_t size)
{
	uart_irq_rx_disable(tty->uart_dev);

	tty->rx_ringbuf = buf;
	tty->rx_ringbuf_sz = size;

	if (size > 0) {
		k_sem_init(&tty->rx_sem, 0, UINT_MAX);
		uart_irq_rx_enable(tty->uart_dev);
	}

	return 0;
}

int tty_set_tx_buf(struct tty_serial *tty, void *buf, size_t size)
{
	uart_irq_tx_disable(tty->uart_dev);

	tty->tx_ringbuf = buf;
	tty->tx_ringbuf_sz = size;

	k_sem_init(&tty->tx_sem, size - 1, UINT_MAX);

	/* New buffer is initially empty, no need to re-enable interrupts,
	 * it will be done when needed (on first output char).
	 */

	return 0;
}
