/*
 * Copyright (c) 2015 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>

#define BUF_SZ 1024

static int pos;
char pk_console[BUF_SZ];

void __printk_hook_install(int (*fn)(int));
void *__printk_get_hook(void);
int (*_old_char_out)(int);

char *expected = "22 113 10000 32768 40000 22\n"
		 "p 112 -10000 -32768 -40000 -22\n"
		 "0xcafebabe 0x0000beef\n"
		 "0x1 0x01 0x0001 0x00000001 0x0000000000000001\n"
		 "0x1 0x 1 0x   1 0x       1\n"
		 "42 42 0042 00000042\n"
		 "-42 -42 -042 -0000042\n"
		 "42 42   42       42\n"
		 "42 42 0042 00000042\n"
		 "255     42    abcdef  0x0000002a      42\n"
		 "ERR -1 ERR ffffffffffffffff\n"
;


size_t stv = 22;
unsigned char uc = 'q';
unsigned short int usi = 10000U;
unsigned int ui = 32768U;
unsigned long ul = 40000;

/* FIXME
 * we know printk doesn't have full support for 64-bit values.
 * at least show it can print u64_t values less than 32-bits wide
 */
unsigned long long ull = 22;

char c = 'p';
signed short int ssi = -10000;
signed int si = -32768;
signed long sl = -40000;
signed long long sll = -22;

u32_t hex = 0xCAFEBABE;

void *ptr = (void *)0xBEEF;

static int ram_console_out(int character)
{
	pk_console[pos] = (char)character;
	pos = (pos + 1) % BUF_SZ;
	return _old_char_out(character);
}
/**
 * @addtogroup kernel_common_tests
 * @{
 */

/**
 * @brief Test printk() functionality
 *
 * @see printk(), __printk_get_hook(),
 * __printk_hook_install(), snprintk()
 *
 */
void test_printk(void)
{
	int count;

	_old_char_out = __printk_get_hook();
	__printk_hook_install(ram_console_out);

	printk("%zu %hhu %hu %u %lu %llu\n", stv, uc, usi, ui, ul, ull);
	printk("%c %hhd %hd %d %ld %lld\n", c, c, ssi, si, sl, sll);
	printk("0x%x %p\n", hex, ptr);
	printk("0x%x 0x%02x 0x%04x 0x%08x 0x%016x\n", 1, 1, 1, 1, 1);
	printk("0x%x 0x%2x 0x%4x 0x%8x\n", 1, 1, 1, 1);
	printk("%d %02d %04d %08d\n", 42, 42, 42, 42);
	printk("%d %02d %04d %08d\n", -42, -42, -42, -42);
	printk("%u %2u %4u %8u\n", 42, 42, 42, 42);
	printk("%u %02u %04u %08u\n", 42, 42, 42, 42);
	printk("%-8u%-6d%-4x%-2p%8d\n", 0xFF, 42, 0xABCDEF, (char *)42, 42);
	printk("%lld %lld %llu %llx\n", 0xFFFFFFFFFULL, -1LL, -1ULL, -1ULL);

	pk_console[pos] = '\0';
	zassert_true((strcmp(pk_console, expected) == 0), "printk failed");

	(void)memset(pk_console, 0, sizeof(pk_console));
	count = 0;

	count += snprintk(pk_console + count, sizeof(pk_console) - count,
			  "%zu %hhu %hu %u %lu %llu\n",
			  stv, uc, usi, ui, ul, ull);
	count += snprintk(pk_console + count, sizeof(pk_console) - count,
			  "%c %hhd %hd %d %ld %lld\n", c, c, ssi, si, sl, sll);
	count += snprintk(pk_console + count, sizeof(pk_console) - count,
			  "0x%x %p\n", hex, ptr);
	count += snprintk(pk_console + count, sizeof(pk_console) - count,
			  "0x%x 0x%02x 0x%04x 0x%08x 0x%016x\n", 1, 1, 1, 1, 1);
	count += snprintk(pk_console + count, sizeof(pk_console) - count,
			  "0x%x 0x%2x 0x%4x 0x%8x\n", 1, 1, 1, 1);
	count += snprintk(pk_console + count, sizeof(pk_console) - count,
			  "%d %02d %04d %08d\n", 42, 42, 42, 42);
	count += snprintk(pk_console + count, sizeof(pk_console) - count,
			  "%d %02d %04d %08d\n", -42, -42, -42, -42);
	count += snprintk(pk_console + count, sizeof(pk_console) - count,
			  "%u %2u %4u %8u\n", 42, 42, 42, 42);
	count += snprintk(pk_console + count, sizeof(pk_console) - count,
			  "%u %02u %04u %08u\n", 42, 42, 42, 42);
	count += snprintk(pk_console + count, sizeof(pk_console) - count,
			  "%-8u%-6d%-4x%-2p%8d\n",
			  0xFF, 42, 0xABCDEF, (char *)42, 42);
	count += snprintk(pk_console + count, sizeof(pk_console) - count,
			  "%lld %lld %llu %llx\n",
			  0xFFFFFFFFFULL, -1LL, -1ULL, -1ULL);
	pk_console[count] = '\0';
	zassert_true((strcmp(pk_console, expected) == 0), "snprintk failed");
}
/**
 * @}
 */
