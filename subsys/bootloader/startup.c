/*
 * Nordic Semiconductor ASA Copyright 2018
 * Free USE
 * Sigvart M. Hovland
 */

#include <stdint.h>

/* Linker-defined symbols for addresses of flash and ram */

//extern uint32_t __end_text;
//extern uint32_t __start_data;
//extern uint32_t __end_data; 
/* Block started by symbol, data container for statically allocated objects
 * Such as uninitialized objects both variables and constatns declared in 
 * file scope and uninitialized static local variables
 * Short name BETTER SAVE SPACE(BSS)
 */
extern uint32_t __bss_start;  //__start_bss;
extern uint32_t __bss_end;
extern uint32_t __data_rom_start;
extern uint32_t __data_ram_start;
extern uint32_t __data_ram_end;
extern uint32_t __image_text_start;
extern uint32_t __image_text_end;
extern uint32_t __start_stack;
extern uint32_t __end_stack;
extern uint32_t _main_stack;

extern uint32_t _image_rodata_start;
extern uint32_t _image_rodata_end;
extern uint32_t _image_rom_start;
extern uint32_t _image_rom_end;


/* C main function to be called from the reset_handler */
extern int main(void);

/* Device specific intialization functions for erratas and system clock setup
 */
extern void system_init(void);

/* Forward decleration for dummy handler */
void dummy_handler(void);
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
extern uint32_t __kernel_ram_start;
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
/*
_image_text_end == _end_text
__data_rom_start == _start_data
__data_ram_end == _end_data
*/


void _bss_zero(register uint32_t *dest, uint32_t *end)
{
	while(dest < &end)
	{
		*dest++ = 0;
	}
	/* Alternative implementation
	 * size = (uint32_t)&__bss_end - (uint32_t)&__bss_start
	 * for(;dest < &end; dest++)
	 * {
	 * 	*dest = 0;
	 * }
	 */
}

void _data_copy( register uint32_t *src, register uint32_t *dest, uint32_t *end)
{
	while(dest < &end)
	{
		*dest++ = *src++;
	}
}

extern uint32_t _image_text_end;
void reset_handler(void)
{
	_bss_zero(&__bss_start, &__bss_end);
	//maybe __data_ram_start instead of image_text_end
	_data_copy(&_image_text_end, &__data_ram_start, &__data_ram_end);
	//register uint32_t *src, *dst;
	//src = &_image_text_end;
	//dst = &__data_ram_start;
	//while(dst < &__data_ram_end)
	//{
	//	*dst++ = *src++;
	//}
	
	#ifdef SYSTEM_INIT
	system_init();	
	#endif
	main();
	while(1);
}
void __start(void){
	reset_handler();
}

void dummy_handler(void)
{
	/* Hang on unexpected interrupts as it's considered a bug in the program */
	while(1);
}
