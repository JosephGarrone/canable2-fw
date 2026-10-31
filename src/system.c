//
// system: initialize system clocks and other core peripherals
//

#include "stm32g4xx_hal.h"
#include "system.h"


// Private functions
static void __option_byte_config(void);


// Initialize system clocks
void system_init(void)
{
    HAL_Init();

    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    /** Configure the main internal regulator output voltage
    */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);
    /** Initializes the CPU, AHB and APB busses clocks
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSI48;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
    RCC_OscInitStruct.PLL.PLLN = 85;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
      while(1);
    }
    /** Initializes the CPU, AHB and APB busses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_8) != HAL_OK)
    {
      while(1);
    }
    /** Initializes the peripherals clocks
    */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB|RCC_PERIPHCLK_FDCAN;
    PeriphClkInit.FdcanClockSelection = RCC_FDCANCLKSOURCE_PCLK1;
    PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      while(1);
    }

    /** Configures CRS
         */
    RCC_CRSInitTypeDef pInit = {0};
	pInit.Prescaler = RCC_CRS_SYNC_DIV1;
	pInit.Source = RCC_CRS_SYNC_SOURCE_USB;
	pInit.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
	pInit.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000,1000);
	pInit.ErrorLimitValue = 34;
	pInit.HSI48CalibrationValue = 32;

	HAL_RCCEx_CRSConfig(&pInit);

    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE(); // just nrst is on portG

    __option_byte_config();
}


// Restarting into the STM32's ROM USB DFU bootloader, for firmware updates
// without the BOOT jumper. The jump is made from here rather than across a
// reset, so nothing is left behind to bring the bootloader back later: every
// peripheral this firmware started is returned to its reset state first.
#define SYSTEM_MEMORY_BASE 0x1FFF0000UL


// Jump to the bootloader. Called with interrupts disabled; does not return.
void system_enter_bootloader(void)
{
    // Detach from USB for a while first, so the host drops the CDC port
    // before the bootloader's DFU device appears
    USB->BCDR &= ~USB_BCDR_DPPU;
    for(volatile uint32_t i = 0; i < 2000000UL; i++);

    // Return USB, FDCAN and the pins to their reset state, then the clocks.
    // With the pins reset the transceiver's standby line floats to its
    // pull-up, so it is off the bus while the bootloader runs, as it is
    // when the bootloader is entered with the BOOT jumper.
    __HAL_RCC_USB_FORCE_RESET();
    __HAL_RCC_USB_RELEASE_RESET();
    __HAL_RCC_FDCAN_FORCE_RESET();
    __HAL_RCC_FDCAN_RELEASE_RESET();
    __HAL_RCC_GPIOA_FORCE_RESET();
    __HAL_RCC_GPIOB_FORCE_RESET();
    __HAL_RCC_GPIOC_FORCE_RESET();
    __HAL_RCC_GPIOA_RELEASE_RESET();
    __HAL_RCC_GPIOB_RELEASE_RESET();
    __HAL_RCC_GPIOC_RELEASE_RESET();
    HAL_RCC_DeInit();

    // HAL_RCC_DeInit restarts SysTick; stop it and every interrupt
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    for(uint32_t i = 0; i < sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]); i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFUL;
        NVIC->ICPR[i] = 0xFFFFFFFFUL;
    }
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;

    // Map system memory at address 0 and start the bootloader from its own
    // vector table, with interrupts enabled as after a reset
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    SYSCFG->MEMRMP = SYSCFG_MEMRMP_MEM_MODE_0;

    uint32_t stack = *(volatile uint32_t *)SYSTEM_MEMORY_BASE;
    uint32_t entry = *(volatile uint32_t *)(SYSTEM_MEMORY_BASE + 4);
    SCB->VTOR = SYSTEM_MEMORY_BASE;
    __DSB();
    __ISB();
    __enable_irq();

    // One block, so nothing uses the old stack once the new one is set
    __asm volatile("msr msp, %0\n\tbx %1" : : "r"(stack), "r"(entry) : "memory");
    __builtin_unreachable();
}


// Disable all interrupts
void system_irq_disable(void)
{
	__disable_irq();
	__DSB();
	__ISB();
}


// Enable all interrupts
void system_irq_enable(void)
{
	__enable_irq();
}


// Configure option bytes: set BoR level to 4 (2.8v)
static void __option_byte_config(void)
{
	// Set BoR level to 4
	// This eliminates an issue where poor quality USB hubs
	// that provide low voltage before switching the 5v supply on
	// which was causing PoR issues where the microcontroller
	// would enter boot mode incorrectly.

	// Get option bytes
	FLASH_OBProgramInitTypeDef config = {0};
	HAL_FLASHEx_OBGetConfig(&config);

	// If BoR is already at level 4, then don't bother doing anything
	if((config.USERConfig & FLASH_OPTR_BOR_LEV_Msk) == OB_BOR_LEVEL_4)
		return;

	// Unlock flash
	if(HAL_FLASH_Unlock() != HAL_OK)
		return;

	// Unlock option bytes
   if(HAL_FLASH_OB_Unlock() != HAL_OK)
	   return;

   FLASH_OBProgramInitTypeDef progval = {0};

   // Update the user option byte
   progval.OptionType = OPTIONBYTE_USER;
   progval.USERType = OB_USER_BOR_LEV;
   progval.USERConfig = OB_BOR_LEVEL_4; // 2.8v for level 4

   // Program the option bytes
   HAL_FLASHEx_OBProgram(&progval);

   // Lock flash / option bytes
   HAL_FLASH_OB_Lock();
   HAL_FLASH_Lock();

   // Note: option byte update will take effect on the next power cycle
}


// Convert a 32-bit value to an ascii hex value
void system_hex32(char *out, uint32_t val)
{
	char *p = out + 8;
	*p-- = 0;
	while (p >= out) {
		uint8_t nybble = val & 0x0F;
		if (nybble < 10)
			*p = '0' + nybble;
		else
			*p = 'A' + nybble - 10;
		val >>= 4;
		p--;
	}
} 
