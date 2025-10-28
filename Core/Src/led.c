#include "led.h"

void LED_On(void) { HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); }

void LED_Off(void) { HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET); }

void LED_Toggle(void) { HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); }

void LED_Flash(uint32_t delay_ms, uint8_t times) {
  for (uint8_t i = 0; i < times; i++) {
    LED_On();
    HAL_Delay(delay_ms);
    LED_Off();
    HAL_Delay(delay_ms);
  }
}