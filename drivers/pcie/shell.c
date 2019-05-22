/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <shell/shell.h>
#include <drivers/pcie/pcie.h>

#ifdef CONFIG_PCIE_MSI
#include <drivers/pcie/msi.h>
#endif

#define MAX_BUS (0xFFFFFFFF & PCIE_BDF_BUS_MASK)
#define MAX_DEV (0xFFFFFFFF & PCIE_BDF_DEV_MASK)
#define MAX_FUNC (0xFFFFFFFF & PCIE_BDF_FUNC_MASK)

static void show_msi(const struct shell *shell, pcie_bdf_t bdf)
{
#ifdef CONFIG_PCIE_MSI
	u32_t msi;
	u32_t data;

	msi = pcie_get_cap(bdf, PCIE_MSI_CAP_ID);

	if (msi) {
		data = pcie_conf_read(bdf, msi + PCIE_MSI_MCR);
		shell_fprintf(shell, SHELL_NORMAL, "    MSI support%s%s\n",
			      (data & PCIE_MSI_MCR_64) ? ", 64-bit" : "",
			      (data & PCIE_MSI_MCR_EN) ? ", enabled" : "");
	}
#endif
}

static void show_bars(const struct shell *shell, pcie_bdf_t bdf)
{
	u32_t data;
	int bar;

	for (bar = PCIE_CONF_BAR0; bar <= PCIE_CONF_BAR5; ++bar) {
		data = pcie_conf_read(bdf, bar);
		if (data == PCIE_CONF_BAR_NONE) {
			continue;
		}

		shell_fprintf(shell, SHELL_NORMAL, "    bar %d: %s%s %x\n",
			      bar - PCIE_CONF_BAR0,
			      PCIE_CONF_BAR_IO(data) ? "I/O" : "MEM",
			      PCIE_CONF_BAR_64(data) ? ", 64-bit" : "",
			      PCIE_CONF_BAR_ADDR(data));

		if (PCIE_CONF_BAR_64(data)) {
			++bar;
		}
	}
}

static void show(const struct shell *shell, pcie_bdf_t bdf)
{
	u32_t data;
	unsigned int irq;

	data = pcie_conf_read(bdf, PCIE_CONF_ID);

	if (data == PCIE_ID_NONE) {
		return;
	}

	shell_fprintf(shell, SHELL_NORMAL, "%d:%x.%d ID %x:%x ",
		     PCIE_BDF_TO_BUS(bdf),
		     PCIE_BDF_TO_DEV(bdf),
		     PCIE_BDF_TO_FUNC(bdf),
		     PCIE_ID_TO_VEND(data),
		     PCIE_ID_TO_DEV(data));

	data = pcie_conf_read(bdf, PCIE_CONF_CLASSREV);
	shell_fprintf(shell, SHELL_NORMAL,
		     "class %x subclass %x prog i/f %x rev %x",
		     PCIE_CONF_CLASSREV_CLASS(data),
		     PCIE_CONF_CLASSREV_SUBCLASS(data),
		     PCIE_CONF_CLASSREV_PROGIF(data),
		     PCIE_CONF_CLASSREV_REV(data));

	data = pcie_conf_read(bdf, PCIE_CONF_TYPE);

	if (PCIE_CONF_TYPE_BRIDGE(data)) {
		shell_fprintf(shell, SHELL_NORMAL, " [bridge]\n");
	} else {
		shell_fprintf(shell, SHELL_NORMAL, "\n");
		show_bars(shell, bdf);
		show_msi(shell, bdf);
		irq = pcie_wired_irq(bdf);
		if (irq != PCIE_CONF_INTR_IRQ_NONE) {
			shell_fprintf(shell, SHELL_NORMAL,
				      "    wired interrupt on IRQ %d\n", irq);
		}
	}
}

static int cmd_lspcie(const struct shell *shell, size_t argc, char **argv)
{
	int bus;
	int dev;
	int func;

	for (bus = 0; bus <= MAX_BUS; ++bus) {
		for (dev = 0; dev <= MAX_DEV; ++dev) {
			for (func = 0; func <= MAX_FUNC; ++func) {
				show(shell, PCIE_BDF(bus, dev, func));
			}
		}
	}

	return 0;
}

SHELL_CMD_REGISTER(lspcie, NULL, "List PCI(e) devices", cmd_lspcie);
