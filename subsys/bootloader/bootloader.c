#include <stdint.h>
#include <nrf.h>

#define LED1_GPIO (13UL)
#define LED2_GPIO (14UL)
#define LED4_GPIO (16UL)
#define BUTTON1_GPIO (11UL)
#define BUTTON2_GPIO (12UL)

#define EnablePrivilegedMode() __asm("SVC #0")

void config_led(uint32_t pin_num){
	NRF_GPIO->PIN_CNF[pin_num] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
		(GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
		(GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
		(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
		(GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
}

void config_input(uint32_t pin_num){
	NRF_GPIO->PIN_CNF[pin_num] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
		(GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
		(GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
		(GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
		(GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
}

static void boot_from(uint32_t *address)
{
	if(CONTROL_nPRIV_Msk & __get_CONTROL())
	{
		EnablePrivilegedMode();
	}
	//__disable_irq();
	for(uint8_t i = 0; i < 8; i++) NVIC->ICER[i] = 0xFFFFFFFF;
	for(uint8_t i = 0; i < 8; i++) NVIC->ICPR[i] = 0xFFFFFFFF;
	SysTick->CTRL = 0;
	SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;
	SCB->SHCSR &= ~(SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_MEMFAULTENA_Msk );
	if(CONTROL_SPSEL_Msk & __get_CONTROL())
	{
		__set_CONTROL(__get_CONTROL() & ~CONTROL_SPSEL_Msk);
	}
	//__DSB(); /* Force Memory Write before continuing */
	//__ISB(); /* Flush and refill pipeline with updated premissions */
	SCB->VTOR = (uint32_t) address;
	//__enable_irq();
	__set_MSP(address[0]);
	((void(*)(void))address[1])( ) ;
}



int main(void)
{
	//boot_from((uint32_t *) 0x00008000);
	//
	config_led(LED1_GPIO);
	config_led(LED2_GPIO);
	config_led(LED4_GPIO);
	config_input(BUTTON1_GPIO);
	config_input(BUTTON2_GPIO);
	while(1){
		uint32_t volatile input;
		input = (NRF_GPIO->IN >> BUTTON1_GPIO) & 1UL;
		if(input){
			NRF_GPIO->OUTSET = (1UL << LED1_GPIO);
		}
		else{
			NRF_GPIO->OUTCLR = (1UL << LED1_GPIO);
			boot_from((uint32_t *) 0x00001000);
		}
		input = (NRF_GPIO->IN >> BUTTON2_GPIO) & 1UL;
		if(input){
			NRF_GPIO->OUTSET = (1UL << LED2_GPIO);
		}
		else{
			NRF_GPIO->OUTCLR = (1UL << LED2_GPIO);
			boot_from((uint32_t *) 0x00014000);
		}
		uint32_t volatile tm0 = 1000000;
		while(tm0--);
		NRF_GPIO->OUTSET = (1UL << LED4_GPIO);
		tm0 = 1000000;
		while(tm0--);
		NRF_GPIO->OUTCLR = (1UL << LED4_GPIO);

	
	}
}

extern uint32_t __data_rom_start;
extern uint32_t __data_ram_start;

extern uint32_t __data_ram_end;
extern uint32_t __bss_start;
extern uint32_t __bss_end;



void __start(){
	register uint32_t *src, *dst;
	src = &__data_rom_start;
	dst = &__data_ram_start;
	while(dst < &__data_ram_end){
		*dst++ = *src++;
	}

	dst = &__bss_start;
	while(dst < &__bss_end){
		*dst++ = 0;
	}
	main();
	while(1);
}

