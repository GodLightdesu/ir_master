#include "data_uart.h"

static UART_HandleTypeDef *dataUart_huart;

void dataUart_Init(UART_HandleTypeDef *huart) {
  dataUart_huart = huart;
}

// Function to parse and display IR data as decimal values
HAL_StatusTypeDef ParseAndDisplayIRData(uint8_t *data, uint16_t size) {
  if (dataUart_huart == NULL || data == NULL) return HAL_ERROR;
  
  char buffer[128];
  int pos = 0;
  
  // Add prefix
  pos += sprintf(&buffer[pos], "Decimal: ");
  
  // Parse each 2-byte pair (ensure we don't exceed buffer size)
  for (int i = 0; i < size && i+1 < size && pos < 110; i += 2) {
    uint16_t value = (data[i+1] << 8) | data[i];  // Little-endian (LSB first)
    pos += sprintf(&buffer[pos], "%u ", value);
  }
  
  buffer[pos++] = '\r';
  buffer[pos++] = '\n';
  return HAL_UART_Transmit(dataUart_huart, (uint8_t*)buffer, pos, HAL_MAX_DELAY);
}

// Function to display raw hex data
HAL_StatusTypeDef DisplayRawHexData(uint8_t *data, uint16_t size) {
  if (dataUart_huart == NULL || data == NULL) return HAL_ERROR;
  
  char buffer[128];
  int bufferPos = 0;
  
  // Add prefix
  bufferPos += sprintf(&buffer[bufferPos], "Raw: ");
  
  // Ensure we don't exceed buffer size (3 chars per byte + safety margin)
  for (int i = 0; i < size && bufferPos < 115; i++) {
    bufferPos += sprintf(&buffer[bufferPos], "%02x ", data[i]);
  }
  
  buffer[bufferPos++] = '\r';
  buffer[bufferPos++] = '\n';  
  return HAL_UART_Transmit(dataUart_huart, (uint8_t*)buffer, bufferPos, HAL_MAX_DELAY);
}