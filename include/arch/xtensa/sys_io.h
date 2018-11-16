/*
 * Copyright (c) 2016 Cadence Design Systems, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_ARCH_XTENSA_SYS_IO_H_
#define ZEPHYR_INCLUDE_ARCH_XTENSA_SYS_IO_H_

#if !defined(_ASMLANGUAGE)

#include <sys_io.h>

/* Memory mapped registers I/O functions */

static ALWAYS_INLINE u32_t sys_read32(mem_addr_t addr)
{
	return *(volatile u32_t *)addr;
}

static ALWAYS_INLINE void sys_write32(u32_t data, mem_addr_t addr)
{
	*(volatile u32_t *)addr = data;
}

static ALWAYS_INLINE u16_t sys_read16(mem_addr_t addr)
{
	return *(volatile u16_t *)addr;
}

static ALWAYS_INLINE void sys_write16(u16_t data, mem_addr_t addr)
{
	*(volatile u16_t *)addr = data;
}


static ALWAYS_INLINE u8_t sys_read8(mem_addr_t addr)
{
	return *(volatile u8_t *)addr;
}

static ALWAYS_INLINE void sys_write8(u8_t data, mem_addr_t addr)
{
	*(volatile u8_t *)addr = data;
}

/* Memory bit manipulation functions */

static ALWAYS_INLINE void sys_set_bit(mem_addr_t addr, unsigned int bit)
{
	u32_t temp = *(volatile u32_t *)addr;

	*(volatile u32_t *)addr = temp | (1 << bit);
}

static ALWAYS_INLINE void sys_clear_bit(mem_addr_t addr, unsigned int bit)
{
	u32_t temp = *(volatile u32_t *)addr;

	*(volatile u32_t *)addr = temp & ~(1 << bit);
}

static ALWAYS_INLINE int sys_test_bit(mem_addr_t addr, unsigned int bit)
{
	int temp = *(volatile int *)addr;

	return (int)(temp & (1 << bit));
}

static ALWAYS_INLINE int sys_test_and_set_bit(mem_addr_t addr, unsigned int bit)
{
	int retval = (*(volatile int *)addr) & (1 << bit);
	*(volatile int *)addr = (*(volatile int *)addr) | (1 << bit);
	return retval;
}

static ALWAYS_INLINE
	int sys_test_and_clear_bit(mem_addr_t addr, unsigned int bit)
{
	int retval = (*(volatile int *)addr) & (1 << bit);
	*(volatile int *)addr = (*(volatile int *)addr) & ~(1 << bit);
	return retval;
}

static ALWAYS_INLINE
	void sys_bitfield_set_bit(mem_addr_t addr, unsigned int bit)
{
	/* Doing memory offsets in terms of 32-bit values to prevent
	 * alignment issues
	 */
	sys_set_bit(addr + ((bit >> 5) << 2), bit & 0x1F);
}

static ALWAYS_INLINE
	void sys_bitfield_clear_bit(mem_addr_t addr, unsigned int bit)
{
	sys_clear_bit(addr + ((bit >> 5) << 2), bit & 0x1F);
}

static ALWAYS_INLINE
	int sys_bitfield_test_bit(mem_addr_t addr, unsigned int bit)
{
	return sys_test_bit(addr + ((bit >> 5) << 2), bit & 0x1F);
}


static ALWAYS_INLINE
	int sys_bitfield_test_and_set_bit(mem_addr_t addr, unsigned int bit)
{
	int ret;

	ret = sys_bitfield_test_bit(addr, bit);
	sys_bitfield_set_bit(addr, bit);

	return ret;
}

static ALWAYS_INLINE
	int sys_bitfield_test_and_clear_bit(mem_addr_t addr, unsigned int bit)
{
	int ret;

	ret = sys_bitfield_test_bit(addr, bit);
	sys_bitfield_clear_bit(addr, bit);

	return ret;
}
#endif /* !_ASMLANGUAGE */


#endif /* ZEPHYR_INCLUDE_ARCH_XTENSA_SYS_IO_H_ */
