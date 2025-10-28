#ifndef IR_H
#define IR_H

#include "led.h"
#include "i2c.h"
#include <stdint.h>
#include <string.h>

typedef enum { SLAVE_1 = 0, SLAVE_2 } Slave_ID;

void IR_Init(I2C_HandleTypeDef *hi2c1, I2C_HandleTypeDef *hi2c2);

void IR_ReadData(Slave_ID slaves_id);
uint8_t IR_GetData(Slave_ID slave_id, uint8_t *data, uint16_t size);

#endif