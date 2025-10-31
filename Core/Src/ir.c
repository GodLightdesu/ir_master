#include "ir.h"
#include "led.h"

#define SLAVES_NO 2
#define SLAVE_1_ADDR (0x30 << 1)
#define SLAVE_2_ADDR (0x31 << 1)

static I2C_HandleTypeDef *I2C_Handle[SLAVES_NO] = {0};

// 雙緩衝設計
static uint8_t RxBuffer[SLAVES_NO][IR_BUFFER_SIZE] = {0};      // ISR 寫入
uint8_t ProcessBuffer[SLAVES_NO][IR_BUFFER_SIZE] = {0};        // Main 讀取
static volatile uint8_t DataReady[SLAVES_NO] = {0};            // 資料就緒標誌

uint8_t maxEye = 0;
uint16_t maxValue = 0;
static uint16_t eyeValues[SLAVES_NO * EYE_NUM] = {0};

void IR_Init(I2C_HandleTypeDef *hi2c1, I2C_HandleTypeDef *hi2c2) {
  I2C_Handle[SLAVE_1] = hi2c1;
  I2C_Handle[SLAVE_2] = hi2c2;
  
  // 清除所有緩衝區和狀態
  for (int i = 0; i < SLAVES_NO; i++) {
    memset(RxBuffer[i], 0, IR_BUFFER_SIZE);
    memset(ProcessBuffer[i], 0, IR_BUFFER_SIZE);
    DataReady[i] = 0;
  }
}

HAL_StatusTypeDef IR_ReadData(Slave_ID slaves_id) {
  if (I2C_Handle[slaves_id] == NULL) { 
    return HAL_ERROR; 
  }

  // Check if I2C is busy
  if (HAL_I2C_GetState(I2C_Handle[slaves_id]) != HAL_I2C_STATE_READY) {
    return HAL_BUSY;
  }

  uint16_t devAddr = (slaves_id == SLAVE_1) ? SLAVE_1_ADDR : SLAVE_2_ADDR;
  HAL_StatusTypeDef status = HAL_I2C_Master_Receive_DMA(
    I2C_Handle[slaves_id],
    devAddr,
    RxBuffer[slaves_id],
    IR_BUFFER_SIZE
  );
  
  return status;
}

uint8_t IR_SaveData(Slave_ID slave_id, uint8_t *data, uint16_t size) {
  if (DataReady[slave_id]) {
    // 複製資料
    size = (size > IR_BUFFER_SIZE) ? IR_BUFFER_SIZE : size;
    memcpy(data, ProcessBuffer[slave_id], size);

    // 清除標誌
    DataReady[slave_id] = 0;
    return 1;  // 成功
  }
  return 0;  // 無新資料
}

uint8_t IR_IsDataReady(Slave_ID slave_id) { return DataReady[slave_id]; }

void IR_ClearDataReady(Slave_ID slave_id) { DataReady[slave_id] = 0; }

uint16_t combine_data(uint8_t msb, uint8_t lsb) { return (msb << 8) | lsb; }

float IR_ADC_to_Voltage(uint16_t adc_value, float vref) {
  // 假設 12-bit ADC (0-4095)
  return (adc_value * vref) / 4095.0f;
}

/* I2C event callback */
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  for (int sid = 0; sid < SLAVES_NO; sid++) {
    if (hi2c == I2C_Handle[sid]) {
      // 複製到處理緩衝區
      memcpy(ProcessBuffer[sid], RxBuffer[sid], IR_BUFFER_SIZE);
      
      // 設定資料就緒標誌
      DataReady[sid] = 1;
      
      break;
    }
  }
}

void updateValues() {
  // Require both slaves to have new data
  DataReady[SLAVE_2] = 1;
  if (!IR_IsDataReady(SLAVE_1) || !IR_IsDataReady(SLAVE_2)) {
    return; // No new data to process
  }

  // Reset previous result before recomputing
  maxValue = 0;
  maxEye = 0;

  // Extract eye values from both slaves
  for (int sid = 0; sid < SLAVES_NO; sid++) {
    for (int i = 0; i < EYE_NUM; i++) {
      // ProcessBuffer layout: [Vref_LSB,Vref_MSB, eye0_LSB, eye0_MSB, eye1_LSB, eye1_MSB, ...]
      uint8_t lsb = ProcessBuffer[sid][2 + i * 2];
      uint8_t msb = ProcessBuffer[sid][3 + i * 2];
      eyeValues[sid * EYE_NUM + i] = combine_data(msb, lsb);
    }
  }

  // Find max eye value
  for (int i = 0; i < SLAVES_NO * EYE_NUM; i++) {
    if (eyeValues[i] > maxValue) {
      maxValue = eyeValues[i];
      maxEye = i;
    }
  }
}

/* I2C error callback */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  // Handle I2C errors
  for (int sid = 0; sid < SLAVES_NO; sid++) {
    if (hi2c == I2C_Handle[sid]) {
      // Clear error state - the next request will retry
      // You can add error counting or logging here if needed
      break;
    }
  }
}