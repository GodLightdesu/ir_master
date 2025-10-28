#ifndef LED_H
#define LED_H

#include "gpio.h"
#include <stdint.h>

void LED_On(void);
void LED_Off(void);
void LED_Toggle(void);
void LED_Flash(uint32_t delay_ms, uint8_t times);

#endif  // LED_H