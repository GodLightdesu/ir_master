#include "ir.h"
#include "led.h"

#define SLAVES_NO 2
#define SLAVE_1_ADDR (0x30 << 1)
#define SLAVE_2_ADDR (0x31 << 1)
#define MAX_SAMPLES 10  // Maximum sampling buffer

static I2C_HandleTypeDef *I2C_Handle[SLAVES_NO] = {0};

// 雙緩衝設計
static uint8_t RxBuffer[SLAVES_NO][IR_BUFFER_SIZE] = {0};      // ISR 寫入
uint8_t ProcessBuffer[SLAVES_NO][IR_BUFFER_SIZE] = {0};        // Main 讀取
static volatile uint8_t DataReady[SLAVES_NO] = {0};            // 資料就緒標誌

// Multi-sampling buffers
static uint16_t SampleBuffer[SLAVES_NO][MAX_SAMPLES][EYE_NUM + 1] = {0};
static uint8_t SampleIndex[SLAVES_NO] = {0};
static uint8_t SampleCount[SLAVES_NO] = {0};

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

void IR_ProcessData(Slave_ID slave_id, uint8_t sampling_times) {
  // Limit sampling times to buffer size
  if (sampling_times > MAX_SAMPLES) sampling_times = MAX_SAMPLES;
  if (sampling_times == 0) return;
  
  uint8_t idx = SampleIndex[slave_id];
  
  // Store current sample (skip Vref at bytes 0-1)
  for (int i = 0; i < EYE_NUM; i++) {
    int pos = (i + 1) * 2;
    SampleBuffer[slave_id][idx][i] = 
      (ProcessBuffer[slave_id][pos + 1] << 8) | ProcessBuffer[slave_id][pos];
  }
  
  // Update index and wait for enough samples
  SampleIndex[slave_id] = (idx + 1) % sampling_times;
  if (SampleCount[slave_id] < sampling_times) {
    SampleCount[slave_id]++;
    return;
  }
  
  // Calculate average and find global min in one pass
  uint16_t avg[EYE_NUM];
  uint16_t global_min = 0xFFFF;

  // Calculate averages of each sensor
  for (int sensor = 0; sensor < EYE_NUM; sensor++) {
    uint32_t sum = 0;
    for (int sample = 0; sample < sampling_times; sample++) {
      sum += SampleBuffer[slave_id][sample][sensor];
    }
    avg[sensor] = sum / sampling_times;
    if (avg[sensor] < global_min) global_min = avg[sensor];
  }
  
  // Subtract ambient light and write back (preserve Vref)
  for (int i = 0; i < EYE_NUM; i++) {
    uint16_t val = (avg[i] > global_min) ? (avg[i] - global_min) : 0;
    // Preserve Vref
    int pos = (i + 1) * 2;
    // Store the processed value
    ProcessBuffer[slave_id][pos] = val & 0xFF;
    ProcessBuffer[slave_id][pos + 1] = val >> 8;
  }
}

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