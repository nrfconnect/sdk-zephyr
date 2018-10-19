#include <stdint.h>
#include <nrf.h>
#include <generated_dts_board.h>

#ifdef CONFIG_SECURE_BOOT_DEBUG
#include <SEGGER_RTT_sb.h>
#endif /* SEGGER_DEBUG */

#define LED1_GPIO (GPIO_LEDS_LED_0_GPIO_PIN)
#define LED2_GPIO (GPIO_LEDS_LED_1_GPIO_PIN)
#define LED3_GPIO (GPIO_LEDS_LED_2_GPIO_PIN)
#define LED4_GPIO (GPIO_LEDS_LED_3_GPIO_PIN)

#define BUTTON1_GPIO (GPIO_KEYS_BUTTON_0_GPIO_PIN)
#define BUTTON2_GPIO (GPIO_KEYS_BUTTON_1_GPIO_PIN)
#define BUTTON3_GPIO (GPIO_KEYS_BUTTON_2_GPIO_PIN)
#define BUTTON4_GPIO (GPIO_KEYS_BUTTON_3_GPIO_PIN)

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

	__disable_irq();

	for(uint8_t i = 0; i < 8; i++) NVIC->ICER[i] = 0xFFFFFFFF;
	for(uint8_t i = 0; i < 8; i++) NVIC->ICPR[i] = 0xFFFFFFFF;

	SysTick->CTRL = 0;

	SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;
	SCB->SHCSR &= ~(SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_MEMFAULTENA_Msk );

	if(CONTROL_SPSEL_Msk & __get_CONTROL())
	{
		__set_CONTROL(__get_CONTROL() & ~CONTROL_SPSEL_Msk);
	}

	__DSB(); /* Force Memory Write before continuing */
	__ISB(); /* Flush and refill pipeline with updated premissions */

	SCB->VTOR = (uint32_t) address;

	__enable_irq();
	__set_MSP(address[0]);
	((void(*)(void))address[1])( ) ;
}

void led_init(void)
{
	config_led(LED1_GPIO);
	config_led(LED2_GPIO);
	config_led(LED3_GPIO);
	config_led(LED4_GPIO);
}

void button_init(void)
{
	config_input(BUTTON1_GPIO);
	config_input(BUTTON2_GPIO);
	config_input(BUTTON3_GPIO);
	config_input(BUTTON4_GPIO);
}

int main(void)
{

	#ifdef CONFIG_SECURE_BOOT_DEBUG
	SEGGER_RTT_Init();
	#endif /* SEGGER_DEBUG */

	led_init();
	button_init();


	/* TODO: Clean up button and led  configurations before jump */

	while(1){
		uint32_t volatile input;

		input = (NRF_GPIO->IN >> BUTTON1_GPIO) & 1UL;
		if(input){
			NRF_GPIO->OUTSET = (1UL << LED1_GPIO);
		}
		else{
			#ifdef CONFIG_SECURE_BOOT_DEBUG
			SEGGER_RTT_printf(0, "Tried to boot from area s0\r\n");
			#endif /* SEGGER_DEBUG */
			NRF_GPIO->OUTCLR = (1UL << LED1_GPIO);
			boot_from((uint32_t *) (0x00000000 + FLASH_AREA_S0_OFFSET));
		}
		input = (NRF_GPIO->IN >> BUTTON2_GPIO) & 1UL;
		if(input){
			NRF_GPIO->OUTSET = (1UL << LED2_GPIO);
		}
		else{

			#ifdef CONFIG_SECURE_BOOT_DEBUG
			SEGGER_RTT_printf(0, "Tried to boot from area s1\r\n");
			#endif /* SEGGER_DEBUG */
			NRF_GPIO->OUTCLR = (1UL << LED2_GPIO);
			boot_from((uint32_t *) (0x00000000 + FLASH_AREA_S1_OFFSET));
		}
		input = (NRF_GPIO->IN >> BUTTON3_GPIO) & 1UL;
		if(input){
			NRF_GPIO->OUTSET = (1UL << LED3_GPIO);
		}
		else{
			NRF_GPIO->OUTCLR = (1UL << LED3_GPIO);
			#ifdef CONFIG_SECURE_BOOT_DEBUG
			SEGGER_RTT_printf(0, "Tried to boot from app area\r\n");
			#endif /* SEGGER_DEBUG */
			boot_from((uint32_t *) (0x00000000 + FLASH_AREA_APP_OFFSET));
		}

		uint32_t volatile tm0 = 1000000;
		while(tm0--);
		NRF_GPIO->OUTSET = (1UL << LED4_GPIO);
		tm0 = 1000000;
		while(tm0--);
		NRF_GPIO->OUTCLR = (1UL << LED4_GPIO);


	}
}

