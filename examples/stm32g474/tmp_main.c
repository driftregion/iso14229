#include <stdint.h>
#include <stdio.h>
#include "stm32g4xx.h"

#define LED_PIN GPIO_PIN_5
#define LED_PORT GPIOA

UART_HandleTypeDef uart2;

void clock_init();

int main(void)
{
  HAL_Init();
  clock_init();
  SystemCoreClockUpdate(); // Update the internal clock frequency variable

  // Initialize LED GPIO
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitTypeDef gpio_init = {0};
  gpio_init.Pin = LED_PIN;
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_LOW;
  gpio_init.Alternate = 0;

  HAL_GPIO_Init(LED_PORT, &gpio_init);

  // Initialize UART
  uart2.Instance = USART2;
  uart2.Init.BaudRate = 115200;
  uart2.Init.Mode = UART_MODE_TX;
  uart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  uart2.Init.WordLength = UART_WORDLENGTH_8B;
  uart2.Init.StopBits = UART_STOPBITS_1;
  uart2.Init.Parity = UART_PARITY_NONE;
  uart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&uart2) != HAL_OK)
  {
    while(1);
  }

  while(1)
  {
    HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
    printf("[%.3f] Hello, World!\r\n", HAL_GetTick()/1000.0f);
    HAL_Delay(500);
  }
}

void clock_init()
{
  // Enable power controller and set voltage scale mode 1
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  // Configure PLL dividers and multiplier
  /*
  The external oscillator HSE=24 MHz
  VCO = HSE/PLLM*PLLN 
      = 24/6*85= 340 MHz 
      /2 = 170 MHz 

      but hanging on OscConfig. Why?
  */

  RCC_OscInitTypeDef osc_init = {0};
  osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  osc_init.HSEState = RCC_HSE_ON;

  osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  osc_init.PLL.PLLState = RCC_PLL_ON;
  osc_init.PLL.PLLM = RCC_PLLM_DIV6;
  osc_init.PLL.PLLN = 85;
  osc_init.PLL.PLLP = RCC_PLLP_DIV2;
  osc_init.PLL.PLLQ = RCC_PLLQ_DIV2;
  osc_init.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&osc_init) != HAL_OK)
  {
    while(1);
  }

  /* Set PLL output as the source for the system clock.
   * Since APB1 clock must not be more than 50 MHz, set the PCKL1 divider to 2.
   */
  RCC_ClkInitTypeDef clock_init = {0};
  clock_init.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_HCLK;
  clock_init.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clock_init.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clock_init.APB1CLKDivider = RCC_HCLK_DIV2;
  clock_init.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&clock_init, FLASH_LATENCY_3) != HAL_OK) // Configure flash controller for 3V3 supply and 100 MHz -> 3 wait states
  {
    while(1);
  }
}
