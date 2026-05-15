#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "bms.h"

#define BMS_BUFFSIZE 128
extern uint8_t bms_rx_buffer[BMS_BUFFSIZE];
extern uint16_t bms_rx_len;

void USART2_IQR(void);
void BMS_SendData(uint8_t *data, uint16_t len);
HAL_StatusTypeDef FDCAN1_SendData(uint8_t *pData);
uint16_t pack_bms_data(uint8_t* buffer, const BMS_BasicInfo_t* basic, const BMS_CellVoltage_t* cell);

#ifdef __cplusplus
}
#endif
#endif /*__ MESSAGE_H__ */
