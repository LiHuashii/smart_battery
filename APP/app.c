#include "app.h"
#include <string.h>
#include "cmsis_os.h"
#include "main.h"
#include "fh_log.h"
#include "usart.h"
#include "switch.h"
#include "message.h"
#include "led.h"
#include "bms.h"

uint8_t bms_tx_buffer[32];
uint16_t bms_tx_len;

extern osSemaphoreId_t BMS_RXHandle;
extern osSemaphoreId_t PWR_KEYHandle;
extern LEDShowTypeDef led_show_type;
extern PowerModeTypedef power_mode;
extern BMS_BasicInfo_t bms_basicinfo;
extern BMS_CellVoltage_t bms_cellvoltage;
extern osMessageQueueId_t bms_send_bufHandle;
extern osThreadId_t SensorReadHandle;

void uart_send_str(const char *str) {
    uint16_t len = strlen(str);
    if (len > 0) {
        HAL_UART_Transmit(&huart1, (uint8_t *)str, len, HAL_MAX_DELAY);
    }
}

uint32_t log_time(void) {
    return HAL_GetTick();
}

void app_init(void)
{
    // Initialize application components here
	HAL_Delay(100);

	log_init(uart_send_str, log_time);
  	LOGI("The log system has been initialized successfully. Now, the user program will be initialized.");
	__HAL_UART_ENABLE_IT(&huart2,UART_IT_IDLE);
	__HAL_UART_ENABLE_IT(&huart2,UART_IT_TC);
	HAL_UART_Receive_DMA(&huart2, bms_rx_buffer, sizeof(bms_rx_buffer)-1);
	PowerOn();
}

void PowerSwitchFunction(void)
{
    // Code to power on the device
	// __HAL_UART_ENABLE_IT(&huart2,UART_IT_IDLE);
	// __HAL_UART_ENABLE_IT(&huart2,UART_IT_TC);
	// HAL_UART_Receive_DMA(&huart2, bms_rx_buffer, sizeof(bms_rx_buffer)-1);
	if(power_mode == POWER_OUT){
		for(uint8_t j = 0;j < 3;j++){
			LOGI("Sending discharge command %d", j+1);
			BMS_MakeControlDischargeMosCmd(true, bms_tx_buffer, &bms_tx_len);
			BMS_SendData(bms_tx_buffer, bms_tx_len);
			HAL_Delay(50);
		}
	}else if(power_mode == POWER_IN){
		osDelay(1000);
		for(uint8_t i = 0;i < 3;i++){
			LOGI("Sending charge command %d", i+1);
			BMS_MakeControlChargeMosCmd(true, bms_tx_buffer, &bms_tx_len);
			BMS_SendData(bms_tx_buffer, bms_tx_len);
			HAL_Delay(50);
		}
	}
	
    for(;;)
    {
		if(osSemaphoreAcquire(PWR_KEYHandle, osWaitForever) == osOK){//等待关机键按下
			PowerOff();
		}
    }
}

uint8_t close = 0;
void SensorReadFunction(void)
{
    // Code to read sensor data
	osDelay(2000);
	
    for(;;)
    {
		BMS_MakeReadBasicInfoCmd(bms_tx_buffer, &bms_tx_len);
		BMS_SendData(bms_tx_buffer, bms_tx_len);
		osDelay(500);

		BMS_MakeReadCellVoltageCmd(bms_tx_buffer, &bms_tx_len);
		BMS_SendData(bms_tx_buffer, bms_tx_len);
		osDelay(500);

		// BMS_MakeControlChargeMosCmd(true, bms_tx_buffer, &bms_tx_len);
		// 	BMS_SendData(bms_tx_buffer, bms_tx_len);
		// 	osDelay(500);
    }
}

void DataReportFunction(void)
{
    // Code to report data
	osDelay(3000);
	uint8_t can_tx_buffer[65];
    for(;;)
    {
		pack_bms_data(can_tx_buffer, &bms_basicinfo, &bms_cellvoltage);
		// LOGI("%d %d", len, FDCAN1_SendData(can_tx_buffer, len));
		FDCAN1_SendData(can_tx_buffer);
        osDelay(300);
    }
}

void IOUT_CHECKFunction(void)
{
	// Code to check input/output
	osDelay(2000);
	for(;;)
	{
		if(power_mode == POWER_OUT){
			//放电模式下检测放电接口是否异常拔出
			if(HAL_GPIO_ReadPin(OUT_Check_GPIO_Port,OUT_Check_Pin) == GPIO_PIN_SET){//检测到放电拔出
				osDelay(100);//等待100ms，防止抖动误判
				if(HAL_GPIO_ReadPin(OUT_Check_GPIO_Port,OUT_Check_Pin) == GPIO_PIN_SET){
					//立即关闭放电
					osThreadSuspend(SensorReadHandle);
					osDelay(50);
					BMS_MakeControlDischargeMosCmd(false, bms_tx_buffer, &bms_tx_len);
					BMS_SendData(bms_tx_buffer, bms_tx_len);
					osDelay(50);
					BMS_MakeControlChargeMosCmd(false, bms_tx_buffer, &bms_tx_len);
					BMS_SendData(bms_tx_buffer, bms_tx_len);
					osDelay(200);
					HAL_GPIO_WritePin(PWRCTRL_GPIO_Port, PWRCTRL_Pin, GPIO_PIN_RESET);//关闭控制板电源
				}
				
			}
		}else if(power_mode == POWER_IN){
			//充电模式下检测充电接口是否异常拔出
			if(HAL_GPIO_ReadPin(IN_Check_GPIO_Port,IN_Check_Pin) == GPIO_PIN_SET){//检测到充电拔出
				osDelay(100);//等待100ms，防止抖动误判
				if(HAL_GPIO_ReadPin(IN_Check_GPIO_Port,IN_Check_Pin) == GPIO_PIN_SET){
					//立即关闭充电
					osThreadSuspend(SensorReadHandle);
					osDelay(50);
					BMS_MakeControlChargeMosCmd(false, bms_tx_buffer, &bms_tx_len);
					BMS_SendData(bms_tx_buffer, bms_tx_len);
					osDelay(50);
					BMS_MakeControlDischargeMosCmd(false, bms_tx_buffer, &bms_tx_len);
					BMS_SendData(bms_tx_buffer, bms_tx_len);
					osDelay(200);
					HAL_GPIO_WritePin(PWRCTRL_GPIO_Port, PWRCTRL_Pin, GPIO_PIN_RESET);//关闭控制板电源
				}
				
			}
		}

		osDelay(100);
	}
}

void LEDFunction(void){
	// Code to control LEDs
    for(;;)
    {
		if(led_show_type == LED_IN){
			//充电指示：闪烁显示当前电量（每500ms闪烁一次，闪烁次数表示当前电量档位，最多4档）
			SetLedsByRSOC(bms_basicinfo.rsoc);
		} else if(led_show_type == LED_OUT){
			//放电指示：显示当前电量
			SetLedsByRSOC(bms_basicinfo.rsoc);
		}
        osDelay(1);
    }
}