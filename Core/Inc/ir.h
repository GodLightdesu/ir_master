#ifndef IR_H
#define IR_H

#include "led.h"
#include "i2c.h"
#include <stdint.h>
#include <string.h>

// Vref + 7 * sensors, each 2 bytes
#define IR_BUFFER_SIZE 16 // 8 * 2 bytes
#define EYE_NUM 7

typedef enum { SLAVE_1 = 0, SLAVE_2 } Slave_ID;
extern uint8_t ProcessBuffer[2][IR_BUFFER_SIZE];

void IR_Init(I2C_HandleTypeDef *hi2c1, I2C_HandleTypeDef *hi2c2);

HAL_StatusTypeDef IR_ReadData(Slave_ID slaves_id);
uint8_t IR_SaveData(Slave_ID slave_id, uint8_t *data, uint16_t size);
uint8_t IR_IsDataReady(Slave_ID slave_id);
void IR_ClearDataReady(Slave_ID slave_id);

uint16_t combine_data(uint8_t msb, uint8_t lsb);

void IR_ProcessData(Slave_ID slave_id, uint8_t sampling_times);
void arrangeData(Slave_ID slave_id);
float IR_ADC_to_Voltage(uint16_t adc_value, float vref);

#endif