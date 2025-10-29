#ifndef IR_H
#define IR_H

#include "led.h"
#include "i2c.h"
#include <stdint.h>
#include <string.h>

#define IR_BUFFER_SIZE 16  // 8 * 2 bytes

typedef enum { SLAVE_1 = 0, SLAVE_2 } Slave_ID;
extern uint8_t ProcessBuffer[2][IR_BUFFER_SIZE];

void IR_Init(I2C_HandleTypeDef *hi2c1, I2C_HandleTypeDef *hi2c2);

void IR_ReadData(Slave_ID slaves_id);
uint8_t IR_SaveData(Slave_ID slave_id, uint8_t *data, uint16_t size);
uint8_t IR_IsDataReady(Slave_ID slave_id);
void IR_ClearDataReady(Slave_ID slave_id);

#endif