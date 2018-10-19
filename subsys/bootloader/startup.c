/*
 * Nordic Semiconductor ASA Copyright 2018
 * Sigvart M. Hovland
 */

/* General TODO: put stack after .bss and .data
 * +--------------+
 * | .bss + .data |
 * |--------------|
 * |	.stack	  |
 * |      |       |
 * |	  V		  |
 * |--------------|
 * | 	 End      |
 * +--------------+
 * And upwards growing heap if heap is present ontop of .bss and .data
 * This gives you MMU less stack protection for both .heap and .stack
 * +--------------+
 * |--------------|
 * |	  ^		  |
 * |	  |       |
 * |	.heap	  |
 * |--------------|
 * | .bss + .data |
 * |--------------|
 * |	.stack	  |
 * |      |       |
 * |	  V		  |
 * |--------------|
 * | 	 End      |
 * +--------------+
 * MMU Less stack and heap protection, but limited heap size
 * You can't resize the stack to account for more heap etc. so this
 * should be configurable. As you may  want the heap to grow towards
 * the stack for for small devices with low memory.
 */

#include <stdint.h>

/* Linker-defined symbols for addresses of flash and ram */

/* Block started by symbol, data container for statically allocated objects
 * Such as uninitialized objects both variables and constatns declared in
 * file scope and uninitialized static local variables
 * Short name BETTER SAVE SPACE(BSS)
 */
extern uint32_t __bss_start;
extern uint32_t __bss_end;
extern uint32_t __data_rom_start;
extern uint32_t __data_ram_start;
extern uint32_t __data_ram_end;
extern uint32_t _image_text_end;

/* C main function to be called from the reset_handler */
extern int main(void);

/* Device specific intialization functions for erratas and system clock setup */
extern void SystemInit(void);
/* Forward decleration for dummy handler */
void dummy_handler(void);

/* weak assign all interrupt handlers to dummy_handler */
#define ALIAS(name) __attribute__((weak, alias (name)))
void reset_handler(void);
void nmi_handler(void) ALIAS("dummy_handler");
void hard_fault_handler(void) ALIAS("dummy_handler");
#if defined(CONFIG_ARMV7_M_ARMV8_M_MAINLINE)
void mpu_fault_handler(void) ALIAS("dummy_handler");
void bus_fault_handler(void) ALIAS("dummy_handler");
void usage_fault_handler(void) ALIAS("dummy_handler");
void debug_monitor_handler(void) ALIAS("dummy_handler");
#if defined(CONFIG_ARM_SECURE_FIRMWARE)
void secure_fault_handler(void) ALIAS("dummy_handler");
#endif /* CONFIG_ARM_SECURE_FIRMWARE */
#endif /* CONFIG_ARMV7_M_ARMV8_M_MAINLINE */
void svc_handler(void) ALIAS("dummy_handler");
void pend_sv_handler(void) ALIAS("dummy_handler");
void sys_tick_handler(void) ALIAS("dummy_handler");


extern uint32_t __kernel_ram_start; //TODO: Find _end_of_stack symbol

/* TODO: Add vendor specific vectors to vector table */
void *core_vector_table[16] __attribute__((section(".exc_vector_table"))) = {
	&__kernel_ram_start + CONFIG_MAIN_STACK_SIZE,
	reset_handler, //__reset
	nmi_handler, // __nmi
	hard_fault_handler, //__hard_fault
#if defined(CONFIG_ARMV6_M_ARMV8_M_BASELINE)
	0, /* reserved */ //__reserved
	0, /* reserved */
	0, /* reserved */
	0, /* reserved */
	0, /* reserved */
	0, /* reserved */
	0, /* reserved */
	svc_handler, //__svc
	0, /* reserved */
#elif defined(CONFIG_ARMV7_M_ARMV8_M_MAINLINE)
	mpu_fault_handler, //__mpu_fault
	bus_fault_handler, //__bus_fault
	usage_fault_handler, //__usage_fault
#if defined(CONFIG_ARM_SECURE_FIRMWARE)
	secure_fault_handler, //__secure_fault
#else
	0, /* reserved */
#endif /* CONFIG_ARM_SECURE_FIRMWARE */
	0, /* reserved */
	0, /* reserved */
	0, /* reserved */
	svc_handler, //__svc
	debug_monitor_handler, //debug_monitor
#else /* ARM_ARCHITECTURE */
#error Unknown ARM architecture
#endif /* CONFIG_ARMV6_M_ARMV8_M_BASELINE */
	0, /* reserved */
	pend_sv_handler, //__pendsv
#if defined(CONFIG_CORTEX_M_SYSTICK)
	sys_tick_handler, //_timer_int_handler
#else
	0, /* reserved */
#endif /* CONFIG_CORTEX_M_SYSTICK */
};

void _bss_zero(uint32_t *dest, uint32_t *end)
{
	while(dest < (uint32_t *) &end)
	{
		*dest++ = 0;
	}
}

void _data_copy(uint32_t *src, uint32_t *dest, uint32_t *end)
{
	while(dest < (uint32_t *) &end)
	{
		*dest++ = *src++;
	}
}

void reset_handler(void)
{
	_bss_zero(&__bss_start, &__bss_end);
	_data_copy(&_image_text_end, &__data_ram_start, &__data_ram_end);
	#if defined(CONFIG_SECURE_BOOT_SYSTEM_INIT)
	SystemInit(); /* Create define for system INIT */
	#endif /* CONFIG_SECURE_BOOT TODO: Find a way to use select defines? */
	main();
	while(1);
}

/* TODO: Find a way to redefine the entry point from __start to reset handler? */
void __start(void){
	reset_handler();
}

void dummy_handler(void)
{
	/* Hang on unexpected interrupts as it's considered a bug in the program */
	while(1);
}
