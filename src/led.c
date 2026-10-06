//
// LED: Handles blinking of status lights
//

#include "stm32g4xx_hal.h"
#include "led.h"
#include "error.h"


// Private variables
static volatile uint32_t led_blue_laston = 0;
static volatile uint32_t led_green_laston = 0;
static uint32_t led_blue_lastoff = 0;
static uint32_t led_green_lastoff = 0;
static uint8_t error_blink_status = 0;
static uint8_t error_was_indicating = 0;
static uint32_t last_errflash = 0;

// Status LEDs off (I0). Both LEDs are lit by writing 1 and dark at 0, so a
// quiet LED is held at 0 whatever the code below asks for. LEDS_QUIET_DEFAULT
// (make LEDS_QUIET=1) sets it at power-up.
#ifndef LEDS_QUIET_DEFAULT
#define LEDS_QUIET_DEFAULT 0
#endif
static uint8_t led_quiet = LEDS_QUIET_DEFAULT;


// Write an LED pin, or hold it dark while the LEDs are quiet
static void led_write(GPIO_TypeDef *port, uint16_t pin, uint8_t value)
{
    HAL_GPIO_WritePin(port, pin, led_quiet ? 0 : value);
}


// Turn the status LEDs off (quiet = 1) or back to normal (quiet = 0)
void led_set_quiet(uint8_t quiet)
{
    led_quiet = quiet;

    // Normal operation shows green as the power light and blue dark
    led_write(LED_GREEN, 1);
    led_write(LED_BLUE, 0);
}


// Initialize LED GPIOs
void led_init()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Pin = LED_BLUE_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = 0;
    HAL_GPIO_Init(LED_BLUE_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LED_GREEN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = 0;
    HAL_GPIO_Init(LED_GREEN_Port, &GPIO_InitStruct);


    led_write(LED_GREEN, 1); 
}


// Turn green LED on
void led_green_on(void)
{
	// Make sure the LED has been off for at least LED_DURATION before turning on again
	// This prevents a solid status LED on a busy canbus
	if(led_green_laston == 0 && HAL_GetTick() - led_green_lastoff > LED_DURATION)
	{
        // Invert LED
		led_write(LED_GREEN, 0);
		led_green_laston = HAL_GetTick();
	}
}


// Turn green LED on
void led_green_off(void)
{
	led_write(LED_GREEN, 0);
}


// Blink blue LED (blocking)
void led_blue_blink(uint8_t numblinks)
{
	uint8_t i;
	for(i=0; i<numblinks; i++)
	{
		led_write(LED_BLUE, 1);
		HAL_Delay(100);
		led_write(LED_BLUE, 0);
		HAL_Delay(100);
	}
}


// Attempt to turn on status LED
void led_blue_on(void)
{
	// Make sure the LED has been off for at least LED_DURATION before turning on again
	// This prevents a solid status LED on a busy canbus
	if(led_blue_laston == 0 && HAL_GetTick() - led_blue_lastoff > LED_DURATION)
	{
		led_write(LED_BLUE, 1);
		led_blue_laston = HAL_GetTick();
	}
}


// Process time-based LED events
void led_process(void)
{

    // If error occurred in the last 2 seconds, override LEDs with blink sequence
    if(error_last_timestamp() > 0 && (HAL_GetTick() - error_last_timestamp() < 2000))
    {
    	if(HAL_GetTick() - last_errflash > 150)
    	{
    		last_errflash = HAL_GetTick();
			led_write(LED_BLUE, error_blink_status);
			led_write(LED_GREEN, error_blink_status);
            error_blink_status = !error_blink_status;
            error_was_indicating = 1;
    	}
    }
    // Otherwise, normal LED operation
    else
    {
        // If we were blinking but no longer are blinking, turn the power LED back on.
        if(error_was_indicating)
        {
            led_write(LED_GREEN, 1);
            error_was_indicating = 0;
        }
        
		// If LED has been on for long enough, turn it off
		if(led_blue_laston > 0 && HAL_GetTick() - led_blue_laston > LED_DURATION)
		{
			led_write(LED_BLUE, 0);
			led_blue_laston = 0;
			led_blue_lastoff = HAL_GetTick();
		}

		// If LED has been on for long enough, turn it off
		if(led_green_laston > 0 && HAL_GetTick() - led_green_laston > LED_DURATION)
		{
			// Invert LED
			led_write(LED_GREEN, 1);
			led_green_laston = 0;
			led_green_lastoff = HAL_GetTick();
		}
    }
}

