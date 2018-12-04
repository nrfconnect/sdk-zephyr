/*
 * Copyright (c) 2018, Christian Taedcke, Diego Sueiro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <uart.h>
#include <em_leuart.h>
#include <em_gpio.h>
#include <em_cmu.h>
#include <soc.h>

#define DEV_CFG(dev) \
	((const struct leuart_gecko_config * const)(dev)->config->config_info)
#define DEV_DATA(dev) \
	((struct leuart_gecko_data * const)(dev)->driver_data)
#define DEV_BASE(dev) \
	((LEUART_TypeDef *)(DEV_CFG(dev))->base)

struct leuart_gecko_config {
	LEUART_TypeDef *base;
	CMU_Clock_TypeDef clock;
	u32_t baud_rate;
	struct soc_gpio_pin pin_rx;
	struct soc_gpio_pin pin_tx;
	unsigned int loc;
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	void (*irq_config_func)(struct device *dev);
#endif
};

struct leuart_gecko_data {
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	uart_irq_callback_user_data_t callback;
	void *cb_data;
#endif
};

static int leuart_gecko_poll_in(struct device *dev, unsigned char *c)
{
	LEUART_TypeDef *base = DEV_BASE(dev);
	u32_t flags = LEUART_StatusGet(base);

	if (flags & LEUART_STATUS_RXDATAV) {
		*c = LEUART_Rx(base);
		return 0;
	}

	return -1;
}

static void leuart_gecko_poll_out(struct device *dev, unsigned char c)
{
	LEUART_TypeDef *base = DEV_BASE(dev);

	/* LEUART_Tx function already waits for the transmit buffer being empty
	 * and and waits for the bus to be free to transmit.
	 */
	LEUART_Tx(base, c);
}

static int leuart_gecko_err_check(struct device *dev)
{
	LEUART_TypeDef *base = DEV_BASE(dev);
	u32_t flags = LEUART_IntGet(base);
	int err = 0;

	if (flags & LEUART_IF_RXOF) {
		err |= UART_ERROR_OVERRUN;
	}

	if (flags & LEUART_IF_PERR) {
		err |= UART_ERROR_PARITY;
	}

	if (flags & LEUART_IF_FERR) {
		err |= UART_ERROR_FRAMING;
	}

	LEUART_IntClear(base, LEUART_IF_RXOF |
		       LEUART_IF_PERR |
		       LEUART_IF_FERR);

	return err;
}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
static int leuart_gecko_fifo_fill(struct device *dev, const u8_t *tx_data,
			       int len)
{
	LEUART_TypeDef *base = DEV_BASE(dev);
	u8_t num_tx = 0;

	while ((len - num_tx > 0) &&
	       (base->STATUS & LEUART_STATUS_TXBL)) {

		base->TXDATA = (u32_t)tx_data[num_tx++];
	}

	return num_tx;
}

static int leuart_gecko_fifo_read(struct device *dev, u8_t *rx_data,
			       const int len)
{
	LEUART_TypeDef *base = DEV_BASE(dev);
	u8_t num_rx = 0;

	while ((len - num_rx > 0) &&
	       (base->STATUS & LEUART_STATUS_RXDATAV)) {

		rx_data[num_rx++] = (u8_t)base->RXDATA;
	}

	return num_rx;
}

static void leuart_gecko_irq_tx_enable(struct device *dev)
{
	LEUART_TypeDef *base = DEV_BASE(dev);
	u32_t mask = LEUART_IEN_TXBL | LEUART_IEN_TXC;

	LEUART_IntEnable(base, mask);
}

static void leuart_gecko_irq_tx_disable(struct device *dev)
{
	LEUART_TypeDef *base = DEV_BASE(dev);
	u32_t mask = LEUART_IEN_TXBL | LEUART_IEN_TXC;

	LEUART_IntDisable(base, mask);
}

static int leuart_gecko_irq_tx_complete(struct device *dev)
{
	LEUART_TypeDef *base = DEV_BASE(dev);
	u32_t flags = LEUART_IntGet(base);

	return (flags & LEUART_IF_TXC) != 0;
}

static int leuart_gecko_irq_tx_ready(struct device *dev)
{
	LEUART_TypeDef *base = DEV_BASE(dev);
	u32_t flags = LEUART_IntGet(base);

	return (flags & LEUART_IF_TXBL) != 0;
}

static void leuart_gecko_irq_rx_enable(struct device *dev)
{
	LEUART_TypeDef *base = DEV_BASE(dev);
	u32_t mask = LEUART_IEN_RXDATAV;

	LEUART_IntEnable(base, mask);
}

static void leuart_gecko_irq_rx_disable(struct device *dev)
{
	LEUART_TypeDef *base = DEV_BASE(dev);
	u32_t mask = LEUART_IEN_RXDATAV;

	LEUART_IntDisable(base, mask);
}

static int leuart_gecko_irq_rx_full(struct device *dev)
{
	LEUART_TypeDef *base = DEV_BASE(dev);
	u32_t flags = LEUART_IntGet(base);

	return (flags & LEUART_IF_RXDATAV) != 0;
}

static int leuart_gecko_irq_rx_ready(struct device *dev)
{
	LEUART_TypeDef *base = DEV_BASE(dev);
	u32_t mask = LEUART_IEN_RXDATAV;

	return (base->IEN & mask)
		&& leuart_gecko_irq_rx_full(dev);
}

static void leuart_gecko_irq_err_enable(struct device *dev)
{
	LEUART_TypeDef *base = DEV_BASE(dev);

	LEUART_IntEnable(base, LEUART_IF_RXOF |
			 LEUART_IF_PERR |
			 LEUART_IF_FERR);
}

static void leuart_gecko_irq_err_disable(struct device *dev)
{
	LEUART_TypeDef *base = DEV_BASE(dev);

	LEUART_IntDisable(base, LEUART_IF_RXOF |
			 LEUART_IF_PERR |
			 LEUART_IF_FERR);
}

static int leuart_gecko_irq_is_pending(struct device *dev)
{
	return leuart_gecko_irq_tx_ready(dev) || leuart_gecko_irq_rx_ready(dev);
}

static int leuart_gecko_irq_update(struct device *dev)
{
	return 1;
}

static void leuart_gecko_irq_callback_set(struct device *dev,
					  uart_irq_callback_user_data_t cb,
					  void *cb_data)
{
	struct leuart_gecko_data *data = dev->driver_data;

	data->callback = cb;
	data->cb_data = cb_data;
}

static void leuart_gecko_isr(void *arg)
{
	struct device *dev = arg;
	struct leuart_gecko_data *data = dev->driver_data;

	if (data->callback) {
		data->callback(data->cb_data);
	}
}
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

static void leuart_gecko_init_pins(struct device *dev)
{
	const struct leuart_gecko_config *config = DEV_CFG(dev);
	LEUART_TypeDef *base = DEV_BASE(dev);

	soc_gpio_configure(&config->pin_rx);
	soc_gpio_configure(&config->pin_tx);

#ifdef _LEUART_ROUTEPEN_MASK
	base->ROUTEPEN = LEUART_ROUTEPEN_RXPEN | LEUART_ROUTEPEN_TXPEN;
	base->ROUTELOC0 = (base->ROUTELOC0
				   & ~(_LEUART_ROUTELOC0_TXLOC_MASK
				   | _LEUART_ROUTELOC0_RXLOC_MASK))
				   | (config->loc <<
				      _LEUART_ROUTELOC0_TXLOC_SHIFT)
				   | (config->loc <<
				      _LEUART_ROUTELOC0_RXLOC_SHIFT);
#else
	base->ROUTE = LEUART_ROUTE_RXPEN | LEUART_ROUTE_TXPEN
			      | (config->loc << 8);
#endif
}

static int leuart_gecko_init(struct device *dev)
{
	const struct leuart_gecko_config *config = DEV_CFG(dev);
	LEUART_TypeDef *base = DEV_BASE(dev);
	LEUART_Init_TypeDef leuartInit = LEUART_INIT_DEFAULT;

	/* The peripheral and gpio clock are already enabled from soc and gpio
	 * driver
	 */

	leuartInit.baudrate = config->baud_rate;

	/* Enable LEUART clock */
	CMU_ClockEnable(config->clock, true);

	/* Enable CORE LE clock in order to access LE modules */
	CMU_ClockEnable(cmuClock_CORELE, true);

	/* Select LFXO for LEUARTs (and wait for it to stabilize) */
	CMU_ClockSelectSet(cmuClock_LFB, cmuSelect_LFXO);

	/* Init LEUART */
	LEUART_Init(base, &leuartInit);

	/* Initialize LEUART pins */
	leuart_gecko_init_pins(dev);

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	config->irq_config_func(dev);
#endif

	return 0;
}

static const struct uart_driver_api leuart_gecko_driver_api = {
	.poll_in = leuart_gecko_poll_in,
	.poll_out = leuart_gecko_poll_out,
	.err_check = leuart_gecko_err_check,
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	.fifo_fill = leuart_gecko_fifo_fill,
	.fifo_read = leuart_gecko_fifo_read,
	.irq_tx_enable = leuart_gecko_irq_tx_enable,
	.irq_tx_disable = leuart_gecko_irq_tx_disable,
	.irq_tx_complete = leuart_gecko_irq_tx_complete,
	.irq_tx_ready = leuart_gecko_irq_tx_ready,
	.irq_rx_enable = leuart_gecko_irq_rx_enable,
	.irq_rx_disable = leuart_gecko_irq_rx_disable,
	.irq_rx_ready = leuart_gecko_irq_rx_ready,
	.irq_err_enable = leuart_gecko_irq_err_enable,
	.irq_err_disable = leuart_gecko_irq_err_disable,
	.irq_is_pending = leuart_gecko_irq_is_pending,
	.irq_update = leuart_gecko_irq_update,
	.irq_callback_set = leuart_gecko_irq_callback_set,
#endif
};

#ifdef CONFIG_LEUART_GECKO_0

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
static void leuart_gecko_config_func_0(struct device *dev);
#endif

static const struct leuart_gecko_config leuart_gecko_0_config = {
	.base = (LEUART_TypeDef *)DT_LEUART_GECKO_0_BASE_ADDRESS,
	.clock = cmuClock_LEUART0,
	.baud_rate = DT_LEUART_GECKO_0_CURRENT_SPEED,
	.pin_rx = PIN_LEUART0_RXD,
	.pin_tx = PIN_LEUART0_TXD,
	.loc = DT_LEUART_GECKO_0_LOCATION,
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	.irq_config_func = leuart_gecko_config_func_0,
#endif
};

static struct leuart_gecko_data leuart_gecko_0_data;

DEVICE_AND_API_INIT(leuart_0, DT_LEUART_GECKO_0_LABEL, &leuart_gecko_init,
		    &leuart_gecko_0_data, &leuart_gecko_0_config, PRE_KERNEL_1,
		    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &leuart_gecko_driver_api);

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
static void leuart_gecko_config_func_0(struct device *dev)
{
	IRQ_CONNECT(DT_LEUART_GECKO_0_IRQ,
		    DT_LEUART_GECKO_0_IRQ_PRIORITY, leuart_gecko_isr,
		    DEVICE_GET(leuart_0), 0);

	irq_enable(DT_LEUART_GECKO_0_IRQ);
}
#endif

#endif /* CONFIG_LEUART_GECKO_0 */

#ifdef CONFIG_LEUART_GECKO_1

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
static void leuart_gecko_config_func_1(struct device *dev);
#endif

static const struct leuart_gecko_config leuart_gecko_1_config = {
	.base = (LEUART_TypeDef *)CONFIG_LEUART_GECKO_1_BASE_ADDRESS,
	.clock = cmuClock_LEUART0,
	.baud_rate = CONFIG_LEUART_GECKO_1_CURRENT_SPEED,
	.pin_rx = PIN_LEUART1_RXD,
	.pin_tx = PIN_LEUART1_TXD,
	.loc = CONFIG_LEUART_GECKO_1_LOCATION,
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	.irq_config_func = leuart_gecko_config_func_1,
#endif
};

static struct leuart_gecko_data leuart_gecko_1_data;

DEVICE_AND_API_INIT(leuart_1, CONFIG_LEUART_GECKO_1_LABEL, &leuart_gecko_init,
		    &leuart_gecko_1_data, &leuart_gecko_1_config, PRE_KERNEL_1,
		    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &leuart_gecko_driver_api);

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
static void leuart_gecko_config_func_1(struct device *dev)
{
	IRQ_CONNECT(CONFIG_LEUART_GECKO_1_IRQ,
		    CONFIG_LEUART_GECKO_1_IRQ_PRIORITY, leuart_gecko_isr,
		    DEVICE_GET(leuart_1), 0);

	irq_enable(CONFIG_LEUART_GECKO_1_IRQ);
}
#endif

#endif /* CONFIG_LEUART_GECKO_1 */
