#include "led.h"

void LEDControl(uint8_t num, uint8_t state){
	switch(num){
		case 0: 
			HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
			break;
		case 1: 
			HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
			break;
		case 2: 
			HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
			break;
		case 3: 
			HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
			break;
		default:
			break;
	}
}


/**
 * @brief 根据剩余容量百分比（RSOC）控制四个 LED 的亮灯数量
 * @param rsoc 剩余容量百分比 (0~100)
 */
void SetLedsByRSOC(uint8_t rsoc)
{
    LEDControl(0,rsoc>75?1:0);
    LEDControl(1,rsoc>50?1:0);
    LEDControl(2,rsoc>25?1:0);
    LEDControl(3,rsoc>0?1:0);
}
