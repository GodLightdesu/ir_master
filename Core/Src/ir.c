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
  // ==================== Step 1: Validate Parameters ====================
  if (sampling_times > MAX_SAMPLES) {
    sampling_times = MAX_SAMPLES;  // Limit to buffer size
  }
  if (sampling_times == 0) {
    return;  // Nothing to process
  }
  
  // ==================== Step 2: Store Current Sample ====================
  uint8_t current_idx = SampleIndex[slave_id];
  uint8_t *pProcessBuf = ProcessBuffer[slave_id];
  
  // Copy 7 sensor values from ProcessBuffer to SampleBuffer (skip Vref at bytes 0-1)
  for (int sensor = 0; sensor < EYE_NUM; sensor++) {
    int byte_pos = (sensor + 1) * 2;  // Skip Vref (bytes 0-1), each sensor is 2 bytes
    uint8_t lsb = pProcessBuf[byte_pos];
    uint8_t msb = pProcessBuf[byte_pos + 1];
    SampleBuffer[slave_id][current_idx][sensor] = (msb << 8) | lsb;  // Combine to 16-bit
  }
  
  // ==================== Step 3: Check Sample Collection Status ====================
  uint8_t *pSampleCount = &SampleCount[slave_id];
  
  if (*pSampleCount < sampling_times) {
    // Not enough samples yet - accumulate and wait
    (*pSampleCount)++;
    SampleIndex[slave_id] = (current_idx + 1) % sampling_times;  // Move to next position
    return;  // Exit early - no processing yet
  }
  
  // ==================== Step 4: Find Global Minimum (Ambient Light) ====================
  uint16_t global_min = 0xFFFF;  // Start with max value
  
  // Scan all samples of all sensors to find the minimum value (ambient light baseline)
  for (int sensor = 0; sensor < EYE_NUM; sensor++) {
    for (int sample = 0; sample < sampling_times; sample++) {
      uint16_t value = SampleBuffer[slave_id][sample][sensor];
      if (value < global_min) {
        global_min = value;
      }
    }
  }
  
  // ==================== Step 5: Process Latest Data & Write Back ====================
  // Use current_idx because that's where we just wrote the latest sample
  for (int sensor = 0; sensor < EYE_NUM; sensor++) {
    // Get latest reading for this sensor
    uint16_t latest_value = SampleBuffer[slave_id][current_idx][sensor];
    
    // Remove ambient light (subtract global minimum)
    uint16_t processed_value = (latest_value > global_min) ? (latest_value - global_min) : 0;
    
    // Write back to ProcessBuffer in little-endian format
    int byte_pos = (sensor + 1) * 2;  // Preserve Vref at bytes 0-1
    pProcessBuf[byte_pos]     = processed_value & 0xFF;        // LSB
    pProcessBuf[byte_pos + 1] = (processed_value >> 8) & 0xFF; // MSB
  }
  arrangeData(slave_id);
  
  // ==================== Step 6: Update Index for Next Write ====================
  SampleIndex[slave_id] = (current_idx + 1) % sampling_times;  // Circular buffer
}

/*
eye: 1, 2, 3, 4, 5, 6, 7
buf: 1, 2, 4, 5, 3, 6, 7
*/
void arrangeData(Slave_ID slave_id) {
  uint8_t *pProcessBuf = ProcessBuffer[slave_id];
  uint8_t arrangedBuf[IR_BUFFER_SIZE] = {0};

  // Copy Vref (bytes 0-1)
  arrangedBuf[0] = pProcessBuf[0];
  arrangedBuf[1] = pProcessBuf[1];

  // Rearranging according to the specified order
  for (int i = 0; i < EYE_NUM; i++) {
    int srcIndex;
    switch (i) {
      case 0: srcIndex = 1; break; // eye 1
      case 1: srcIndex = 2; break; // eye 2
      case 2: srcIndex = 4; break; // eye 3
      case 3: srcIndex = 5; break; // eye 4
      case 4: srcIndex = 3; break; // eye 5
      case 5: srcIndex = 6; break; // eye 6
      case 6: srcIndex = 7; break; // eye 7
      default: srcIndex = i + 1; break;
    }
    // Each sensor data is 2 bytes
    arrangedBuf[(i + 1) * 2]     = pProcessBuf[srcIndex * 2];     // LSB
    arrangedBuf[(i + 1) * 2 + 1] = pProcessBuf[srcIndex * 2 + 1]; // MSB
  }

  // Copy back to ProcessBuffer
  memcpy(pProcessBuf, arrangedBuf, IR_BUFFER_SIZE);
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