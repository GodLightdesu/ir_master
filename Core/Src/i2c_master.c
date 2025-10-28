#include "i2c_master.h"

// 掃描 I2C 区域尋找可用設備
uint16_t I2C_Scan(I2C_HandleTypeDef *hi2c) {
  uint16_t devAddr = 0;
  for (uint16_t addr = 1; addr < 128; addr++) {
    if (HAL_I2C_IsDeviceReady(hi2c, addr << 1, 3, 100) == HAL_OK) {
      devAddr = addr;
      break;
    }
  }
  return devAddr;
}