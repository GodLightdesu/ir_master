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

void IR_ReadData(Slave_ID slaves_id) {
  if (I2C_Handle[slaves_id] == NULL) { return; }

  uint16_t devAddr = (slaves_id == SLAVE_1) ? SLAVE_1_ADDR : SLAVE_2_ADDR;
  HAL_I2C_Master_Receive_IT(
    I2C_Handle[slaves_id],
    devAddr,
    RxBuffer[slaves_id],
    IR_BUFFER_SIZE
  );
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